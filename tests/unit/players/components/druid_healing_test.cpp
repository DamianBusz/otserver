/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/players/components/druid_healing.hpp"
#include "creatures/players/components/mana_buffer.hpp"
#include "creatures/combat/combat.hpp"
#include "creatures/combat/condition.hpp"
#include "creatures/combat/spells.hpp"
#include "creatures/monsters/monster.hpp"
#include "creatures/monsters/monsters.hpp"
#include "creatures/players/player.hpp"
#include "creatures/players/grouping/groups.hpp"
#include "creatures/players/grouping/party.hpp"
#include "creatures/interactions/chat.hpp"
#include "creatures/players/vocations/vocation.hpp"
#include "config/configmanager.hpp"
#include "game/game.hpp"
#include "items/tile.hpp"
#include "items/weapons/weapons.hpp"
#include "io/io_wheel.hpp"
#include "lib/logging/in_memory_logger.hpp"
#include "lua/creature/events.hpp"
#include "lua/functions/creatures/combat/combat_functions.hpp"
#include "lua/functions/lua_functions_loader.hpp"
#include "lua/global/lua_variant.hpp"
#include "map/spectators.hpp"
#include "map/utils/mapsector.hpp"

TEST(DruidHealing, ExactThresholdsFractionalStagesAndSaturatedArithmetic) {
	for (uint8_t stage : { 1, 2, 3 }) {
		const int base = stage == 1 ? 500 : stage == 2 ? 750 : 1000;
		EXPECT_EQ(DruidHealing::groveBasisPoints(stage, 600, 1000), 0);
		EXPECT_EQ(DruidHealing::groveBasisPoints(stage, 599, 1000), base);
		EXPECT_EQ(DruidHealing::groveBasisPoints(stage, 300, 1000), base);
		EXPECT_EQ(DruidHealing::groveBasisPoints(stage, 299, 1000), 2 * base);
	}
	EXPECT_EQ(DruidHealing::groveBasisPoints(3, 1, 0), 0);
	EXPECT_EQ(DruidHealing::groveBasisPoints(0, 1, 1000), 0);
	EXPECT_EQ(DruidHealing::groveBasisPoints(3, INT32_MAX, INT32_MAX), 0);
	EXPECT_EQ(DruidHealing::boost(1000, 750), 1075);
	EXPECT_EQ(DruidHealing::fraction(333, 3000), 99);
	EXPECT_EQ(DruidHealing::boost(INT32_MAX, INT32_MAX), INT32_MAX);
	EXPECT_EQ(DruidHealing::fraction(INT32_MAX, INT32_MAX), INT32_MAX);
	EXPECT_EQ(DruidHealing::fraction(-10, 3000), 0);
}

TEST(ManaBuffer, LethalBoundariesAndSurchargeWindow) {
	ManaBufferState state;
	EXPECT_EQ(state.consume(99, 100, 10000, 10000, 1000).manaSpent, 0);
	auto first = state.consume(100, 100, 10000, 10000, 1000);
	EXPECT_TRUE(first.survived);
	EXPECT_EQ(first.manaSpent, 2510);
	auto next = state.consume(20, 1, 7490, 10000, 2999);
	EXPECT_TRUE(next.survived);
	EXPECT_EQ(next.manaSpent, 200);
	auto boundary = state.consume(20, 1, 7290, 10000, 3000);
	EXPECT_TRUE(boundary.survived);
	EXPECT_EQ(boundary.manaSpent, 2700);
	state.reset();
	EXPECT_EQ(state.consume(100, 100, 10000, 10001, 3001).manaSpent, 2511);
}

TEST(ManaBuffer, ExhaustedManaDoesNotPreventDeathAndExtremeDamageCannotWrap) {
	for (uint32_t mana : { 0, 2509, 2510 }) {
		ManaBufferState state;
		auto result = state.consume(100, 100, mana, 10000, 0);
		EXPECT_FALSE(result.survived);
		EXPECT_EQ(result.manaSpent, mana);
	}
	ManaBufferState state;
	EXPECT_TRUE(state.consume(100, 100, 2511, 10000, 0).survived);
	state.reset();
	auto extreme = state.consume(INT64_MAX, 1, UINT32_MAX, UINT32_MAX, UINT64_MAX - 1);
	EXPECT_FALSE(extreme.survived);
	EXPECT_EQ(extreme.manaSpent, UINT32_MAX);
	EXPECT_EQ(state.consume(1, 0, 1000, 1000, 0).manaSpent, 0);
}

