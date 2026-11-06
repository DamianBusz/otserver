/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 */

#include "server/network/protocol/misc_analyser.hpp"
#include "server/network/protocol/protocolgame.hpp"
#include "server/network/message/outputmessage.hpp"
#include "creatures/players/player.hpp"
#include "creatures/players/grouping/groups.hpp"
#include "creatures/players/grouping/party.hpp"
#include "creatures/players/livestream/livestream.hpp"
#include "creatures/players/vocations/vocation.hpp"
#include "creatures/interactions/chat.hpp"
#include "creatures/combat/combat.hpp"
#include "creatures/combat/spells.hpp"
#include "creatures/monsters/monsters.hpp"
#include "config/configmanager.hpp"
#include "game/game.hpp"
#include "io/iobestiary.hpp"
#include "items/tile.hpp"
#include "items/item.hpp"
#include "lib/di/container.hpp"
#include "lib/logging/in_memory_logger.hpp"
#include "lua/creature/events.hpp"
#include "kv/in_memory_kv.hpp"

#ifndef USE_PRECOMPILED_HEADERS
	#include <gtest/gtest.h>
	#include <limits>
#endif

namespace {
	constexpr auto TOKEN = "0123456789abcdef0123456789abcdef";
	constexpr auto NEXT_TOKEN = "ffffffffffffffffffffffffffffffff";
	std::string request(const char* action = "enable", const char* token = TOKEN) {
		return nlohmann::json { { "version", 1 }, { "action", action }, { "session", token } }.dump();
	}
	uint64_t value(const nlohmann::json &payload, const char* category, const nlohmann::json &id) {
		if (!payload.is_object()) {
			return 0;
		}
		for (const auto &event : payload["events"]) {
			if (event["category"] == category && event["id"] == id) {
				return event["amount"].get<uint64_t>();
			}
		}
		return 0;
	}
	class MiscTestInjector : public di::extension::injector<> {
	public:
		void drain() {
			cfg().data<Chat>().reset();
			cfg().data<Events>().reset();
			cfg().data<Spells>().reset();
			cfg().data<Monsters>().reset();
			cfg().data<Game>().reset();
		}
	};
}

TEST(MiscAnalyserTest, ExplicitOptInResetDisableAndInvalidRequests) {
	MiscAnalyser counters;
	counters.record(28);
	EXPECT_TRUE(counters.takePending().is_null());
	for (const auto &bad : { std::string("{"), std::string("[]"), request("enable", "bad"), request("unknown"),
	                         std::string("{\"version\":2,\"action\":\"enable\",\"session\":\"0123456789abcdef0123456789abcdef\"}"), std::string(257, 'x') }) {
		EXPECT_FALSE(counters.handleRequest(bad));
		EXPECT_TRUE(counters.takePending().is_null());
	}
	ASSERT_TRUE(counters.handleRequest(request()));
	const auto ack = counters.takePending();
	EXPECT_EQ(ack["session"], TOKEN);
	EXPECT_TRUE(ack["events"].empty());
	counters.record(28, 5);
	ASSERT_TRUE(counters.handleRequest(request()));
	EXPECT_EQ(value(counters.takePending(), "forge", "onslaught"), 5U);
	counters.record(28, 9);
	ASSERT_TRUE(counters.handleRequest(request("enable", NEXT_TOKEN)));
	const auto reset = counters.takePending();
	EXPECT_EQ(reset["session"], NEXT_TOKEN);
	EXPECT_TRUE(reset["events"].empty());
	EXPECT_FALSE(counters.handleRequest(request("disable")));
	counters.record(31, 3);
	EXPECT_EQ(value(counters.takePending(), "forge", "transcendence"), 3U);
	ASSERT_TRUE(counters.handleRequest(request("disable", NEXT_TOKEN)));
	counters.record(31);
	EXPECT_TRUE(counters.takePending().is_null());
}

