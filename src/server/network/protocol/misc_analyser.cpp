/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 */

#include "server/network/protocol/misc_analyser.hpp"
#include "server/network/message/networkmessage.hpp"

#ifndef USE_PRECOMPILED_HEADERS
	#include <algorithm>
	#include <utility>
#endif

bool MiscAnalyser::handleRequest(const std::string &buffer) {
	if (buffer.size() > 256) {
		return false;
	}
	const auto request = nlohmann::json::parse(buffer, nullptr, false);
	if (!request.is_object() || !request.contains("version") || !request["version"].is_number_integer()
	    || request["version"] != 1 || !request.contains("action") || !request["action"].is_string()
	    || !request.contains("session") || !request["session"].is_string()) {
		return false;
	}
	const auto token = request["session"].get<std::string>();
	if (token.size() != 32 || !std::all_of(token.begin(), token.end(), [](char ch) {
			return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
		})) {
		return false;
	}
	const auto action = request["action"].get<std::string>();
	if (action == "enable") {
		if (enabled && session == token) {
			return true; // Duplicate enable must not lose pending deltas.
		}
		session = token;
		enabled = true;
		acknowledge = true;
	} else if (action == "disable" && enabled && session == token) {
		enabled = false;
		acknowledge = false;
	} else {
		return false;
	}
	pending.fill(0);
	return true;
}

void MiscAnalyser::record(uint8_t counter, uint64_t amount) {
	if (!enabled || counter >= COUNTER_COUNT || amount == 0) {
		return;
	}
	pending[counter] += std::min(amount, MAX_DELTA - pending[counter]);
}

nlohmann::json MiscAnalyser::takePending() {
	if (!enabled) {
		return nullptr;
	}
	auto events = nlohmann::json::array();
	static constexpr std::array<const char*, 7> ids { "critical", "mana", "life", "onslaught", "ruse", "momentum", "transcendence" };
	for (uint8_t counter = 0; counter < COUNTER_COUNT; ++counter) {
		if (pending[counter] == 0) {
			continue;
		}
		auto event = nlohmann::json { { "amount", pending[counter] } };
		if (counter < CHARM_COUNT) {
			event["category"] = "charm";
			event["id"] = counter;
		} else {
			event["category"] = counter < CHARM_COUNT + 3 ? "imbuement" : "forge";
			event["id"] = ids[counter - CHARM_COUNT];
		}
		events.push_back(std::move(event));
	}
	if (events.empty() && !acknowledge) {
		return nullptr;
	}
	acknowledge = false;
	pending.fill(0);
	return { { "version", 1 }, { "session", session }, { "events", std::move(events) } };
}

void MiscAnalyser::writePacket(NetworkMessage &msg, const nlohmann::json &payload) {
	msg.addByte(0x32);
	msg.addByte(OPCODE);
	msg.addString(payload.dump());
}