class HealingTestInjector : public di::extension::injector<> {
public:
	void releaseLuaOwners() {
		cfg().data<Chat>().reset();
		cfg().data<Events>().reset();
		cfg().data<Spells>().reset();
		cfg().data<Monsters>().reset();
		cfg().data<Game>().reset();
	}
};

class DruidHealingGame : public ::testing::Test {
protected:
	static void SetUpTestSuite() {
		previous = DI::getTestContainer();
		InMemoryLogger::install(injector);
		DI::setTestContainer(&injector);
		g_configManager().setConfigFileLua("config.lua.dist");
		ASSERT_TRUE(g_configManager().load());
		ASSERT_TRUE(g_vocations().loadFromXml());
	}
	static void TearDownTestSuite() {
		// Close callback owners while their injected Lua environment is still alive,
		// before restoring the process-wide test container.
		injector.releaseLuaOwners();
		DI::setTestContainer(previous);
	}
	void SetUp() override {
		originalItemsSize = Item::items.getItems().size();
		if (originalItemsSize == 0) Item::items.getItems().resize(1);
		group = std::make_shared<Group>();
		group->flags[static_cast<size_t>(PlayerFlags_t::IsAlwaysPremium)] = true;
		caster = create(1, Position(100, 100, 7));
		primary = create(2, Position(101, 100, 7));
		secondary = create(3, Position(102, 100, 7));
	}
	void TearDown() override {
		Spectators::clearCache();
		if (g_game().getPlayerByID(caster->getID()) == caster) g_game().removePlayer(caster);
		for (const auto &creature : mappedCreatures) {
			if (const auto monster = creature->getMonster(); monster && g_game().getMonsterByID(monster->getID()) == monster) {
				g_game().removeMonster(monster);
			}
			const auto pos = creature->getPosition();
			g_game().map.getMapSector(pos.x, pos.y)->removeCreature(creature);
			creature->setMaster(nullptr);
			creature->setParent({});
		}
		for (const auto &[pos, previousTile] : mappedTiles) {
			g_game().map.getBestMapSector(pos.x, pos.y)->createFloor(pos.z)->setTile(pos.x, pos.y, previousTile);
		}
		mappedCreatures.clear();
		mappedTiles.clear();
		if (party) party->disband();
		for (const auto &player : players) player->setParent({});
		tiles.clear();
		players.clear();
		if (originalItemsSize == 0) Item::items.getItems().clear();
	}
	std::shared_ptr<Player> create(uint32_t guid, const Position &position) {
		auto player = std::make_shared<Player>();
		player->setName("HealingTest" + std::to_string(guid));
		player->setGroup(group);
		player->setLevel(100);
		EXPECT_TRUE(player->setVocation(VOCATION_ELDER_DRUID));
		player->setGUID(guid);
		player->setID();
		place(player, position);
		player->setVarStats(STAT_MAXHITPOINTS, 10000 - player->getMaxHealth());
		player->setVarStats(STAT_MAXMANAPOINTS, 10000 - player->getMaxMana());
		health(player, 1000);
		player->changeMana(10000);
		players.push_back(player);
		return player;
	}
	void place(const std::shared_ptr<Player> &player, const Position &position) {
		auto tile = std::make_shared<DynamicTile>(position);
		tile->addThing(player);
		tiles.push_back(tile);
	}
	void makeParty() {
		party = Party::create(caster);
		for (const auto &player : { primary, secondary }) {
			ASSERT_TRUE(party->invitePlayer(player));
			ASSERT_TRUE(party->joinParty(player));
		}
	}
	void health(const std::shared_ptr<Player> &player, int32_t value) { player->changeHealth(value - player->getHealth(), false); }
	std::shared_ptr<Tile> mapTile(const Position &pos) {
		mappedTiles.emplace_back(pos, g_game().map.getTile(pos));
		auto tile = std::make_shared<DynamicTile>(pos);
		g_game().map.getBestMapSector(pos.x, pos.y)->createFloor(pos.z)->setTile(pos.x, pos.y, tile);
		return tile;
	}
	void mapCreature(const std::shared_ptr<Creature> &creature, const Position &pos) {
		mapTile(pos)->addThing(creature);
		g_game().map.getMapSector(pos.x, pos.y)->addCreature(creature);
		mappedCreatures.push_back(creature);
		Spectators::clearCache();
	}
	std::shared_ptr<Monster> monster(const Position &pos) {
		auto type = std::make_shared<MonsterType>("ForkTest");
		type->info.health = type->info.healthMax = 10000;
		auto creature = std::make_shared<Monster>(type);
		creature->setID();
		mapCreature(creature, pos);
		return creature;
	}
	CombatDamage healing(int32_t amount = 1000) {
		CombatDamage damage;
		damage.primary.type = COMBAT_HEALING;
		damage.primary.value = amount;
		damage.origin = ORIGIN_SPELL;
		damage.instantSpellName = "Heal Friend";
		return damage;
	}
	inline static HealingTestInjector injector {};
	inline static decltype(DI::getTestContainer()) previous = nullptr;
	size_t originalItemsSize = 0;
	CreatureVector mappedCreatures;
	std::vector<std::pair<Position, std::shared_ptr<Tile>>> mappedTiles;
	std::shared_ptr<Group> group;
	std::shared_ptr<Player> caster, primary, secondary;
	std::shared_ptr<Party> party;
	std::vector<std::shared_ptr<Player>> players;
	std::vector<std::shared_ptr<Tile>> tiles;
};