TEST(MiscAnalyserTest, AllCountersBatchExactlyOnceWithinClientLimitsAndNextOpcodeBoundary) {
	MiscAnalyser counters;
	ASSERT_TRUE(counters.handleRequest(request()));
	for (uint8_t id = 0; id < 32; ++id) {
		counters.record(id, id + 1);
		counters.record(id, 2);
	}
	counters.record(255, 7);
	counters.record(0, 0);
	const auto payload = counters.takePending();
	ASSERT_EQ(payload["events"].size(), 32U);
	RecordProperty("wire_payload", payload.dump());
	for (uint8_t id = 0; id < 25; ++id) {
		EXPECT_EQ(value(payload, "charm", id), id + 3U);
	}
	EXPECT_EQ(value(payload, "imbuement", "critical"), 28U);
	EXPECT_EQ(value(payload, "imbuement", "mana"), 29U);
	EXPECT_EQ(value(payload, "imbuement", "life"), 30U);
	EXPECT_EQ(value(payload, "forge", "onslaught"), 31U);
	EXPECT_EQ(value(payload, "forge", "ruse"), 32U);
	EXPECT_EQ(value(payload, "forge", "momentum"), 33U);
	EXPECT_EQ(value(payload, "forge", "transcendence"), 34U);
	EXPECT_TRUE(counters.takePending().is_null());
	EXPECT_LT(payload.dump().size(), 32768U);
	NetworkMessage wire;
	MiscAnalyser::writePacket(wire, payload);
	wire.addByte(0x1D);
	wire.setBufferPosition(NetworkMessage::INITIAL_BUFFER_POSITION);
	EXPECT_EQ(wire.getByte(), 0x32);
	EXPECT_EQ(wire.getByte(), 90);
	EXPECT_EQ(nlohmann::json::parse(wire.getString()), payload);
	EXPECT_EQ(wire.getByte(), 0x1D);
	EXPECT_FALSE(wire.isOverrun());
	counters.record(26, std::numeric_limits<uint64_t>::max());
	counters.record(26, 5);
	EXPECT_EQ(value(counters.takePending(), "imbuement", "mana"), MiscAnalyser::MAX_DELTA);
}

class MiscAnalyserProtocolTest : public ::testing::Test {
protected:
	static void SetUpTestSuite() {
		previous = DI::getTestContainer();
		InMemoryLogger::install(injector);
		KVMemory::install(injector);
		DI::setTestContainer(&injector);
		g_configManager().setConfigFileLua("config.lua.dist");
		ASSERT_TRUE(g_configManager().load());
		ASSERT_TRUE(g_vocations().loadFromXml());
	}
	static void TearDownTestSuite() {
		injector.drain();
		DI::setTestContainer(previous);
	}
	void SetUp() override {
		originalItemsSize = Item::items.getItems().size();
		if (originalItemsSize == 0) {
			Item::items.getItems().resize(1);
		}
	}
	void TearDown() override {
		for (const auto &player : players) {
			g_livestream().applyState(player, {});
			if (player->getParty()) {
				player->getParty()->disband();
			}
			if (g_game().getPlayerByID(player->getID()) == player) {
				g_game().removePlayer(player);
			}
			g_game().removeDeadPlayer(player->getName());
			player->setParent({});
			if (player->getClient()) {
				player->getClient()->player.reset();
			}
		}
		players.clear();
		if (originalItemsSize == 0) {
			Item::items.getItems().clear();
		}
	}
	std::shared_ptr<Player> makePlayer(uint32_t id) {
		auto player = std::make_shared<Player>();
		player->setName("MiscTest" + std::to_string(id));
		player->setGUID(id);
		player->setID();
		player->setLevel(100);
		player->setGroup(std::make_shared<Group>());
		EXPECT_TRUE(player->setVocation(VOCATION_KNIGHT));
		player->setVarStats(STAT_MAXHITPOINTS, 10000 - player->getMaxHealth());
		player->setVarStats(STAT_MAXMANAPOINTS, 10000 - player->getMaxMana());
		player->changeHealth(10000, false);
		player->changeMana(10000);
		auto tile = std::make_shared<DynamicTile>(Position(100 + id, 100, 7));
		tile->addThing(player);
		tiles.push_back(tile);
		attach(player);
		g_game().addPlayer(player);
		players.push_back(player);
		return player;
	}
	void attach(const std::shared_ptr<Player> &player) {
		auto connection = std::make_shared<Connection>(io, nullptr);
		connections.push_back(connection);
		auto protocol = std::make_shared<ProtocolGame>(connection);
		protocol->player = player;
		protocol->acceptPackets = true;
		player->client = protocol;
	}
	void negotiate(const std::shared_ptr<Player> &player) {
		NetworkMessage msg;
		msg.addByte(90);
		msg.addString(request());
		msg.setBufferPosition(NetworkMessage::INITIAL_BUFFER_POSITION);
		player->getClient()->parseExtendedOpcode(msg);
	}
	void legacy(const std::shared_ptr<Player> &player) {
		player->getClient()->oldProtocol = true;
	}
	void viewer(const std::shared_ptr<Player> &player) {
		player->getClient()->m_isLivestreamViewer = true;
	}
	nlohmann::json pending(const std::shared_ptr<Player> &player) {
		return player->getClient()->miscAnalyser.takePending();
	}
	void clearOutput(const std::shared_ptr<Player> &player) {
		player->getClient()->getCurrentBuffer().reset();
	}
	void stale(const std::shared_ptr<ProtocolGame> &protocol) {
		protocol->acceptPackets = false;
		protocol->player.reset();
	}
	void broadcastTo(const std::shared_ptr<Player> &player, const std::shared_ptr<Player> &watcher) {
		LivestreamState state;
		state.broadcast = true;
		g_livestream().applyState(player, state);
		g_livestream().addViewer(player, watcher->getClient(), true);
	}
	asio::io_service io;
	size_t originalItemsSize = 0;
	std::vector<std::shared_ptr<Player>> players;
	std::vector<std::shared_ptr<Connection>> connections;
	std::vector<std::shared_ptr<Tile>> tiles;
	inline static MiscTestInjector injector {};
	inline static decltype(DI::getTestContainer()) previous = nullptr;
};

