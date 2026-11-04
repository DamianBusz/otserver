/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019-2023 OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "lib/logging/in_memory_logger.hpp"
#include "security/rsa.hpp"

#ifndef USE_PRECOMPILED_HEADERS
	#include <stdexcept>
#endif

class RSATest : public ::testing::Test {
protected:
	static void SetUpTestSuite() {
		previousContainer = DI::getTestContainer();
		DI::setTestContainer(&InMemoryLogger::install(injector));
		logger = &dynamic_cast<InMemoryLogger &>(DI::get<Logger>());
	}

	static void TearDownTestSuite() {
		DI::setTestContainer(previousContainer);
	}

	void SetUp() override {
		logger = &logger->reset();
	}

	static InMemoryLogger &testLogger() {
		return *logger;
	}

private:
	inline static di::extension::injector<> injector {};
	inline static di::extension::injector<>* previousContainer { nullptr };
	inline static InMemoryLogger* logger { nullptr };
};

TEST_F(RSATest, StartRejectsMissingPemFile) {
	EXPECT_THROW(DI::create<RSAManager &>().start("non_existent_key.pem"), std::runtime_error);

	auto &logger = testLogger();

	ASSERT_EQ(1u, logger.logs.size());
	EXPECT_EQ(std::string { "error" }, logger.logs[0].level);
	EXPECT_EQ(
		fmt::format("RSA key file non_existent_key.pem was not found or is invalid; aborting startup"),
		logger.logs[0].message
	);
}