TEST_F(DruidHealingGame, SelfBonusAffectsSpellsAndRunesButNotPotionsOrLinks) {
	ASSERT_TRUE(caster->setDruidStance(DruidStance::SharedConservation));
	for (int kind : { 0, 1, 2, 3 }) {
		health(caster, 1000);
		auto damage = healing();
		if (kind != 0) damage.instantSpellName.clear();
		if (kind == 1) damage.runeSpellName = "ultimate healing rune";
		if (kind == 3) damage.healingKind = HealingKind::HealingLink;
		ASSERT_TRUE(g_game().combatChangeHealth(caster, caster, damage));
		EXPECT_EQ(caster->getHealth(), kind <= 1 ? 2100 : 2000);
	}
}

TEST_F(DruidHealingGame, SharingAndLinkUseNominalHealingWithoutRecursiveBonuses) {
	makeParty();
	ASSERT_TRUE(caster->setDruidStance(DruidStance::SharedConservation));
	health(caster, 3000);
	health(secondary, 500);
	health(primary, 10000); // primary overheal does not suppress nominal sharing
	caster->wheel().addStat(WheelStat_t::HEALING, 100);
	auto damage = healing();
	damage.healingLink = 25;
	ASSERT_TRUE(g_game().combatChangeHealth(caster, primary, damage));
	EXPECT_EQ(primary->getHealth(), 10000);
	EXPECT_EQ(secondary->getHealth(), 830); // (1000 + 100) * 30%, flat bonus once
	EXPECT_EQ(caster->getHealth(), 3275); // (1000 + 100) * 25%, no self/flat bonus repeated
}

TEST_F(DruidHealingGame, LowestAbsoluteHealthIncludesCasterAndRejectsInvalidRecipients) {
	makeParty();
	ASSERT_TRUE(caster->setDruidStance(DruidStance::SharedConservation));
	health(caster, 500);
	health(secondary, 500); // GUID tie-break selects caster
	auto damage = healing();
	ASSERT_TRUE(g_game().combatChangeHealth(caster, primary, damage));
	EXPECT_EQ(caster->getHealth(), 830); // 300 plus 10% self heal
	EXPECT_EQ(secondary->getHealth(), 500);
	for (const auto &position : { Position(108, 100, 7), Position(100, 106, 7), Position(100, 100, 8) }) {
		place(secondary, position);
		health(caster, 10000);
		damage = healing();
		ASSERT_TRUE(g_game().combatChangeHealth(caster, primary, damage));
		EXPECT_EQ(secondary->getHealth(), 500);
	}
	place(secondary, Position(102, 100, 7));
	secondary->setParty(nullptr); // stale party-list member must be ignored
	damage = healing();
	ASSERT_TRUE(g_game().combatChangeHealth(caster, primary, damage));
	EXPECT_EQ(secondary->getHealth(), 500);
	secondary->setParty(party);
	secondary->setRemoved();
	damage = healing();
	ASSERT_TRUE(g_game().combatChangeHealth(caster, primary, damage));
	EXPECT_EQ(secondary->getHealth(), 500);
}