TEST_F(MiscAnalyserProtocolTest, UnsupportedLegacyViewersAndReplacementConnectionsCannotReceive) {
	auto supported = makePlayer(1), unsupported = makePlayer(2), old = makePlayer(3), watcher = makePlayer(4);
	legacy(old);
	viewer(watcher);
	for (const auto &player : { supported, old, watcher }) {
		negotiate(player);
	}
	for (const auto &player : players) {
		player->recordMiscEffect(MiscAnalyserCounter::Momentum);
	}
	EXPECT_EQ(value(pending(supported), "forge", "momentum"), 1U);
	for (const auto &player : { unsupported, old, watcher }) {
		EXPECT_TRUE(pending(player).is_null());
	}
	auto replaced = supported->getClient();
	attach(supported);
	replaced->recordMiscCounter(28);
	supported->recordMiscEffect(MiscAnalyserCounter::Onslaught);
	EXPECT_TRUE(pending(supported).is_null());
	negotiate(supported);
	supported->recordMiscEffect(MiscAnalyserCounter::Onslaught);
	EXPECT_EQ(value(pending(supported), "forge", "onslaught"), 1U);
	stale(replaced);
}

TEST_F(MiscAnalyserProtocolTest, SharedSweepDeliversOnlyToOwnerWithoutCastMirroring) {
	auto owner = makePlayer(11), watcher = makePlayer(12);
	negotiate(owner);
	broadcastTo(owner, watcher);
	clearOutput(owner);
	clearOutput(watcher);
	owner->recordMiscCharm(CHARM_CURSE);
	owner->recordMiscEffect(MiscAnalyserCounter::Transcendence, 2);
	g_game().updatePartyTracking();
	auto output = owner->getClient()->getCurrentBuffer();
	ASSERT_TRUE(output);
	output->setBufferPosition(NetworkMessage::INITIAL_BUFFER_POSITION);
	EXPECT_EQ(output->getByte(), 0x32);
	EXPECT_EQ(output->getByte(), 90);
	const auto payload = nlohmann::json::parse(output->getString());
	EXPECT_EQ(value(payload, "charm", 5), 1U);
	EXPECT_EQ(value(payload, "forge", "transcendence"), 2U);
	EXPECT_FALSE(watcher->getClient()->getCurrentBuffer());
	clearOutput(owner);
	g_game().updatePartyTracking();
	EXPECT_FALSE(owner->getClient()->getCurrentBuffer());
}

