/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "security/argon.hpp"

#include <argon2.h>

bool Argon2::verifyPassword(const std::string &password, const std::string &phash) const {
	if (!phash.starts_with("$argon2id$")) {
		return false;
	}

	const auto result = argon2id_verify(phash.c_str(), password.data(), password.size());
	if (result != ARGON2_OK && result != ARGON2_VERIFY_MISMATCH) {
		g_logger().warn("Invalid Argon2 password hash: {}", argon2_error_message(result));
	}
	return result == ARGON2_OK;
}

bool Argon2::argon(const std::string &password_attempt, const std::string &hashed_password) const {
	return verifyPassword(password_attempt, hashed_password);
}
