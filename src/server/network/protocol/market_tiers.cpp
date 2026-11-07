/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 */

#include "server/network/protocol/market_tiers.hpp"
#include "server/network/message/networkmessage.hpp"
#include <fmt/format.h>
#ifndef USE_PRECOMPILED_HEADERS
	#include <algorithm>
#endif

std::optional<MarketTierOverview::Request> MarketTierOverview::parseRequest(const std::string &buffer) {
	if (buffer.size() > 256) {
		return std::nullopt;
	}
	const auto data = nlohmann::json::parse(buffer, nullptr, false);
	if (!data.is_object() || !data.contains("version") || !data["version"].is_number_integer() || data["version"] != 1
	    || data.value("action", nlohmann::json()) != "tiers" || !data.contains("itemId") || !data["itemId"].is_number_unsigned()
	    || data["itemId"] == 0 || data["itemId"] > 65535 || !data.contains("request") || !data["request"].is_string()) {
		return std::nullopt;
	}
	const auto token = data["request"].get<std::string>();
	if (token.size() != 32 || !std::all_of(token.begin(), token.end(), [](char ch) { return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'); })) {
		return std::nullopt;
	}
	return Request { data["itemId"].get<uint16_t>(), token };
}

std::string MarketTierOverview::query(uint16_t itemId, uint8_t maxTier, int64_t cutoff) {
	return fmt::format(
		"SELECT COALESCE(json_agg(tier_book), '[]'::json)::text AS book FROM ("
		"SELECT tier, sale, COUNT(*) AS offers, SUM(amount) AS amount, "
		"CASE WHEN sale = 1 THEN MIN(price) ELSE MAX(price) END AS price "
		"FROM market_offers WHERE itemtype = {} AND tier BETWEEN 0 AND {} "
		"AND amount > 0 AND price > 0 AND sale IN (0, 1) AND created > {} GROUP BY tier, sale) AS tier_book",
		itemId, std::min(maxTier, MarketTierOverview::MAX_TIER), cutoff
	);
}

nlohmann::json MarketTierOverview::response(const Request &request, uint8_t maxTier, const std::optional<std::vector<Row>> &rows) {
	auto payload = nlohmann::json { { "version", 1 }, { "itemId", request.itemId }, { "request", request.token }, { "ok", rows.has_value() } };
	if (!rows) {
		return payload; // Query failures are never advertised as zero listings.
	}
	maxTier = std::min(maxTier, MAX_TIER);
	auto tiers = nlohmann::json::array();
	for (uint8_t tier = 0; tier <= maxTier; ++tier) {
		tiers.push_back({ { "tier", tier }, { "buyOffers", 0 }, { "sellOffers", 0 }, { "buyAmount", "0" }, { "sellAmount", "0" }, { "bestBuy", "0" }, { "bestSell", "0" } });
	}
	for (const auto &row : *rows) {
		if (row.tier > maxTier || row.action > 1 || row.offers == 0 || row.amount == 0 || row.bestPrice == 0) {
			continue;
		}
		auto &tier = tiers[row.tier];
		const bool sell = row.action == 1;
		tier[sell ? "sellOffers" : "buyOffers"] = row.offers;
		tier[sell ? "sellAmount" : "buyAmount"] = std::to_string(row.amount);
		tier[sell ? "bestSell" : "bestBuy"] = std::to_string(row.bestPrice);
	}
	payload["maxTier"] = maxTier;
	payload["tiers"] = std::move(tiers);
	return payload;
}

void MarketTierOverview::writePacket(NetworkMessage &msg, const nlohmann::json &payload) {
	msg.addByte(0x32);
	msg.addByte(OPCODE);
	msg.addString(payload.dump());
}
