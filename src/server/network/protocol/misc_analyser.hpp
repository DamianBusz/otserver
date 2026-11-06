/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 */

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <nlohmann/json.hpp>

class NetworkMessage;

// Connection-owned, bounded counters. No timers or retained gameplay objects.
class MiscAnalyser final {
public:
	static constexpr uint8_t OPCODE = 90;
	static constexpr uint8_t CHARM_COUNT = 25;
	static constexpr uint8_t COUNTER_COUNT = 32;
	static constexpr uint64_t MAX_DELTA = 9007199254740991ULL;

	bool handleRequest(const std::string &buffer);
	void record(uint8_t counter, uint64_t amount = 1);
	nlohmann::json takePending();
	static void writePacket(NetworkMessage &msg, const nlohmann::json &payload);

private:
	bool enabled = false;
	bool acknowledge = false;
	std::string session;
	std::array<uint64_t, COUNTER_COUNT> pending {};
};
