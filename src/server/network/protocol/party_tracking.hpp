/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

class NetworkMessage;

// Reserved extended opcode; see docs/systems/party-position-tracking.md.
class PartyTracking final {
public:
	static constexpr uint8_t OPCODE = 89;
	static constexpr uint32_t INTERVAL_MS = 200;
	static constexpr size_t BATCH_SIZE = 32;

	struct Member {
		std::string id;
		uint32_t creatureId = 0;
		std::string name;
		uint16_t x = 0;
		uint16_t y = 0;
		uint8_t z = 0;
		bool leader = false;
		bool operator==(const Member &) const = default;
	};

	PartyTracking();
	PartyTracking(const PartyTracking &) = delete;
	PartyTracking &operator=(const PartyTracking &) = delete;
	PartyTracking(PartyTracking &&) = delete;
	PartyTracking &operator=(PartyTracking &&) = delete;

	// The identity belongs to the connection, including clients that do not subscribe.
	const std::string &getMemberId() const { return memberId; }
	bool needsRefresh() const { return enabled || resetPending; }
	bool handleRequest(const std::string &buffer);
	std::vector<nlohmann::json> refresh(uint64_t membershipRevision, const std::vector<Member> &members);
	static void writePacket(NetworkMessage &msg, const nlohmann::json &payload);

private:
	nlohmann::json envelope(const char* type);
	void appendBatches(std::vector<nlohmann::json> &messages, const char* type, const char* field, const nlohmann::json &entries);

	const std::string memberId;
	std::string session;
	bool enabled = false;
	bool resetPending = false;
	uint64_t revision = 0;
	uint64_t epoch = 0;
	uint64_t sequence = 0;
	std::map<std::string, Member> previous;
};
