/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 */
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

class NetworkMessage;

class MarketTierOverview final {
public:
	static constexpr uint8_t OPCODE = 91;
	static constexpr uint8_t MAX_TIER = 10;
	struct Request {
		uint16_t itemId = 0;
		std::string token;
	};
	struct Row {
		uint8_t tier = 0;
		uint8_t action = 0;
		uint32_t offers = 0;
		uint64_t amount = 0;
		uint64_t bestPrice = 0;
	};
	static std::optional<Request> parseRequest(const std::string &buffer);
	static std::string query(uint16_t itemId, uint8_t maxTier, int64_t cutoff);
	static nlohmann::json response(const Request &request, uint8_t maxTier, const std::optional<std::vector<Row>> &rows);
	static void writePacket(NetworkMessage &msg, const nlohmann::json &payload);
};
