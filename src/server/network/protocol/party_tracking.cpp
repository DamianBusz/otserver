/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 */

#include "server/network/protocol/party_tracking.hpp"
#include "server/network/message/networkmessage.hpp"

#ifndef USE_PRECOMPILED_HEADERS
	#include <algorithm>
	#include <atomic>
#endif

namespace {
	// No retained gameplay objects or static destruction dependencies.
	std::atomic<uint64_t> nextPartyTrackingMemberId { 0 };

	nlohmann::json memberJson(const PartyTracking::Member &member) {
		return {
			{ "id", member.id }, { "creatureId", member.creatureId }, { "name", member.name },
			{ "x", member.x }, { "y", member.y }, { "z", member.z }, { "leader", member.leader }
		};
	}
}

PartyTracking::PartyTracking() :
	memberId(std::to_string(++nextPartyTrackingMemberId)) { }

bool PartyTracking::handleRequest(const std::string &buffer) {
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
		session = token;
		enabled = true;
	} else if (enabled && token == session && (action == "disable" || action == "resync")) {
		enabled = action != "disable";
	} else {
		return false;
	}
	// Requests coalesce until the shared sweep, including repeated enable/resync.
	resetPending = true;
	return true;
}

nlohmann::json PartyTracking::envelope(const char* type) {
	return {
		{ "version", 1 }, { "type", type }, { "session", session },
		{ "epoch", std::to_string(epoch) }, { "seq", std::to_string(++sequence) }
	};
}

void PartyTracking::appendBatches(std::vector<nlohmann::json> &messages, const char* type, const char* field, const nlohmann::json &entries) {
	const auto parts = std::max<size_t>(1, (entries.size() + BATCH_SIZE - 1) / BATCH_SIZE);
	for (size_t part = 0; part < parts; ++part) {
		auto message = envelope(type);
		message["part"] = part;
		message["parts"] = parts;
		message[field] = nlohmann::json::array();
		for (size_t i = part * BATCH_SIZE; i < std::min(entries.size(), (part + 1) * BATCH_SIZE); ++i) {
			message[field].push_back(entries[i]);
		}
		messages.push_back(std::move(message));
	}
}

std::vector<nlohmann::json> PartyTracking::refresh(uint64_t membershipRevision, const std::vector<Member> &members) {
	std::vector<nlohmann::json> messages;
	if (!needsRefresh()) {
		return messages;
	}
	if (!enabled) {
		membershipRevision = 0;
	}
	std::map<std::string, Member> current;
	if (membershipRevision != 0) {
		for (const auto &member : members) {
			current.emplace(member.id, member);
		}
	}
	if (resetPending || membershipRevision != revision) {
		++epoch;
		sequence = 0;
		revision = membershipRevision;
		resetPending = false;
		if (revision == 0) {
			auto message = envelope("clear");
			message["enabled"] = enabled;
			messages.push_back(std::move(message));
		} else {
			auto entries = nlohmann::json::array();
			for (const auto &[id, member] : current) {
				entries.push_back(memberJson(member));
			}
			appendBatches(messages, "snapshot", "members", entries);
		}
	} else if (revision != 0) {
		auto removed = nlohmann::json::array();
		for (const auto &[id, member] : previous) {
			if (!current.contains(id)) {
				removed.push_back(id);
			}
		}
		if (!removed.empty()) {
			appendBatches(messages, "remove", "ids", removed);
		}
		auto changed = nlohmann::json::array();
		for (const auto &[id, member] : current) {
			const auto old = previous.find(id);
			if (old == previous.end() || old->second != member) {
				changed.push_back(memberJson(member));
			}
		}
		if (!changed.empty()) {
			appendBatches(messages, "update", "members", changed);
		}
	}
	previous = std::move(current);
	return messages;
}

void PartyTracking::writePacket(NetworkMessage &msg, const nlohmann::json &payload) {
	msg.addByte(0x32);
	msg.addByte(OPCODE);
	msg.addString(payload.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace));
}
