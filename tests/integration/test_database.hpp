#pragma once

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>

#include <fmt/format.h>
#include <libpq-fe.h>

#include "database/database.hpp"

// Loads DB test configuration from a .env-like file and/or OS env vars.
// Priority: OS env > file > default. Password is required.
class TestDatabase final {
	struct TransparentHash {
		using is_transparent = void;
		size_t operator()(std::string_view s) const noexcept {
			return std::hash<std::string_view> {}(s);
		}
	};
	struct TestEnvError : public std::runtime_error {
		using std::runtime_error::runtime_error;
	};
	using EnvMap = std::unordered_map<std::string, std::string, TransparentHash, std::equal_to<>>;

	struct DbConfig {
		std::string host;
		std::string user;
		std::string pass;
		std::string database;
		uint32_t port;
		std::string sock;
		std::string schemaPath;
		std::string clientPath;
		bool allowReset;
	};

	static EnvMap loadEnvFile(const std::string &path) {
		EnvMap map;
		std::ifstream in(path);
		if (!in) {
			return map;
		}

		std::string line;
		while (std::getline(in, line)) {
			if (line.empty() || line[0] == '#') {
				continue;
			}
			const auto pos = line.find('=');
			if (pos == std::string::npos) {
				continue;
			}

			auto key = line.substr(0, pos);
			auto val = line.substr(pos + 1);

			// Strip surrounding quotes if present: KEY="value"
			if (!val.empty() && val.size() >= 2 && val.front() == '"' && val.back() == '"') {
				val = val.substr(1, val.size() - 2);
			}
			map[std::move(key)] = std::move(val);
		}
		return map;
	}

	static std::string pickEnvPath() {
		if (const char* p = std::getenv("TEST_ENV_FILE"); p && *p) {
			return p;
		}

#ifdef TESTS_ENV_DEFAULT
		if (std::filesystem::exists(TESTS_ENV_DEFAULT)) {
			return std::string { TESTS_ENV_DEFAULT };
		}
#endif

		namespace fs = std::filesystem;
		for (const char* cand : { "tests/.env", "../tests/.env", "../../tests/.env" }) {
			if (fs::exists(cand)) {
				return cand;
			}
		}

		throw TestEnvError("Test .env file not found. Set TEST_ENV_FILE or define TESTS_ENV_DEFAULT.");
	}

	static std::string get(const EnvMap &m, const char* key, const char* def = nullptr, bool required = false) {
		if (const char* v = std::getenv(key); v && *v) {
			return std::string { v };
		}
		if (auto it = m.find(std::string_view { key }); it != m.end() && !it->second.empty()) {
			return it->second;
		}
		if (required) {
			throw TestEnvError(std::string("Missing required key: ") + key);
		}

		return def ? std::string { def } : std::string {};
	}

	static bool getBool(const EnvMap &m, const char* key, bool def = false) {
		auto value = get(m, key, def ? "1" : "0");
		std::ranges::transform(value, value.begin(), [](unsigned char c) {
			return static_cast<char>(std::tolower(c));
		});
		return value == "1" || value == "true" || value == "yes" || value == "on";
	}

	static std::string shellQuote(const std::string &value) {
		std::string quoted = "\"";
		for (const auto ch : value) {
			if (ch == '"' || ch == '\\' || ch == '$' || ch == '`') {
				quoted += '\\';
				quoted += ch;
			} else {
				quoted += ch;
			}
		}
		quoted += '"';
		return quoted;
	}

	static std::string resolvePostgreSQLClient(const EnvMap &env) {
		if (auto configured = get(env, "TEST_DB_CLIENT", ""); !configured.empty()) {
			return configured;
		}

		const auto pathValue = std::getenv("PATH") ? std::string { std::getenv("PATH") } : std::string {};
		for (const auto name : { "psql" }) {
			std::string::size_type begin = 0;
			while (begin <= pathValue.size()) {
				const auto end = pathValue.find(':', begin);
				const auto directory = pathValue.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
				if (!directory.empty()) {
					const auto candidate = std::filesystem::path(directory) / name;
					if (std::filesystem::exists(candidate)) {
						return candidate.string();
					}
				}
				if (end == std::string::npos) {
					break;
				}
				begin = end + 1;
			}
		}

		return "psql";
	}

