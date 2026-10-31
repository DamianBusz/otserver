/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 */

#include "server/network/protocol/party_tracking.hpp"
#include "server/network/protocol/protocolgame.hpp"
#include "server/network/message/outputmessage.hpp"
#include "creatures/players/grouping/party.hpp"
#include "creatures/players/livestream/livestream.hpp"
#include "creatures/interactions/chat.hpp"
#include "creatures/combat/spells.hpp"
#include "creatures/monsters/monsters.hpp"
#include "creatures/players/grouping/groups.hpp"
#include "creatures/players/player.hpp"
#include "creatures/players/vocations/vocation.hpp"
#include "config/configmanager.hpp"
#include "game/game.hpp"
#include "lib/di/container.hpp"
#include "lib/logging/in_memory_logger.hpp"
#include "lua/creature/events.hpp"
#include "items/tile.hpp"

#ifndef USE_PRECOMPILED_HEADERS
	#include <gtest/gtest.h>
#endif

namespace {
	class PartyTrackingTestInjector : public di::extension::injector<> {
	public:
		void releaseLuaOwners() {
			cfg().data<Chat>().reset();
			cfg().data<Events>().reset();
			cfg().data<Spells>().reset();
			cfg().data<Monsters>().reset();
			cfg().data<Game>().reset();
		}
	};

	constexpr auto SESSION = "0123456789abcdef0123456789abcdef";

	std::string request(const char* action = "enable", const char* session = SESSION, int version = 1) {
		return nlohmann::json { { "version", version }, { "action", action }, { "session", session } }.dump();
	}
}

TEST(PartyTrackingTest, RequiresExplicitNegotiationAndIgnoresMalformedOrStaleRequests) {
	PartyTracking tracking;
	std::vector<PartyTracking::Member> members { { "10", 100, "Leader", 100, 200, 7, true } };
	EXPECT_TRUE(tracking.refresh(1, members).empty());
	for (const auto &bad : { std::string("[]"), std::string("{"), request("enable", SESSION, 2), request("enable", "wrong"), request("unknown"), std::string(257, 'x') }) {
		EXPECT_FALSE(tracking.handleRequest(bad));
		EXPECT_TRUE(tracking.refresh(1, members).empty());
	}
	ASSERT_TRUE(tracking.handleRequest(request()));
	auto snapshot = tracking.refresh(1, members);
	ASSERT_EQ(snapshot.size(), 1U);
	EXPECT_EQ(snapshot[0]["type"], "snapshot");
	EXPECT_EQ(snapshot[0]["session"], SESSION);
	EXPECT_EQ(snapshot[0]["members"][0]["name"], "Leader");
	EXPECT_FALSE(tracking.handleRequest(request("disable", "ffffffffffffffffffffffffffffffff")));
	EXPECT_TRUE(tracking.refresh(1, members).empty());
	ASSERT_TRUE(tracking.handleRequest(request("disable")));
	const auto clear = tracking.refresh(1, members);
	ASSERT_EQ(clear.size(), 1U);
	EXPECT_EQ(clear[0]["type"], "clear");
	EXPECT_EQ(clear[0]["enabled"], false);
	EXPECT_TRUE(tracking.refresh(1, members).empty());
}

TEST(PartyTrackingTest, BatchesSnapshotsAndResetsInsteadOfReusingOldEpochs) {
	PartyTracking tracking;
	ASSERT_TRUE(tracking.handleRequest(request()));
	std::vector<PartyTracking::Member> members;
	for (uint32_t i = 0; i < 70; ++i) {
		members.push_back({ std::to_string(i), i, "Name", 65535, 65535, 15, i == 0 });
	}
	auto messages = tracking.refresh(1, members);
	ASSERT_EQ(messages.size(), 3U);
	for (size_t i = 0; i < messages.size(); ++i) {
		EXPECT_EQ(messages[i]["type"], "snapshot");
		EXPECT_EQ(messages[i]["parts"], 3);
		EXPECT_EQ(messages[i]["part"], i);
		EXPECT_EQ(messages[i]["seq"], std::to_string(i + 1));
		EXPECT_LE(messages[i]["members"].size(), PartyTracking::BATCH_SIZE);
	}
	EXPECT_TRUE(tracking.refresh(1, members).empty());
	ASSERT_TRUE(tracking.handleRequest(request("resync")));
	EXPECT_NE(tracking.refresh(1, members)[0]["epoch"], messages[0]["epoch"]);
	const auto reset = tracking.refresh(0, {});
	ASSERT_EQ(reset.size(), 1U);
	EXPECT_EQ(reset[0]["type"], "clear");
	EXPECT_TRUE(tracking.refresh(0, {}).empty());
	EXPECT_NE(tracking.refresh(3, members)[0]["epoch"], reset[0]["epoch"]);
	ASSERT_TRUE(tracking.handleRequest(request("enable", "ffffffffffffffffffffffffffffffff")));
	EXPECT_EQ(tracking.refresh(3, members)[0]["session"], "ffffffffffffffffffffffffffffffff");
}