TEST_F(DruidHealingGame, GroveCriticalsApplyOnceIndependentlyToPrimaryAndSharedHealing) {
	makeParty();
	ASSERT_TRUE(caster->setDruidStance(DruidStance::SharedConservation));
	caster->wheel().setStage(WheelStage_t::BLESSING_OF_THE_GROVE, 2);
	caster->setVarSkill(SKILL_CRITICAL_HIT_CHANCE, 10000);
	caster->setVarSkill(SKILL_CRITICAL_HIT_DAMAGE, 5000 - caster->getSkillLevel(SKILL_CRITICAL_HIT_DAMAGE)
		- static_cast<int32_t>(caster->getBaseCritical().damage * 10000));
	health(caster, 9000);
	health(primary, 4000);
	health(secondary, 4000);
	auto damage = healing();
	damage.healingLink = 25;
	damage.criticalChance = 10000; // bypass the configured equipment chance cap, retaining independent rolls
	ASSERT_TRUE(g_game().combatChangeHealth(caster, primary, damage));
	EXPECT_TRUE(damage.critical);
	EXPECT_EQ(primary->getHealth(), 5612); // floor(1000 * 1.075) then +50%
	EXPECT_EQ(secondary->getHealth(), 4483); // floor(300 * 1.075) then +50%, no inherited crit
	EXPECT_EQ(caster->getHealth(), 9403); // final primary *25%, no second critical
}

TEST_F(DruidHealingGame, ManaBufferPreventsLethalDamageForBothMageVocations) {
	for (auto vocation : { VOCATION_DRUID, VOCATION_ELDER_DRUID, VOCATION_SORCERER, VOCATION_MASTER_SORCERER, VOCATION_ROYAL_PALADIN }) {
		ASSERT_TRUE(caster->setDruidStance(DruidStance::None));
		ASSERT_TRUE(caster->setVocation(vocation));
		health(caster, 100);
		caster->changeMana(10000);
		const auto mana = caster->getMana();
		const bool mage = vocation != VOCATION_ROYAL_PALADIN;
		EXPECT_EQ(caster->applyManaBuffer(nullptr, 101, 1000), mage);
		EXPECT_EQ(caster->getMana(), mage ? mana - 2520 : mana);
	}
	ASSERT_TRUE(caster->setVocation(VOCATION_ELDER_DRUID));
	health(caster, 100);
	caster->changeMana(10000);
	CombatDamage damage;
	damage.primary.type = COMBAT_ICEDAMAGE; // avoid unrelated item-splash assets in this isolated fixture
	damage.primary.value = -100;
	damage.origin = ORIGIN_NONE;
	ASSERT_TRUE(g_game().combatChangeHealth(nullptr, caster, damage));
	EXPECT_EQ(caster->getHealth(), 1);
	EXPECT_EQ(caster->getMana(), 7490);
}

TEST_F(DruidHealingGame, SharedHealingRevalidatesOriginalPartyAndRangeAtApplication) {
	makeParty();
	ASSERT_TRUE(caster->setDruidStance(DruidStance::SharedConservation));
	auto damage = healing(300);
	damage.healingKind = HealingKind::SharedConservation;
	damage.sharedHealingParty = party;
	secondary->setParty(nullptr);
	EXPECT_FALSE(g_game().combatChangeHealth(caster, secondary, damage));
	secondary->setParty(party);
	place(secondary, Position(108, 100, 7));
	EXPECT_FALSE(g_game().combatChangeHealth(caster, secondary, damage));
	place(secondary, Position(102, 100, 7));
	secondary->setRemoved();
	EXPECT_FALSE(g_game().combatChangeHealth(caster, secondary, damage));
	EXPECT_EQ(secondary->getHealth(), 1000);
}

TEST_F(DruidHealingGame, ExhaustedManaBufferAllowsGiftOfLifeAndAppliesEntireHit) {
	health(caster, 100);
	caster->changeMana(-10000);
	caster->changeMana(100); // insufficient for either converted damage or surcharge
	caster->wheel().setStage(WheelStage_t::GIFT_OF_LIFE, 1);
	CombatDamage damage;
	damage.primary.type = COMBAT_ICEDAMAGE;
	damage.primary.value = -100;
	damage.secondary.type = COMBAT_EARTHDAMAGE;
	damage.secondary.value = -100;
	ASSERT_TRUE(g_game().combatChangeHealth(nullptr, caster, damage));
	EXPECT_EQ(caster->getHealth(), 1900); // 100 + Gift's 2000 - the complete 200 damage
	EXPECT_EQ(caster->getMana(), 2000); // drained 100, then Gift restores 20%
	EXPECT_GT(caster->wheel().getGiftOfCooldown(), 0);
}

