/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "database/database.hpp"

#include "config/configmanager.hpp"
#include "lib/di/container.hpp"
#include "lib/metrics/metrics.hpp"
#include "utils/tools.hpp"

#include <iterator>

#ifndef USE_PRECOMPILED_HEADERS
	#include <cassert>
	#include <fmt/format.h>
	#include <string_view>
#endif

namespace {
	// shortcut: legacy queries use standard single-quoted literals; replace this when dollar-quoted SQL is introduced.
	std::string normalizePostgreSQLQuery(std::string_view query) {
		std::string normalized(query);
		bool inString = false;
		for (size_t i = 0; i < normalized.size(); ++i) {
			if (normalized[i] == '\'' && inString && i + 1 < normalized.size() && normalized[i + 1] == '\'') {
				++i;
				continue;
			}
			if (normalized[i] == '\'') {
				inString = !inString;
			} else if (normalized[i] == '`' && !inString) {
				normalized[i] = '"';
			}
		}
		return normalized;
	}

#ifndef NDEBUG
	[[maybe_unused]] const bool queryNormalizerValidated = [] {
		assert(normalizePostgreSQLQuery("SELECT `id`, 'keep `literal` and it''s quoted'") == "SELECT \"id\", 'keep `literal` and it''s quoted'");
		return true;
	}();
#endif

	bool isSuccessfulResult(const PGresult* result) {
		if (!result) {
			return false;
		}
		const auto status = PQresultStatus(result);
		return status == PGRES_COMMAND_OK || status == PGRES_TUPLES_OK;
	}

	void appendInsertBaseQuery(std::string &sql, std::string_view baseQuery, bool baseHasSpace) {
		sql += baseQuery;
		if (!baseHasSpace) {
			sql.push_back(' ');
		}
	}

}

Database::~Database() {
	if (handle != nullptr) {
		PQfinish(handle);
	}
}

Database &Database::getInstance() {
	return inject<Database>();
}

bool Database::connect() {
	return connect(&g_configManager().getString(POSTGRES_HOST), &g_configManager().getString(POSTGRES_USER), &g_configManager().getString(POSTGRES_PASS), &g_configManager().getString(POSTGRES_DB), g_configManager().getNumber(POSTGRES_PORT), &g_configManager().getString(POSTGRES_SOCK));
}

bool Database::connect(const std::string* host, const std::string* user, const std::string* password, const std::string* database, uint32_t port, const std::string* sock) {
	if (host->empty() || user->empty() || database->empty() || port == 0) {
		g_logger().warn("PostgreSQL host, user, database or port not provided");
	}

	const auto portString = std::to_string(port);
	const char* keywords[] = { "host", "port", "user", "password", "dbname", nullptr };
	const char* values[] = { sock->empty() ? host->c_str() : sock->c_str(), portString.c_str(), user->c_str(), password->c_str(), database->c_str(), nullptr };
	if (handle) {
		PQfinish(handle);
		handle = nullptr;
	}
	handle = PQconnectdbParams(keywords, values, 0);
	if (!handle || PQstatus(handle) != CONNECTION_OK) {
		g_logger().error("PostgreSQL connection failed: {}", handle ? PQerrorMessage(handle) : "unable to allocate connection");
		return false;
	}
	if (PQsetClientEncoding(handle, "UTF8") != 0) {
		g_logger().error("Failed to set PostgreSQL client encoding: {}", PQerrorMessage(handle));
		return false;
	}
	return true;
}