TEST_F(MiscAnalyserProtocolTest, ActualLeechGainsAreCappedAndFullBarsDoNotCount) {
	auto player = makePlayer(21), target = makePlayer(22);
	negotiate(player);
	pending(player);
	player->setVarSkill(SKILL_LIFE_LEECH_AMOUNT, 10000);
	player->setVarSkill(SKILL_MANA_LEECH_AMOUNT, 10000);
	player->changeHealth(-7, false);
	player->changeMana(-9);
	CombatDamage damage;
	damage.affected = 1;
	g_game().applyLifeLeech(player, nullptr, target, damage, 100);
	g_game().applyManaLeech(player, nullptr, target, damage, 100);
	const auto batch = pending(player);
	EXPECT_EQ(value(batch, "imbuement", "life"), 7U);
	EXPECT_EQ(value(batch, "imbuement", "mana"), 9U);
	g_game().applyLifeLeech(player, nullptr, target, damage, 100);
	g_game().applyManaLeech(player, nullptr, target, damage, 100);
	EXPECT_TRUE(pending(player).is_null());
}

TEST_F(MiscAnalyserProtocolTest, CriticalFatalAndPassiveCharmAttributionCountOncePerDamagedTarget) {
	auto attacker = makePlayer(31), target = makePlayer(32);
	negotiate(attacker);
	pending(attacker);
	CombatDamage damage;
	damage.primary.type = COMBAT_PHYSICALDAMAGE;
	damage.primary.value = -100;
	damage.secondary.type = COMBAT_FIREDAMAGE;
	damage.secondary.value = -50;
	damage.critical = true;
	damage.fatal = true;
	damage.miscCharmProcs = (uint32_t(1) << CHARM_LOW) | (uint32_t(1) << CHARM_SAVAGE);
	ASSERT_TRUE(g_game().combatChangeHealth(attacker, target, damage));
	const auto batch = pending(attacker);
	EXPECT_EQ(value(batch, "imbuement", "critical"), 1U);
	EXPECT_EQ(value(batch, "forge", "onslaught"), 1U);
	EXPECT_EQ(value(batch, "charm", CHARM_LOW), 1U);
	EXPECT_EQ(value(batch, "charm", CHARM_SAVAGE), 1U);
	damage.primary.value = -10;
	damage.secondary.value = 0;
	damage.extension = true;
	ASSERT_TRUE(g_game().combatChangeHealth(attacker, target, damage));
	EXPECT_TRUE(pending(attacker).is_null());
	damage.primary.value = -10;
	damage.extension = false;
	damage.suppressCharms = true;
	ASSERT_TRUE(g_game().combatChangeHealth(attacker, target, damage));
	const auto splash = pending(attacker);
	EXPECT_EQ(value(splash, "imbuement", "critical"), 1U);
	EXPECT_EQ(value(splash, "charm", CHARM_LOW), 0U);
	EXPECT_EQ(value(splash, "charm", CHARM_SAVAGE), 0U);
}

TEST_F(MiscAnalyserProtocolTest, OffensiveCharmHandlersReportTheActivatedRuneOnly) {
	auto attacker = makePlayer(41), target = makePlayer(42);
	negotiate(attacker);
	pending(attacker);
	for (auto id : { CHARM_WOUND, CHARM_ENFLAME, CHARM_POISON, CHARM_FREEZE, CHARM_ZAP, CHARM_CURSE, CHARM_DIVINE, CHARM_OVERPOWER, CHARM_OVERFLUX }) {
		auto charm = std::make_shared<Charm>();
		charm->id = id;
		charm->type = CHARM_OFFENSIVE;
		charm->damageType = COMBAT_PHYSICALDAMAGE;
		charm->percent = 5;
		ASSERT_TRUE(g_iobestiary().parseCharmCombat(charm, attacker, target));
		const auto batch = pending(attacker);
		EXPECT_EQ(value(batch, "charm", id), 1U);
		EXPECT_EQ(value(batch, "imbuement", "critical"), 0U);
		EXPECT_EQ(value(batch, "forge", "onslaught"), 0U);
	}
}