class ManaBoundaryCreature : public Creature {
public:
	ManaBoundaryCreature() { mana = UINT32_MAX; }
	uint32_t getMaxMana() const override { return UINT32_MAX; }
	const std::string &getName() const override { return name; }
	const std::string &getTypeName() const override { return name; }
	const std::string &getNameDescription() const override { return name; }
	std::string getDescription(int32_t) override { return name; }
	CreatureType_t getType() const override { return CREATURETYPE_MONSTER; }
	void setID() override { }
	void addList() override { }
	void removeList() override { }
private:
	std::string name = "ManaBoundary";
};

TEST_F(DruidHealingGame, ManaChangesRemainBoundedAcrossSignedBoundary) {
	auto player = std::make_shared<ManaBoundaryCreature>();
	player->changeMana(-1);
	EXPECT_EQ(player->getMana(), UINT32_MAX - 1);
	player->changeMana(INT32_MIN);
	EXPECT_EQ(player->getMana(), INT32_MAX - 1);
	player->changeMana(INT32_MIN);
	EXPECT_EQ(player->getMana(), 0);
	player->changeMana(INT32_MAX);
	player->changeMana(INT32_MAX);
	player->changeMana(INT32_MAX);
	EXPECT_EQ(player->getMana(), UINT32_MAX);
}

TEST_F(DruidHealingGame, FractionalWheelDamageSurvivesRuntimeAggregation) {
	WheelSpells::Bonus bonus;
	bonus.increase.damage = 6.5;
	caster->wheel().addSpellBonus("Terra Wave", bonus);
	CombatDamage damage;
	damage.primary.type = COMBAT_EARTHDAMAGE;
	damage.primary.value = -1000;
	damage.damageMultiplier = caster->wheel().getSpellBonus("Terra Wave", WheelSpellBoost_t::DAMAGE);
	g_game().applyWheelOfDestinyEffectsToDamage(damage, caster, primary);
	EXPECT_DOUBLE_EQ(damage.damageMultiplier, 6.5);
	EXPECT_EQ(damage.primary.value, -1065);
}

TEST_F(DruidHealingGame, RingShieldCostsTwiceManaWithoutChangingSpellShields) {
	ASSERT_TRUE(caster->setVocation(VOCATION_ROYAL_PALADIN));
	for (const auto conditionId : { CONDITIONID_DEFAULT, CONDITIONID_RING }) {
		for (const int mana : { 100, 101, 1000 }) {
			health(caster, 1000);
			caster->changeMana(mana - static_cast<int32_t>(caster->getMana()));
			auto shield = Condition::createCondition(conditionId, CONDITION_MANASHIELD, 60000);
			ASSERT_TRUE(caster->addCondition(shield));
			CombatDamage damage;
			damage.primary.type = COMBAT_ICEDAMAGE;
			damage.primary.value = -70;
			damage.secondary.type = COMBAT_EARTHDAMAGE;
			damage.secondary.value = -30;
			ASSERT_TRUE(g_game().combatChangeHealth(nullptr, caster, damage));
			const int ratio = conditionId == CONDITIONID_RING ? 2 : 1;
			const int absorbed = std::min(100, mana / ratio);
			EXPECT_EQ(caster->getHealth(), 900 + absorbed);
			EXPECT_EQ(caster->getMana(), mana - absorbed * ratio);
			caster->removeCondition(CONDITION_MANASHIELD);
		}
	}
	health(caster, 1000);
	caster->changeMana(100 - static_cast<int32_t>(caster->getMana()));
	auto shield = Condition::createCondition(CONDITIONID_DEFAULT, CONDITION_MANASHIELD, 60000);
	ASSERT_TRUE(shield->setParam(CONDITION_PARAM_MANASHIELD, 30));
	ASSERT_TRUE(caster->addCondition(shield));
	CombatDamage damage;
	damage.primary.type = COMBAT_ICEDAMAGE;
	damage.primary.value = -100;
	ASSERT_TRUE(g_game().combatChangeHealth(nullptr, caster, damage));
	EXPECT_EQ(caster->getHealth(), 930);
	EXPECT_EQ(caster->getMana(), 70);
	EXPECT_FALSE(caster->hasCondition(CONDITION_MANASHIELD));
}