TEST(PartyTrackingTest, ProductionPacketEndsAtNextOpcodeBoundary) {
	PartyTracking tracking;
	ASSERT_TRUE(tracking.handleRequest(request()));
	const auto payload = tracking.refresh(1, { { "4", 0x10000004, "Quoted \"name\"", 33805, 32767, 2, true } })[0];
	NetworkMessage wire;
	PartyTracking::writePacket(wire, payload);
	wire.addByte(0x1D);
	wire.setBufferPosition(NetworkMessage::INITIAL_BUFFER_POSITION);
	EXPECT_EQ(wire.getByte(), 0x32);
	EXPECT_EQ(wire.getByte(), 89);
	EXPECT_EQ(nlohmann::json::parse(wire.getString()), payload);
	EXPECT_EQ(wire.getByte(), 0x1D);
	EXPECT_FALSE(wire.isOverrun());
}

class PartyTrackingProtocolTest : public ::testing::Test {
protected:
	static void SetUpTestSuite() {
		previousContainer = DI::getTestContainer();
		InMemoryLogger::install(injector);
		DI::setTestContainer(&injector);
		g_configManager().setConfigFileLua("config.lua.dist");
		ASSERT_TRUE(g_configManager().load());
		ASSERT_TRUE(g_vocations().loadFromXml());
	}
	static void TearDownTestSuite() {
		// Party lifecycle methods create Lua-owning services. Drain them while
		// this injector and its Lua environment are still the active context.
		injector.releaseLuaOwners();
		DI::setTestContainer(previousContainer);
	}
	void TearDown() override {
		for (const auto &player : players) {
			if (player->getClient()) {
				g_livestream().removeViewer(player->getClient(), true);
			}
		}
		for (const auto &player : players) {
			g_livestream().applyState(player, {});
			// Break protocol/player ownership before the fixture and its IO service die.
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
	}
	std::shared_ptr<Player> makePlayer(uint32_t id, const char* name, Position position = Position(100, 100, 7)) {
		auto player = std::make_shared<Player>();
		player->setName(name);
		player->setGUID(id);
		player->setID();
		player->setGroup(std::make_shared<Group>());
		EXPECT_TRUE(player->setVocation(VOCATION_KNIGHT));
		place(player, position);
		attach(player);
		g_game().addPlayer(player);
		players.push_back(player);
		return player;
	}
	void place(const std::shared_ptr<Player> &player, Position position) {
		auto tile = std::make_shared<DynamicTile>(position);
		tile->addThing(player);
		tiles.push_back(tile);
	}
	void attach(const std::shared_ptr<Player> &player) {
		auto connection = std::make_shared<Connection>(io, nullptr);
		connections.push_back(connection);
		auto protocol = std::make_shared<ProtocolGame>(connection);
		protocol->player = player;
		protocol->acceptPackets = true;
		player->client = protocol;
	}
	void negotiate(const std::shared_ptr<Player> &player, const std::string &payload = request()) {
		NetworkMessage msg;
		msg.addByte(PartyTracking::OPCODE);
		msg.addString(payload);
		msg.setBufferPosition(NetworkMessage::INITIAL_BUFFER_POSITION);
		player->getClient()->parseExtendedOpcode(msg);
	}
	void sweep() {
		for (const auto &player : players) {
			if (player->getClient()) {
				player->getClient()->getCurrentBuffer().reset();
			}
		}
		g_game().updatePartyTracking();
	}
	std::vector<nlohmann::json> packets(const std::shared_ptr<Player> &player) {
		std::vector<nlohmann::json> result;
		auto &output = player->getClient()->getCurrentBuffer();
		if (!output) {
			return result;
		}
		output->setBufferPosition(NetworkMessage::INITIAL_BUFFER_POSITION);
		while (output->getBufferPosition() < NetworkMessage::INITIAL_BUFFER_POSITION + output->getLength()) {
			EXPECT_EQ(output->getByte(), 0x32);
			EXPECT_EQ(output->getByte(), PartyTracking::OPCODE);
			result.push_back(nlohmann::json::parse(output->getString()));
		}
		output.reset();
		return result;
	}
	nlohmann::json onlyPacket(const std::shared_ptr<Player> &player) {
		auto result = packets(player);
		EXPECT_EQ(result.size(), 1U);
		return result.empty() ? nlohmann::json::object() : result.front();
	}
	void asViewer(const std::shared_ptr<Player> &player) { player->getClient()->m_isLivestreamViewer = true; }
	void broadcastTo(const std::shared_ptr<Player> &player, const std::shared_ptr<Player> &viewer) {
		LivestreamState state;
		state.broadcast = true;
		g_livestream().applyState(player, state);
		g_livestream().addViewer(player, viewer->getClient(), true);
	}
	void asLegacy(const std::shared_ptr<Player> &player) { player->getClient()->oldProtocol = true; }
	void setDead(const std::shared_ptr<Player> &player, bool value) { player->setDead(value); }
	void setHealth(const std::shared_ptr<Player> &player, int32_t value) { player->health = value; }
	void detach(const std::shared_ptr<Player> &player) {
		player->getClient()->acceptPackets = false;
		player->getClient()->player.reset();
		player->client.reset();
	}
	asio::io_service io;
	std::vector<std::shared_ptr<Player>> players;
	std::vector<std::shared_ptr<Connection>> connections;
	std::vector<std::shared_ptr<Tile>> tiles;
	inline static PartyTrackingTestInjector injector {};
	inline static di::extension::injector<>* previousContainer = nullptr;
};

TEST_F(PartyTrackingProtocolTest, JoinLeaveDisbandAndLeaderTransferUseIndependentRecipientState) {
	auto leader = makePlayer(1, "Leader");
	auto member = makePlayer(2, "Member", Position(33000, 32000, 9));
	auto third = makePlayer(3, "Third");
	negotiate(leader);
	negotiate(member);
	negotiate(third);
	auto party = Party::create(leader);
	ASSERT_TRUE(party->invitePlayer(member));
	ASSERT_TRUE(party->joinParty(member));
	ASSERT_TRUE(party->invitePlayer(third));
	ASSERT_TRUE(party->joinParty(third));
	sweep();
	const auto snapshot = packets(member);
	ASSERT_EQ(snapshot.size(), 1U);
	ASSERT_EQ(snapshot[0]["members"].size(), 3U);
	ASSERT_TRUE(party->passPartyLeadership(member));
	sweep();
	auto update = packets(third);
	ASSERT_EQ(update.size(), 1U);
	EXPECT_EQ(update[0]["type"], "update");
	ASSERT_EQ(update[0]["members"].size(), 2U);
	for (const auto &entry : update[0]["members"]) {
		EXPECT_EQ(entry["leader"], entry["name"] == "Member");
	}
	ASSERT_TRUE(party->leaveParty(third));
	sweep();
	EXPECT_EQ(onlyPacket(third)["type"], "clear");
	EXPECT_EQ(onlyPacket(member)["type"], "remove");
	party->disband();
	sweep();
	EXPECT_EQ(onlyPacket(leader)["type"], "clear");
	EXPECT_EQ(onlyPacket(member)["type"], "clear");
}

TEST_F(PartyTrackingProtocolTest, RemoteMovementTeleportsAndFloorChangesAreCoalesced) {
	auto leader = makePlayer(11, "Leader");
	auto member = makePlayer(12, "Member");
	auto party = Party::create(leader);
	ASSERT_TRUE(party->invitePlayer(member));
	ASSERT_TRUE(party->joinParty(member));
	negotiate(leader);
	sweep();
	ASSERT_EQ(packets(leader).size(), 1U);
	place(member, Position(101, 100, 7));
	place(member, Position(102, 100, 7));
	place(member, Position(34000, 32000, 12));
	EXPECT_FALSE(leader->getClient()->getCurrentBuffer());
	sweep();
	auto update = packets(leader);
	ASSERT_EQ(update.size(), 1U);
	ASSERT_EQ(update[0]["members"].size(), 1U);
	EXPECT_EQ(update[0]["members"][0]["x"], 34000);
	EXPECT_EQ(update[0]["members"][0]["z"], 12);
	sweep();
	EXPECT_TRUE(packets(leader).empty());
	EXPECT_TRUE(packets(member).empty()); // Unsupported member still trackable, never a recipient.
}

TEST_F(PartyTrackingProtocolTest, DisconnectReconnectAndDeathRemoveOldSessionMarkers) {
	auto leader = makePlayer(21, "Leader");
	auto member = makePlayer(22, "Member");
	auto party = Party::create(leader);
	ASSERT_TRUE(party->invitePlayer(member));
	ASSERT_TRUE(party->joinParty(member));
	negotiate(leader);
	negotiate(member);
	sweep();
	const auto original = packets(member);
	ASSERT_EQ(original.size(), 1U);
	detach(member);
	sweep();
	const auto removed = packets(leader);
	ASSERT_EQ(removed.size(), 1U);
	EXPECT_EQ(removed[0]["type"], "remove");
	attach(member);
	sweep();
	EXPECT_TRUE(packets(member).empty()); // Capabilities do not survive reconnection.
	const auto rejoined = packets(leader);
	ASSERT_EQ(rejoined.size(), 1U);
	EXPECT_NE(rejoined[0]["members"][0]["id"], removed[0]["ids"][0]);
	negotiate(member, request("enable", "ffffffffffffffffffffffffffffffff"));
	sweep();
	EXPECT_EQ(onlyPacket(member)["type"], "snapshot");
	setHealth(member, 0);
	setDead(member, true);
	g_game().removePlayer(member);
	sweep();
	EXPECT_EQ(onlyPacket(leader)["type"], "remove");
	EXPECT_EQ(onlyPacket(member)["type"], "clear");
	setHealth(member, 100);
	setDead(member, false);
	g_game().addPlayer(member);
	place(member, Position(32000, 31000, 7));
	sweep();
	EXPECT_EQ(onlyPacket(member)["type"], "snapshot");
	EXPECT_EQ(onlyPacket(leader)["type"], "update");
}

TEST_F(PartyTrackingProtocolTest, InviteesOtherPartiesLegacyAndCastViewersNeverReceivePositionsWithoutConsent) {
	auto leader = makePlayer(31, "Leader");
	auto member = makePlayer(32, "Member");
	auto invitee = makePlayer(33, "Invitee");
	auto stranger = makePlayer(34, "Stranger");
	auto viewer = makePlayer(35, "Viewer");
	auto party = Party::create(leader);
	ASSERT_TRUE(party->invitePlayer(member));
	ASSERT_TRUE(party->joinParty(member));
	ASSERT_TRUE(party->invitePlayer(invitee));
	const auto otherParty = Party::create(stranger);
	asLegacy(member);
	asViewer(viewer);
	broadcastTo(leader, viewer);
	negotiate(leader);
	negotiate(invitee);
	negotiate(stranger);
	negotiate(viewer);
	sweep();
	const auto tracked = packets(leader);
	ASSERT_EQ(tracked.size(), 1U);
	ASSERT_EQ(tracked[0]["members"].size(), 2U);
	EXPECT_TRUE(packets(member).empty());
	EXPECT_TRUE(packets(viewer).empty());
	EXPECT_EQ(onlyPacket(invitee)["type"], "clear");
	EXPECT_EQ(onlyPacket(stranger)["members"].size(), 1U);
	// Explicit opt-in works independently of the numeric client version.
	negotiate(member);
	sweep();
	EXPECT_EQ(onlyPacket(member)["type"], "snapshot");
}

TEST_F(PartyTrackingProtocolTest, PartySwitchesAndRejoinBetweenSweepsAlwaysReplaceTheSnapshot) {
	auto leader = makePlayer(41, "Leader");
	auto member = makePlayer(42, "Member");
	auto anchor = makePlayer(43, "Anchor");
	negotiate(member);
	sweep();
	EXPECT_EQ(onlyPacket(member)["type"], "clear");
	auto party = Party::create(leader);
	ASSERT_TRUE(party->invitePlayer(anchor));
	ASSERT_TRUE(party->joinParty(anchor));
	ASSERT_TRUE(party->invitePlayer(member));
	ASSERT_TRUE(party->joinParty(member));
	sweep();
	const auto first = onlyPacket(member);
	EXPECT_EQ(first["type"], "snapshot");
	ASSERT_TRUE(party->leaveParty(member));
	ASSERT_TRUE(party->invitePlayer(member));
	ASSERT_TRUE(party->joinParty(member));
	sweep();
	const auto rejoined = onlyPacket(member);
	EXPECT_EQ(rejoined["type"], "snapshot");
	EXPECT_NE(rejoined["epoch"], first["epoch"]);
	ASSERT_TRUE(party->leaveParty(member));
	const auto newParty = Party::create(member);
	sweep();
	const auto switched = onlyPacket(member);
	EXPECT_EQ(switched["type"], "snapshot");
	EXPECT_EQ(switched["members"].size(), 1U);
	EXPECT_NE(switched["epoch"], rejoined["epoch"]);
}

TEST_F(PartyTrackingProtocolTest, ClosedConnectionsAndOneSidedMembershipCannotReceiveOrDisclosePositions) {
	auto leader = makePlayer(51, "Leader");
	auto member = makePlayer(52, "Member");
	auto outsider = makePlayer(53, "Outsider");
	auto party = Party::create(leader);
	ASSERT_TRUE(party->invitePlayer(member));
	ASSERT_TRUE(party->joinParty(member));
	// A mismatched Lua-owned pointer alone is not authorization to see the roster.
	outsider->setParty(party);
	negotiate(leader);
	negotiate(member);
	negotiate(outsider);
	sweep();
	EXPECT_EQ(onlyPacket(outsider)["type"], "clear");
	EXPECT_EQ(onlyPacket(leader)["members"].size(), 2U);
	member->getClient()->getConnection()->close(true);
	sweep();
	EXPECT_TRUE(packets(member).empty());
	EXPECT_EQ(onlyPacket(leader)["type"], "remove");
	outsider->setParty(nullptr);
}