	static bool isSafeTestDatabaseName(std::string_view database) {
		if (database.empty()) {
			return false;
		}
		if (database.find("test") == std::string_view::npos && database.find("otserver") == std::string_view::npos) {
			return false;
		}

		return std::ranges::all_of(database, [](unsigned char c) {
			return std::isalnum(c) || c == '_' || c == '-';
		});
	}

	static PGconn* connectServer(const DbConfig &config, const std::string &database) {
		const auto port = std::to_string(config.port);
		const char* keywords[] = { "host", "port", "user", "password", "dbname", nullptr };
		const char* values[] = { config.sock.empty() ? config.host.c_str() : config.sock.c_str(), port.c_str(), config.user.c_str(), config.pass.c_str(), database.c_str(), nullptr };
		auto* handle = PQconnectdbParams(keywords, values, 0);
		if (!handle || PQstatus(handle) != CONNECTION_OK) {
			const std::string error = handle ? PQerrorMessage(handle) : "unable to allocate connection";
			if (handle) {
				PQfinish(handle);
			}
			throw TestEnvError("Failed to connect to PostgreSQL for schema validation: " + error);
		}
		return handle;
	}

	static bool queryHasRows(PGconn* handle, const std::string &query) {
		PGresult* result = PQexec(handle, query.c_str());
		if (!result || PQresultStatus(result) != PGRES_TUPLES_OK) {
			const std::string error = PQerrorMessage(handle);
			PQclear(result);
			throw TestEnvError("Failed to validate test database schema: " + error);
		}
		const auto hasRows = PQntuples(result) > 0;
		PQclear(result);
		return hasRows;
	}

	static bool schemaNeedsReset(const DbConfig &config) {
		std::unique_ptr<PGconn, decltype(&PQfinish)> server(connectServer(config, "postgres"), PQfinish);
		char* escapedDatabase = PQescapeLiteral(server.get(), config.database.data(), config.database.size());
		if (!escapedDatabase) {
			throw TestEnvError("Failed to escape test database name.");
		}
		const auto databaseExists = queryHasRows(server.get(), fmt::format("SELECT 1 FROM pg_database WHERE datname = {}", escapedDatabase));
		PQfreemem(escapedDatabase);
		if (!databaseExists) {
			return true;
		}

		std::unique_ptr<PGconn, decltype(&PQfinish)> database(connectServer(config, config.database), PQfinish);
		const auto hasPlayerComment = queryHasRows(database.get(), "SELECT 1 FROM information_schema.columns WHERE table_schema = 'public' AND table_name = 'players' AND column_name = 'comment' AND data_type = 'character varying' AND is_nullable = 'NO'");

		return !hasPlayerComment;
	}

	static std::filesystem::path writePostgreSQLServiceFile(const DbConfig &config) {
		auto path = std::filesystem::current_path() / fmt::format(".canary-test-postgresql-{}.conf", std::chrono::steady_clock::now().time_since_epoch().count());
		std::ofstream file(path);
		if (!file) {
			throw TestEnvError("Failed to create temporary PostgreSQL service file.");
		}

		file << "[canary_test]\n";
		file << "host=" << (config.sock.empty() ? config.host : config.sock) << "\n";
		file << "user=" << config.user << "\n";
		file << "password=" << config.pass << "\n";
		file << "port=" << config.port << "\n";
		file.close();
		std::filesystem::permissions(
			path,
			std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
			std::filesystem::perm_options::replace
		);
		return path;
	}

	static void runCommand(const std::string &command) {
		const auto result = std::system(command.c_str());
		if (result != 0) {
			throw TestEnvError("Command failed while resetting test database: " + command);
		}
	}