TEST_F(DruidHealingGame, RodsCanAttackAtZeroManaAndStillRespectRange) {
	ItemType rodType;
	rodType.id = 3066;
	rodType.weaponType = WEAPON_WAND;
	WeaponWand rod;
	rod.configureWeapon(rodType);
	rod.setMana(13);
	caster->changeMana(-10000);
	EXPECT_EQ(rod.playerWeaponCheck(caster, primary, 7), 100);
	ASSERT_TRUE(rod.useWeapon(caster, std::make_shared<Item>(0), primary));
	EXPECT_EQ(caster->getMana(), 13);
	caster->changeMana(-13);
	place(primary, Position(108, 100, 7));
	EXPECT_EQ(rod.playerWeaponCheck(caster, primary, 7), 0);
	place(primary, Position(107, 100, 7));
	EXPECT_EQ(rod.playerWeaponCheck(caster, primary, 7), 100);
	ItemType meleeType;
	meleeType.weaponType = WEAPON_SWORD;
	WeaponMelee melee;
	melee.configureWeapon(meleeType);
	melee.setMana(13);
	EXPECT_EQ(melee.playerWeaponCheck(caster, primary, 7), 0);
}

TEST_F(DruidHealingGame, ForkedAcquisitionRejectsWallsFloorsSummonsAndProtectedPlayers) {
	mapCreature(caster, Position(100, 100, 7));
	auto near = monster(Position(101, 100, 7));
	auto far = monster(Position(103, 100, 7));
	auto above = monster(Position(100, 100, 6));
	auto outside = monster(Position(108, 100, 7));
	EXPECT_EQ(Combat::selectForkedTarget(caster, nullptr, true), near);
	EXPECT_EQ(Combat::selectForkedTarget(caster, far, true), far);
	EXPECT_EQ(Combat::selectForkedTarget(caster, nullptr, false), nullptr);
	EXPECT_EQ(Combat::selectForkedTarget(caster, above, true), nullptr);
	EXPECT_EQ(Combat::selectForkedTarget(caster, outside, true), nullptr);
	ASSERT_TRUE(near->setMaster(caster));
	EXPECT_EQ(Combat::selectForkedTarget(caster, near, true), nullptr);
	EXPECT_EQ(Combat::selectForkedTarget(caster, nullptr, true), far);
	mapCreature(primary, Position(100, 101, 7));
	primary->setLevel(1);
	EXPECT_EQ(Combat::selectForkedTarget(caster, primary, true), nullptr);
	far->getTile()->setFlag(TILESTATE_PROTECTIONZONE);
	EXPECT_EQ(Combat::selectForkedTarget(caster, far, true), nullptr);
	far->getTile()->resetFlag(TILESTATE_PROTECTIONZONE);
	const bool previousBlock = Item::items.getItems()[0].blockProjectile;
	Item::items.getItems()[0].blockProjectile = true;
	mapTile(Position(102, 100, 7))->addThing(std::make_shared<Item>(0));
	EXPECT_EQ(Combat::selectForkedTarget(caster, far, true), nullptr);
	Item::items.getItems()[0].blockProjectile = previousBlock;
	far->setRemoved();
	EXPECT_EQ(Combat::selectForkedTarget(caster, far, true), nullptr);
	EXPECT_EQ(Combat::selectForkedTarget(caster, nullptr, true), nullptr);
}