void Database::createDatabaseBackup(bool compress) const {
	if (!g_configManager().getBoolean(POSTGRES_DB_BACKUP)) {
		return;
	}

	// Get current time for formatting
	auto now = std::chrono::system_clock::now();
	std::string formattedDate = fmt::format("{:%Y-%m-%d}", now);
	std::string formattedTime = fmt::format("{:%H-%M-%S}", now);

	// Create a backup directory based on the current date
	std::string backupDir = fmt::format("database_backup/{}/", formattedDate);
	std::filesystem::create_directories(backupDir);
	std::string backupFileName = fmt::format("{}backup_{}.sql", backupDir, formattedTime);

	// Keep credentials out of the command line.
	std::string tempConfigFile = "database_backup.conf";
	std::ofstream configFile(tempConfigFile);
	if (configFile.is_open()) {
		configFile << "[canary_backup]\n";
		configFile << "host=" << g_configManager().getString(POSTGRES_HOST) << "\n";
		configFile << "port=" << g_configManager().getNumber(POSTGRES_PORT) << "\n";
		configFile << "user=" << g_configManager().getString(POSTGRES_USER) << "\n";
		configFile << "password=" << g_configManager().getString(POSTGRES_PASS) << "\n";
		configFile << "dbname=" << g_configManager().getString(POSTGRES_DB) << "\n";
		configFile.close();
		std::filesystem::permissions(tempConfigFile, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write, std::filesystem::perm_options::replace);
	} else {
		g_logger().error("Failed to create temporary PostgreSQL service file.");
		return;
	}

	// Execute pg_dump using the fixed service and output paths.
	std::string command = fmt::format(
		"PGSERVICEFILE={} pg_dump --file={} service=canary_backup",
		tempConfigFile, backupFileName
	);

	int result = std::system(command.c_str());
	std::filesystem::remove(tempConfigFile);

	if (result != 0) {
		g_logger().error("Failed to create database backup using pg_dump.");
		return;
	}

	// Compress the backup file if requested
	std::string compressedFileName;
	compressedFileName = backupFileName + ".gz";
	gzFile gzFile = gzopen(compressedFileName.c_str(), "wb9");
	if (!gzFile) {
		g_logger().error("Failed to open gzip file for compression.");
		return;
	}

	std::ifstream backupFile(backupFileName, std::ios::binary);
	if (!backupFile.is_open()) {
		g_logger().error("Failed to open backup file for compression: {}", backupFileName);
		gzclose(gzFile);
		return;
	}

	std::string buffer(8192, '\0');
	while (backupFile.read(&buffer[0], buffer.size()) || backupFile.gcount() > 0) {
		gzwrite(gzFile, buffer.data(), backupFile.gcount());
	}

	backupFile.close();
	gzclose(gzFile);
	std::filesystem::remove(backupFileName);

	g_logger().info("Database backup successfully compressed to: {}", compressedFileName);

	// Delete backups older than 7 days
	auto nowTime = std::chrono::system_clock::now();
	auto sevenDaysAgo = nowTime - std::chrono::hours(7 * 24); // 7 days in hours
	for (const auto &entry : std::filesystem::directory_iterator("database_backup")) {
		if (entry.is_directory()) {
			try {
				for (const auto &file : std::filesystem::directory_iterator(entry)) {
					if (file.path().extension() == ".gz") {
						auto fileTime = std::filesystem::last_write_time(file);
						auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(fileTime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
						auto fileTimeSystemClock = std::chrono::system_clock::time_point { sctp.time_since_epoch() };

						if (fileTimeSystemClock < sevenDaysAgo) {
							std::filesystem::remove(file);
							g_logger().info("Deleted old backup file: {}", file.path().string());
						}
					}
				}
			} catch (const std::filesystem::filesystem_error &e) {
				g_logger().error("Failed to check or delete files in backup directory: {}. Error: {}", entry.path().string(), e.what());
			}
		}
	}
}

bool Database::beginTransaction() {
	if (!executeQuery("BEGIN")) {
		return false;
	}
	metrics::lock_latency measureLock("database");
	databaseLock.lock();
	measureLock.stop();

	return true;
}

bool Database::rollback() {
	if (!handle) {
		g_logger().error("Database not initialized!");
		return false;
	}

	PGresult* result = PQexec(handle, "ROLLBACK");
	if (!isSuccessfulResult(result)) {
		g_logger().error("PostgreSQL rollback failed: {}", PQerrorMessage(handle));
		PQclear(result);
		databaseLock.unlock();
		return false;
	}
	PQclear(result);

	databaseLock.unlock();
	return true;
}

bool Database::commit() {
	if (!handle) {
		g_logger().error("Database not initialized!");
		return false;
	}
	PGresult* result = PQexec(handle, "COMMIT");
	if (!isSuccessfulResult(result)) {
		g_logger().error("PostgreSQL commit failed: {}", PQerrorMessage(handle));
		PQclear(result);
		databaseLock.unlock();
		return false;
	}
	PQclear(result);

	databaseLock.unlock();
	return true;
}

bool Database::isRecoverableError(const PGresult* result, const PGconn* connection) {
	if (!connection || PQstatus(connection) != CONNECTION_OK) {
		return true;
	}
	const char* sqlState = result ? PQresultErrorField(result, PG_DIAG_SQLSTATE) : nullptr;
	return sqlState && sqlState[0] == '0' && sqlState[1] == '8';
}

bool Database::retryQuery(std::string_view query, int retries) {
	const auto normalizedQuery = normalizePostgreSQLQuery(query);
	for (int attempt = 0; attempt < retries; ++attempt) {
		PGresult* result = PQexec(handle, normalizedQuery.c_str());
		if (isSuccessfulResult(result)) {
			PQclear(result);
			return true;
		}

		g_logger().error("Query: {}", query.substr(0, 256));
		g_logger().error("PostgreSQL error: {}", PQerrorMessage(handle));
		const bool recoverable = isRecoverableError(result, handle);
		PQclear(result);
		if (!recoverable) {
			return false;
		}

		PQreset(handle);
		if (PQstatus(handle) != CONNECTION_OK) {
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	}
	g_logger().error("Query {} failed after {} retries.", query, retries);
	return false;
}

bool Database::executeQuery(std::string_view query) {
	if (!handle) {
		g_logger().error("Database not initialized!");
		return false;
	}

	g_logger().trace("Executing Query: {}", query);

	metrics::lock_latency measureLock("database");
	std::scoped_lock lock { databaseLock };
	measureLock.stop();

	metrics::query_latency measure(query.substr(0, 50));
	return retryQuery(query, 10);
}

DBResult_ptr Database::storeQuery(std::string_view query) {
	if (!handle) {
		g_logger().error("Database not initialized!");
		return nullptr;
	}
	g_logger().trace("Storing Query: {}", query);

	metrics::lock_latency measureLock("database");
	std::scoped_lock lock { databaseLock };
	measureLock.stop();

	metrics::query_latency measure(query.substr(0, 50));
	const auto normalizedQuery = normalizePostgreSQLQuery(query);
	for (int attempt = 0; attempt < 10; ++attempt) {
		PGresult* result = PQexec(handle, normalizedQuery.c_str());
		if (result && PQresultStatus(result) == PGRES_TUPLES_OK) {
			if (PQntuples(result) == 0) {
				PQclear(result);
				return nullptr;
			}
			return std::make_shared<DBResult>(result);
		}

		g_logger().error("Query: {}", query);
		g_logger().error("PostgreSQL error: {}", PQerrorMessage(handle));
		const bool recoverable = isRecoverableError(result, handle);
		PQclear(result);
		if (!recoverable) {
			return nullptr;
		}
		PQreset(handle);
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
	return nullptr;
}

std::string Database::escapeString(const std::string &s) const {
	metrics::lock_latency measureLock("database");
	std::scoped_lock lock { databaseLock };
	measureLock.stop();

	char* escaped = PQescapeLiteral(handle, s.data(), s.size());
	if (!escaped) {
		g_logger().error("Failed to escape PostgreSQL string: {}", PQerrorMessage(handle));
		return "''";
	}
	std::string result(escaped);
	PQfreemem(escaped);
	return result;
}

std::string Database::escapeBlob(const char* s, uint32_t length) const {
	metrics::lock_latency measureLock("database");
	std::scoped_lock lock { databaseLock };
	measureLock.stop();

	size_t escapedLength = 0;
	unsigned char* escaped = PQescapeByteaConn(handle, reinterpret_cast<const unsigned char*>(s), length, &escapedLength);
	if (!escaped) {
		g_logger().error("Failed to escape PostgreSQL bytea: {}", PQerrorMessage(handle));
		return "''";
	}
	std::string result("'");
	result.append(reinterpret_cast<const char*>(escaped), escapedLength - 1);
	result.push_back('\'');
	PQfreemem(escaped);
	return result;
}

uint64_t Database::getLastInsertId() {
	auto result = storeQuery("SELECT lastval() AS id");
	return result ? result->getNumber<uint64_t>("id") : 0;
}

DBResult::DBResult(PGresult* res) {
	handle = res;

	const int fieldCount = PQnfields(handle);
	for (int i = 0; i < fieldCount; ++i) {
		listNames[PQfname(handle, i)] = static_cast<size_t>(i);
	}
}

DBResult::~DBResult() {
	PQclear(handle);
}

std::string DBResult::getString(const std::string &s) const {
	auto it = listNames.find(s);
	if (it == listNames.end()) {
		g_logger().error("Column '{}' does not exist in result set", s);
		return {};
	}
	const int column = static_cast<int>(it->second);
	if (PQgetisnull(handle, rowIndex, column)) {
		return {};
	}
	return std::string(PQgetvalue(handle, rowIndex, column), static_cast<size_t>(PQgetlength(handle, rowIndex, column)));
}

const char* DBResult::getStream(const std::string &s, unsigned long &size) const {
	auto it = listNames.find(s);
	if (it == listNames.end()) {
		g_logger().error("Column '{}' doesn't exist in the result set", s);
		size = 0;
		return nullptr;
	}

	const int column = static_cast<int>(it->second);
	if (PQgetisnull(handle, rowIndex, column)) {
		size = 0;
		return nullptr;
	}

	if (PQftype(handle, column) != 17) { // bytea
		size = static_cast<unsigned long>(PQgetlength(handle, rowIndex, column));
		return PQgetvalue(handle, rowIndex, column);
	}

	auto &value = binaryValues[it->second];
	if (value.empty()) {
		size_t decodedLength = 0;
		unsigned char* decoded = PQunescapeBytea(reinterpret_cast<const unsigned char*>(PQgetvalue(handle, rowIndex, column)), &decodedLength);
		if (!decoded) {
			size = 0;
			return nullptr;
		}
		value.assign(reinterpret_cast<const char*>(decoded), decodedLength);
		PQfreemem(decoded);
	}
	size = static_cast<unsigned long>(value.size());
	return value.data();
}

uint8_t DBResult::getU8FromString(const std::string &string, const std::string &function) {
	auto result = static_cast<uint8_t>(std::atoi(string.c_str()));
	if (result > std::numeric_limits<uint8_t>::max()) {
		g_logger().error("[{}] Failed to get number value {} for tier table result, on function call: {}", __FUNCTION__, result, function);
		return 0;
	}

	return result;
}

int8_t DBResult::getInt8FromString(const std::string &string, const std::string &function) {
	auto result = static_cast<int8_t>(std::atoi(string.c_str()));
	if (result > std::numeric_limits<int8_t>::max()) {
		g_logger().error("[{}] Failed to get number value {} for tier table result, on function call: {}", __FUNCTION__, result, function);
		return 0;
	}

	return result;
}

size_t DBResult::countResults() const {
	return static_cast<size_t>(PQntuples(handle));
}

bool DBResult::hasNext() const {
	return rowIndex < PQntuples(handle);
}

bool DBResult::next() {
	if (!handle) {
		g_logger().error("Database not initialized!");
		return false;
	}
	++rowIndex;
	binaryValues.clear();
	return hasNext();
}

DBInsert::DBInsert(std::string insertQuery) :
	query(std::move(insertQuery)) {
	this->length = this->query.length();
}

bool DBInsert::addRow(std::string_view row) {
	const size_t rowLength = row.length();
	auto max_packet_size = Database::getInstance().getMaxPacketSize();
	size_t addedLength = values.empty() ? rowLength + 2 : rowLength + 3;

	if (length + addedLength > max_packet_size) {
		if (values.empty() || !execute()) {
			return false;
		}

		addedLength = rowLength + 2;
		if (length + addedLength > max_packet_size) {
			return false;
		}
	}

	length += addedLength;
	if (values.empty()) {
		values.reserve(rowLength + 2);
		values.push_back('(');
		values.append(row);
		values.push_back(')');
	} else {
		values.reserve(values.length() + rowLength + 3);
		values.push_back(',');
		values.push_back('(');
		values.append(row);
		values.push_back(')');
	}
	return true;
}

bool DBInsert::addRow(std::ostringstream &row) {
	bool ret = addRow(row.str());
	row.str(std::string());
	return ret;
}

void DBInsert::upsert(const std::vector<std::string> &newConflictColumns, const std::vector<std::string> &updateColumns) {
	conflictColumns = newConflictColumns;
	upsertColumns = updateColumns;
}

bool DBInsert::execute() {
	if (values.empty()) {
		return true;
	}

	const std::string &baseQuery = this->query;
	std::string upsertQuery;

	if (!upsertColumns.empty()) {
		size_t estimatedSize = 32;
		for (const auto &column : conflictColumns) {
			estimatedSize += column.size() + 4;
		}
		for (const auto &column : upsertColumns) {
			estimatedSize += (column.size() * 2) + 16;
		}

		upsertQuery.reserve(estimatedSize);
		upsertQuery += " ON CONFLICT (";
		auto upsertOutput = std::back_inserter(upsertQuery);
		for (size_t i = 0; i < conflictColumns.size(); ++i) {
			upsertOutput = fmt::format_to(upsertOutput, "`{}`", conflictColumns[i]);
			if (i + 1 < conflictColumns.size()) {
				upsertQuery += ", ";
			}
		}
		upsertQuery += ") DO UPDATE SET ";
		for (size_t i = 0; i < upsertColumns.size(); ++i) {
			upsertOutput = fmt::format_to(upsertOutput, "`{0}` = EXCLUDED.`{0}`", upsertColumns[i]);
			if (i + 1 < upsertColumns.size()) {
				upsertQuery.push_back(',');
				upsertQuery.push_back(' ');
			}
		}
	}

	std::string currentBatch = values;
	const bool baseHasSpace = !baseQuery.empty() && baseQuery.back() == ' ';
	const size_t separatorSize = baseHasSpace ? 0U : 1U;
	const size_t queryPrefixSize = baseQuery.size() + separatorSize + upsertQuery.size();
	if (queryPrefixSize >= Database::MAX_QUERY_SIZE) {
		return false;
	}

	while (!currentBatch.empty()) {
		size_t cutPos = Database::MAX_QUERY_SIZE - queryPrefixSize;
		if (cutPos < currentBatch.size()) {
			cutPos = currentBatch.rfind("),(", cutPos);
			if (cutPos == std::string::npos) {
				return false;
			}
			cutPos += 2;
		} else {
			cutPos = currentBatch.size();
		}

		std::string batchValues = currentBatch.substr(0, cutPos);
		if (!batchValues.empty() && batchValues.back() == ',') {
			batchValues.pop_back();
		}
		currentBatch = currentBatch.substr(cutPos);

		std::string sql;
		sql.reserve(queryPrefixSize + batchValues.size());
		appendInsertBaseQuery(sql, baseQuery, baseHasSpace);
		sql += batchValues;
		sql += upsertQuery;

		if (!g_database().executeQuery(sql)) {
			return false;
		}
	}

	values.clear();
	length = this->query.length();
	return true;
}