	static void resetDatabase(const DbConfig &config) {
		if (!config.allowReset) {
			throw TestEnvError("Test database schema differs from schema.sql. Set TEST_DB_ALLOW_RESET=1 to allow automatic reset of a disposable test database.");
		}
		if (!isSafeTestDatabaseName(config.database)) {
			throw TestEnvError("Refusing to reset database '" + config.database + "'. Test database names must contain 'test' or be an otserver test database and use only [A-Za-z0-9_-].");
		}
		if (!std::filesystem::exists(config.schemaPath)) {
			throw TestEnvError("schema.sql not found for test database reset: " + config.schemaPath);
		}

		std::filesystem::path serviceFile;
		try {
			serviceFile = writePostgreSQLServiceFile(config);
			const auto prefix = fmt::format("PGSERVICEFILE={} PGSERVICE=canary_test {} -v ON_ERROR_STOP=1", shellQuote(serviceFile.string()), shellQuote(config.clientPath));
			runCommand(fmt::format("{} --dbname=postgres --command={}", prefix, shellQuote(fmt::format("DROP DATABASE IF EXISTS \"{}\" WITH (FORCE)", config.database))));
			runCommand(fmt::format("{} --dbname=postgres --command={}", prefix, shellQuote(fmt::format("CREATE DATABASE \"{}\"", config.database))));
			runCommand(fmt::format("{} --dbname={} --file={}", prefix, shellQuote(config.database), shellQuote(config.schemaPath)));
		} catch (...) {
			if (!serviceFile.empty()) {
				std::filesystem::remove(serviceFile);
			}
			throw;
		}

		std::filesystem::remove(serviceFile);
	}

public:
	static void init() {
		static std::once_flag initOnce;
		std::call_once(initOnce, [] {
			const auto envPath = pickEnvPath();
			const auto env = loadEnvFile(envPath);

			std::string host = get(env, "TEST_DB_HOST", "127.0.0.1");
			std::string user = get(env, "TEST_DB_USER", "root");
			std::string pass = get(env, "TEST_DB_PASSWORD", nullptr, /*required=*/true);
			std::string database = get(env, "TEST_DB_NAME", "canary_test");
			std::string portStr = get(env, "TEST_DB_PORT", "5432");
			auto port = static_cast<uint32_t>(std::strtoul(portStr.c_str(), nullptr, 10));
			std::string sock = get(env, "TEST_DB_SOCKET", "");
			std::string schemaPath = get(env, "TEST_DB_SCHEMA", nullptr);
#ifdef TESTS_SCHEMA_DEFAULT
			if (schemaPath.empty()) {
				schemaPath = TESTS_SCHEMA_DEFAULT;
			}
#endif

			const DbConfig config {
				.host = host,
				.user = user,
				.pass = pass,
				.database = database,
				.port = port,
				.sock = sock,
				.schemaPath = schemaPath,
				.clientPath = resolvePostgreSQLClient(env),
				.allowReset = getBool(env, "TEST_DB_ALLOW_RESET", false),
			};

			int schemaRetries = 30;
			while (schemaRetries > 0) {
				try {
					if (schemaNeedsReset(config)) {
						resetDatabase(config);
					}
					break;
				} catch (const TestEnvError &error) {
					schemaRetries--;
					if (schemaRetries <= 0) {
						throw;
					}
					g_logger().warn("Failed to validate or reset test database schema: {}. Retrying in 1s... ({} attempts remaining)", error.what(), schemaRetries);
					std::this_thread::sleep_for(std::chrono::seconds(1));
				}
			}

			int retries = 30;
			while (retries > 0) {
				if (g_database().connect(&host, &user, &pass, &database, port, &sock)) {
					if (g_database().executeQuery("SELECT 1")) {
						return;
					}
					g_logger().warn("Database connected but failed verification (SELECT 1). Retrying...");
				}
				g_logger().warn("Failed to connect to database. Retrying in 1s... ({} attempts remaining)", retries);
				std::this_thread::sleep_for(std::chrono::seconds(1));
				retries--;
			}
			throw std::runtime_error("Failed to connect to database after multiple attempts.");
		});
	}
};