TEST_F(DruidHealingGame, ForkedDamageUsesFixedOriginUniqueTargetsAndWheelLimits) {
	ASSERT_TRUE(g_game().getIOWheel()->initializeGlobalData());
	mapCreature(caster, Position(100, 100, 7));
	auto first = monster(Position(103, 100, 7));
	std::vector<std::shared_ptr<Monster>> targets { first };
	for (int y = 97; y <= 103; ++y) targets.push_back(monster(Position(104, y, 7)));
	auto edge = monster(Position(107, 104, 7));
	auto beyond = monster(Position(108, 104, 7));
	targets.push_back(edge);
	for (const auto &name : { "Forked Glacier", "Forked Thorns" }) {
		for (const int extra : { 0, 1 }) {
			for (const auto &target : targets) target->changeHealth(10000 - target->getHealth());
			Combat combat;
			combat.setInstantSpellName(name);
			combat.setParam(COMBAT_PARAM_TYPE, COMBAT_ICEDAMAGE);
			combat.setPlayerCombatValues(COMBAT_FORMULA_DAMAGE, -100, 0, -100, 0);
			if (extra) {
				caster->wheel().upgradeSpell(name);
				caster->wheel().upgradeSpell(name);
			}
			ASSERT_TRUE(combat.doCombatForked(caster, first));
			int hit = 0;
			for (const auto &target : targets) {
				const auto lost = 10000 - target->getHealth();
				if (lost > 0) ++hit;
				EXPECT_LE(lost, 100); // no duplicate hit; mitigation may reduce damage
			}
			EXPECT_EQ(hit, (std::string_view(name) == "Forked Glacier" ? 7 : 6) + extra);
			EXPECT_LT(first->getHealth(), 10000);
			EXPECT_EQ(beyond->getHealth(), 10000); // near a later target, outside the original fork radius
		}
	}
}

TEST_F(DruidHealingGame, AutoAttackWithoutMainTargetSuppressesCharmsButKeepsOrdinaryCriticals) {
	CombatParams params;
	for (const auto origin : { ORIGIN_MELEE, ORIGIN_RANGED, ORIGIN_FIST, ORIGIN_SPELL }) {
		CombatDamage damage;
		damage.origin = origin;
		damage.primary.type = COMBAT_ICEDAMAGE;
		damage.primary.value = -100;
		damage.criticalChance = 10000;
		damage.criticalDamage = 5000;
		Combat::applyExtensions(caster, { primary, secondary }, damage, params);
		const auto main = primary->getCombatDamage();
		const auto splash = secondary->getCombatDamage();
		EXPECT_EQ(main.suppressCharms, origin == ORIGIN_MELEE || origin == ORIGIN_RANGED || origin == ORIGIN_FIST);
		EXPECT_EQ(splash.suppressCharms, origin == ORIGIN_MELEE || origin == ORIGIN_RANGED || origin == ORIGIN_FIST);
		EXPECT_TRUE(main.critical);
		EXPECT_TRUE(splash.critical);
		EXPECT_EQ(main.primary.value, splash.primary.value);
		EXPECT_LT(main.primary.value, -100);
	}
	caster->setAttackedCreature(nullptr);
}

TEST_F(DruidHealingGame, LuaNumericTargetPropagatesRejectedForkResult) {
	mapCreature(caster, Position(100, 100, 7));
	g_game().addPlayer(caster);
	auto target = monster(Position(101, 100, 7));
	g_game().addMonster(target);
	ASSERT_EQ(g_game().getCreatureByID(caster->getID()), caster);
	ASSERT_EQ(g_game().getCreatureByID(target->getID()), target);
	ASSERT_EQ(Combat::selectForkedTarget(caster, target, false), target);
	std::unique_ptr<lua_State, decltype(&lua_close)> state { luaL_newstate(), &lua_close };
	auto L = state.get();
	CombatFunctions::init(L);
	auto combat = std::make_shared<Combat>();
	combat->setParam(COMBAT_PARAM_TYPE, COMBAT_ICEDAMAGE);
	combat->setPlayerCombatValues(COMBAT_FORMULA_DAMAGE, -100, 0, -100, 0);
	LuaVariant variant;
	variant.type = VARIANT_NUMBER;
	variant.number = target->getID();
	variant.instantName = "Forked Glacier";
	for (bool removed : { false, true }) {
		if (removed) target->setRemoved();
		lua_getglobal(L, "Combat");
		lua_getfield(L, -1, "execute");
		lua_remove(L, -2);
		Lua::pushSharedUserdata<Combat>(L, combat);
		lua_pushnumber(L, caster->getID());
		Lua::pushVariant(L, variant);
		ASSERT_EQ(lua_pcall(L, 3, 1, 0), LUA_OK) << lua_tostring(L, -1);
		ASSERT_TRUE(lua_isboolean(L, -1));
		EXPECT_EQ(lua_toboolean(L, -1) != 0, !removed);
		lua_pop(L, 1);
	}
	EXPECT_LT(target->getHealth(), 10000);
}

