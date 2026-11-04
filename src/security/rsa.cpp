/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "security/rsa.hpp"

#include "lib/di/container.hpp"
#include "security/rsa_backend.hpp"

#ifndef USE_PRECOMPILED_HEADERS
	#include <stdexcept>
#endif

RSAManager::RSAManager(Logger &logger) :
	logger(logger),
	backend(createMbedTlsRsaBackend(logger)) {
}

RSAManager::~RSAManager() = default;

RSAManager &RSAManager::getInstance() {
	return inject<RSAManager>();
}

void RSAManager::start(const std::string &filename) {
	try {
		if (loadPEM(filename)) {
			return;
		}
	} catch (const std::exception &e) {
		logger.error("Loading RSA key from {} failed: {}", filename, e.what());
		throw;
	}

	logger.error("RSA key file {} was not found or is invalid; aborting startup", filename);
	throw std::runtime_error(fmt::format("RSA key file {} was not found or is invalid", filename));
}

void RSAManager::setKey(const char* pString, const char* qString, int base) {
	backend->setKey(pString, qString, base);
}

void RSAManager::decrypt(char* msg) const {
	backend->decrypt(msg);
}

bool RSAManager::loadPEM(const std::string &filename) {
	return backend->loadPEM(filename);
}