TEST_F(DruidHealingGame, RejectedForkSelectionPreservesManaAndBothCooldowns) {
	mapCreature(caster, Position(100, 100, 7));
	std::string param;
	for (uint16_t id : { 317, 318 }) {
		InstantSpell spell;
		spell.setSpellId(id);
		spell.setName(id == 317 ? "Forked Glacier" : "Forked Thorns");
		spell.setMana(180);
		spell.setLevel(id == 317 ? 90 : 80);
		spell.setPremium(true);
		spell.setNeedTarget(true);
		spell.setRange(7);
		spell.setGroup(SPELLGROUP_ATTACK);
		spell.setCooldown(6000);
		spell.setGroupCooldown(2000);
		spell.addVocMap(VOCATION_ELDER_DRUID, true);
		caster->learnInstantSpell(spell.getName());
		const auto mana = caster->getMana();
		for (uint32_t level : { 150, 2000 }) {
			caster->setLevel(level);
			EXPECT_FALSE(spell.playerCastInstant(caster, param));
			EXPECT_EQ(caster->getMana(), mana);
			EXPECT_FALSE(caster->hasCondition(CONDITION_SPELLCOOLDOWN, id));
			EXPECT_FALSE(caster->hasCondition(CONDITION_SPELLGROUPCOOLDOWN, SPELLGROUP_ATTACK));
		}
	}

	// A legal nearby target must not replace an explicitly aimed empty tile.
	auto target = monster(Position(101, 100, 7));
	ASSERT_EQ(Combat::selectForkedTarget(caster, nullptr, true), target);
	const Position emptyAim(100, 101, 7);
	mapTile(emptyAim);
	InstantSpell spell;
	spell.setSpellId(317);
	spell.setName("Forked Glacier");
	spell.setMana(180);
	spell.setNeedTarget(true);
	spell.setGroup(SPELLGROUP_ATTACK);
	spell.setCooldown(6000);
	spell.setGroupCooldown(2000);
	const auto mana = caster->getMana();
	EXPECT_FALSE(spell.playerCastInstant(caster, param, &emptyAim));
	EXPECT_EQ(caster->getMana(), mana);
	EXPECT_FALSE(caster->hasCondition(CONDITION_SPELLCOOLDOWN, 317));
	EXPECT_FALSE(caster->hasCondition(CONDITION_SPELLGROUPCOOLDOWN, SPELLGROUP_ATTACK));

	// Positive accounting control: the fixture does charge normal completed casts.
	spell.postCastSpell(caster);
	EXPECT_EQ(caster->getMana(), mana - 180);
	EXPECT_TRUE(caster->hasCondition(CONDITION_SPELLCOOLDOWN, 317));
	EXPECT_TRUE(caster->hasCondition(CONDITION_SPELLGROUPCOOLDOWN, SPELLGROUP_ATTACK));
}

TEST_F(DruidHealingGame, LegacyFightModeRequestsCannotChangeRebalancedCombatStats) {
	for (uint16_t vocation : { VOCATION_ELDER_DRUID, VOCATION_MASTER_SORCERER, VOCATION_ROYAL_PALADIN, VOCATION_ELITE_KNIGHT, VOCATION_EXALTED_MONK }) {
		ASSERT_TRUE(caster->setVocation(vocation));
		const auto defense = caster->getDefense();
		const auto mitigation = caster->getMitigation();
		for (const auto mode : { FIGHTMODE_ATTACK, FIGHTMODE_BALANCED, FIGHTMODE_DEFENSE }) {
			caster->setFightMode(mode);
			EXPECT_FLOAT_EQ(caster->getAttackFactor(), 1.0f);
			EXPECT_FLOAT_EQ(caster->getDefenseFactor(false), 1.0f);
			EXPECT_FLOAT_EQ(caster->getDefenseFactor(true), 1.0f);
			EXPECT_EQ(caster->getDefense(), defense);
			EXPECT_FLOAT_EQ(caster->getMitigation(), mitigation);
			EXPECT_DOUBLE_EQ(caster->getCombatTacticsMitigation(), 1.0);
			EXPECT_EQ(caster->attackTotal(20, 50, 100), 242);
			EXPECT_EQ(Weapons::getMaxWeaponDamage(150, 100, 50, caster->getAttackFactor(), true), 455);
			EXPECT_EQ(caster->attackTotal(UINT16_MAX, UINT16_MAX, UINT16_MAX), UINT16_MAX);
			EXPECT_EQ(caster->attackRawTotal(UINT16_MAX, UINT16_MAX, UINT16_MAX), UINT16_MAX);
		}
	}
}
