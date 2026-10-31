/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/players/components/knight_stance.hpp"
#include "creatures/players/player.hpp"
#include "creatures/players/grouping/groups.hpp"
#include "creatures/players/vocations/vocation.hpp"
#include "creatures/combat/condition.hpp"
#include "creatures/combat/combat.hpp"
#include "creatures/combat/spells.hpp"
#include "io/io_wheel.hpp"
#include "config/configmanager.hpp"
#include "game/game.hpp"
#include "items/tile.hpp"
#include "map/spectators.hpp"
#include "lua/creature/events.hpp"
#include "lua/functions/creatures/creature_functions.hpp"
#include "lua/functions/core/game/lua_enums.hpp"
#include "lua/functions/lua_functions_loader.hpp"
#include "lib/logging/in_memory_logger.hpp"

TEST(KnightStance, SkillAndDamageArithmeticIsBounded) {
	EXPECT_EQ(KnightStances::skill(KnightStance::BloodRage, SKILL_SWORD, 119), 148);
	EXPECT_EQ(KnightStances::skill(KnightStance::Protector, SKILL_SHIELD, 119), 154);
	EXPECT_EQ(KnightStances::skill(KnightStance::BloodRage, SKILL_SHIELD, 119), 119);
	EXPECT_EQ(KnightStances::skill(KnightStance::Protector, SKILL_AXE, 119), 119);
	EXPECT_EQ(KnightStances::skill(KnightStance::BloodRage, SKILL_SWORD, INT64_MAX), UINT16_MAX);
	EXPECT_EQ(KnightStances::skill(KnightStance::BloodRage, SKILL_SWORD, -100), 0);
	EXPECT_EQ(KnightStances::scaleDamage(INT32_MAX, 115), INT32_MAX);
	EXPECT_EQ(KnightStances::scaleDamage(INT32_MIN, 115), 0);
}

class KnightTestInjector : public di::extension::injector<> {
public:
	void releaseRuntimeOwners() {
		cfg().data<Events>().reset();
		cfg().data<Game>().reset();
	}
};

class KnightStancePlayer : public ::testing::Test {
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
		injector.releaseRuntimeOwners();
		DI::setTestContainer(previous);
	}
	void SetUp() override {
		originalItemsSize = Item::items.getItems().size();
		if (originalItemsSize == 0) Item::items.getItems().resize(1);
		group = std::make_shared<Group>();
		group->flags[static_cast<size_t>(PlayerFlags_t::IsAlwaysPremium)] = true;
		player = makePlayer(Position(100, 100, 7));
		target = makePlayer(Position(101, 100, 7));
	}
	std::shared_ptr<Player> makePlayer(const Position &position) {
		auto value = std::make_shared<Player>();
		value->setGroup(group);
		value->setLevel(200);
		EXPECT_TRUE(value->setVocation(VOCATION_ELITE_KNIGHT));
		value->setVarStats(STAT_MAXHITPOINTS, 10000 - value->getMaxHealth());
		value->changeHealth(10000 - value->getHealth(), false);
		auto tile = std::make_shared<DynamicTile>(position);
		tile->addThing(value);
		tiles.push_back(tile);
		return value;
	}
	void TearDown() override {
		Spectators::clearCache();
		player->setParent({});
		target->setParent({});
		tiles.clear();
		if (originalItemsSize == 0) Item::items.getItems().clear();
	}
	inline static KnightTestInjector injector {};
	inline static di::extension::injector<> *previous = nullptr;
	std::shared_ptr<Group> group;
	std::shared_ptr<Player> player, target;
	std::vector<std::shared_ptr<Tile>> tiles;
	size_t originalItemsSize = 0;
};

TEST_F(KnightStancePlayer, TotalSkillsRecomputeWithoutCompoundingAndClearOnDemotion) {
	for (auto type : { SKILL_SWORD, SKILL_AXE, SKILL_CLUB, SKILL_SHIELD }) {
		player->setVarSkill(type, 120 - player->getSkillLevel(type));
	}
	ASSERT_TRUE(player->setKnightStance(KnightStance::BloodRage));
	for (auto type : { SKILL_SWORD, SKILL_AXE, SKILL_CLUB }) EXPECT_EQ(player->getSkillLevel(type), 150);
	EXPECT_EQ(player->getSkillLevel(SKILL_SHIELD), 120);
	player->setVarSkill(SKILL_SWORD, 4);
	EXPECT_EQ(player->getSkillLevel(SKILL_SWORD), 155);
	ASSERT_TRUE(player->setKnightStance(KnightStance::BloodRage));
	EXPECT_EQ(player->getSkillLevel(SKILL_SWORD), 155);
	ASSERT_TRUE(player->setKnightStance(KnightStance::Protector));
	EXPECT_EQ(player->getSkillLevel(SKILL_SWORD), 124);
	EXPECT_EQ(player->getSkillLevel(SKILL_SHIELD), 156);
	ASSERT_TRUE(player->setVocation(VOCATION_KNIGHT));
	EXPECT_EQ(player->getKnightStance(), KnightStance::None);
	EXPECT_EQ(player->getSkillLevel(SKILL_SHIELD), 120);
	EXPECT_FALSE(player->setKnightStance(KnightStance::Protector));
}

TEST_F(KnightStancePlayer, EligibilityRejectsInvalidSelectionWithoutChangingState) {
	player->setLevel(19);
	EXPECT_FALSE(player->setKnightStance(KnightStance::BloodRage));
	player->setLevel(20);
	group->flags[static_cast<size_t>(PlayerFlags_t::IsAlwaysPremium)] = false;
	EXPECT_FALSE(player->setKnightStance(KnightStance::Protector));
	group->flags[static_cast<size_t>(PlayerFlags_t::IsAlwaysPremium)] = true;
	ASSERT_TRUE(player->setKnightStance(KnightStance::BloodRage));
	EXPECT_FALSE(player->setKnightStance(static_cast<KnightStance>(255)));
	EXPECT_EQ(player->getKnightStance(), KnightStance::BloodRage);
	ASSERT_TRUE(player->setVocation(VOCATION_ELDER_DRUID));
	EXPECT_EQ(player->getKnightStance(), KnightStance::None);
	EXPECT_FALSE(player->setKnightStance(KnightStance::Protector));
	EXPECT_TRUE(player->setKnightStance(KnightStance::None));
}

TEST_F(KnightStancePlayer, SelectingStanceRemovesSavedLegacySkillAndDamageBuff) {
	auto legacy = Condition::createCondition(CONDITIONID_COMBAT, CONDITION_ATTRIBUTES, 10000, 0, false,
		static_cast<uint32_t>(AttrSubId_t::BloodRageProtector));
	ASSERT_NE(legacy, nullptr);
	legacy->setParam(CONDITION_PARAM_SKILL_MELEEPERCENT, 135);
	legacy->setParam(CONDITION_PARAM_BUFF_DAMAGERECEIVED, 115);
	ASSERT_TRUE(player->addCondition(legacy));
	ASSERT_EQ(player->getBuff(BUFF_DAMAGERECEIVED), 115);
	ASSERT_TRUE(player->setKnightStance(KnightStance::BloodRage));
	EXPECT_EQ(player->getBuff(BUFF_DAMAGERECEIVED), 100);
	EXPECT_EQ(player->getCondition(CONDITION_ATTRIBUTES, CONDITIONID_COMBAT,
		static_cast<uint32_t>(AttrSubId_t::BloodRageProtector)), nullptr);
}

TEST_F(KnightStancePlayer, DrainAppliesEachStanceOnceToBothComponentsAndNeverToHealing) {
	for (auto source : { KnightStance::None, KnightStance::BloodRage, KnightStance::Protector }) {
		for (auto recipient : { KnightStance::None, KnightStance::BloodRage, KnightStance::Protector }) {
			ASSERT_TRUE(player->setKnightStance(source));
			ASSERT_TRUE(target->setKnightStance(recipient));
			CombatDamage damage;
			damage.origin = ORIGIN_MELEE;
			damage.primary.type = COMBAT_PHYSICALDAMAGE;
			damage.primary.value = -1000;
			damage.secondary.type = COMBAT_FIREDAMAGE;
			damage.secondary.value = -1000;
			const int expected = (1000 * KnightStances::outgoingPercent(source) / 100) * KnightStances::incomingPercent(recipient) / 100;
			const auto before = target->getHealth();
			ASSERT_TRUE(g_game().combatChangeHealth(player, target, damage));
			EXPECT_EQ(before - target->getHealth(), expected * 2);
			target->changeHealth(before - target->getHealth(), false);
		}
	}
	target->changeHealth(-1000, false);
	CombatDamage heal;
	heal.primary.type = COMBAT_HEALING;
	heal.primary.value = 500;
	const auto before = target->getHealth();
	ASSERT_TRUE(g_game().combatChangeHealth(player, target, heal));
	EXPECT_EQ(target->getHealth() - before, 500);
}

TEST(KnightShield, ExpiryRefreshAndSaturatingDeadline) {
	AttackReductionState state;
	state.apply(50, 100);
	EXPECT_EQ(state.snapshot(10099), 50);
	EXPECT_EQ(state.snapshot(10099), 0);
	state.apply(50, 100);
	EXPECT_EQ(state.snapshot(10100), 0);
	state.apply(75, 100);
	state.apply(50, 9000);
	EXPECT_EQ(state.snapshot(18000), 75);
	state.apply(75, 100);
	state.apply(50, 10100);
	EXPECT_EQ(state.snapshot(10100), 50);
	state.apply(50, UINT64_MAX - 10);
	EXPECT_EQ(state.snapshot(UINT64_MAX - 1), 50);
	state.apply(75, UINT64_MAX - 10);
	EXPECT_EQ(state.snapshot(UINT64_MAX), 0);
	state.apply(99, 100);
	EXPECT_EQ(state.snapshot(101), 0);
}

TEST(KnightShield, EntireSwingUsesOneSnapshotAndFailedAttemptsPreserveIt) {
	AttackReductionState state;
	state.apply(50, 100);
	{
		AttackReductionState::Swing failed(state, 101);
		failed.finish(false);
	}
	{
		AttackReductionState::Swing swing(state, 102);
		EXPECT_EQ(state.snapshot(102), 50);
		state.apply(75, 103); // A new suppression during callbacks belongs to the next attack.
		EXPECT_EQ(state.snapshot(104), 50);
		swing.finish(true);
	}
	EXPECT_EQ(state.snapshot(105), 75);
	state.apply(50, 106);
	{
		AttackReductionState::Swing miss(state, 107);
		miss.finish(true);
	}
	EXPECT_EQ(state.snapshot(108), 0);
}

TEST(KnightShield, NestedAttacksAndInvalidationCannotRestoreStaleState) {
	AttackReductionState state;
	state.apply(50, 100);
	{
		AttackReductionState::Swing outer(state, 101);
		EXPECT_EQ(state.snapshot(101), 50);
		state.apply(75, 102);
		{
			AttackReductionState::Swing nested(state, 103);
			EXPECT_EQ(state.snapshot(103), 75);
		}
		EXPECT_EQ(state.snapshot(104), 50);
		{
			AttackReductionState::Swing nested(state, 105);
			state.clear(); // Death or ownership transfer invalidates every active scope.
		}
		EXPECT_EQ(state.snapshot(106), 0);
	}
	EXPECT_EQ(state.snapshot(107), 0);
}

TEST_F(KnightStancePlayer, ShieldDefenseRequiresShieldAndIncludesOnlyWeaponDefenseModifier) {
	const auto savedSize = Item::items.getItems().size();
	const auto shieldId = static_cast<uint16_t>(savedSize);
	const auto weaponId = static_cast<uint16_t>(savedSize + 1);
	Item::items.getItems().resize(savedSize + 2);
	auto &shieldType = Item::items.getItems()[shieldId];
	shieldType.id = shieldId;
	shieldType.weaponType = WEAPON_SHIELD;
	shieldType.m_primaryType = "shields";
	shieldType.defense = 55;
	auto &weaponType = Item::items.getItems()[weaponId];
	weaponType.id = weaponId;
	weaponType.weaponType = WEAPON_SWORD;
	weaponType.defense = 100;
	weaponType.extraDefense = 3;
	EXPECT_FALSE(player->getShieldSpellDefense().has_value());
	const auto shield = Item::CreateItem(shieldId);
	const auto weapon = Item::CreateItem(weaponId);
	ASSERT_NE(shield, nullptr);
	ASSERT_NE(weapon, nullptr);
	std::static_pointer_cast<Cylinder>(player)->internalAddThing(CONST_SLOT_RIGHT, shield);
	std::static_pointer_cast<Cylinder>(player)->internalAddThing(CONST_SLOT_LEFT, weapon);
	EXPECT_EQ(player->getShieldSpellDefense(), 58);
	player->wheel().setStage(WheelStage_t::COMBAT_MASTERY, 3);
	player->wheel().setSpellInstant("Battle Healing", true);
	player->changeHealth(5000-player->getHealth(),false);
	EXPECT_EQ(player->wheel().getBattleHealingPercent(), 20);
	EXPECT_DOUBLE_EQ(player->wheel().getCombatMasteryPercent(*player,true),10);
	EXPECT_DOUBLE_EQ(player->wheel().getCombatMasteryPercent(*player,false),5);
	weaponType.slotPosition |= SLOTP_TWO_HAND;
	EXPECT_DOUBLE_EQ(player->wheel().getCombatMasteryPercent(*player,false),10);
	weaponType.slotPosition &= ~SLOTP_TWO_HAND;
	const auto defense = player->getDefenseEquipment();
	const auto critical = player->getSkillLevel(SKILL_CRITICAL_HIT_DAMAGE);
	player->wheel().setMajorStat(WheelMajor_t::DEFENSE,999);
	player->wheel().setMajorStat(WheelMajor_t::CRITICAL_DMG_2,999);
	EXPECT_EQ(player->getDefenseEquipment(),defense);
	EXPECT_EQ(player->getSkillLevel(SKILL_CRITICAL_HIT_DAMAGE),critical);
	shield->setAttribute(ItemAttribute_t::DEFENSE, INT32_MAX);
	weapon->setAttribute(ItemAttribute_t::EXTRADEFENSE, INT32_MAX);
	EXPECT_EQ(player->getShieldSpellDefense(), INT32_MAX);
	shield->setAttribute(ItemAttribute_t::DEFENSE, 1);
	weapon->setAttribute(ItemAttribute_t::EXTRADEFENSE, -3);
	EXPECT_EQ(player->getShieldSpellDefense(), 0);
	shieldType.m_primaryType = "spellbooks";
	EXPECT_EQ(player->wheel().getBattleHealingPercent(),10);
	EXPECT_DOUBLE_EQ(player->wheel().getCombatMasteryPercent(*player,true),5);
	EXPECT_FALSE(player->getShieldSpellDefense().has_value());
	std::static_pointer_cast<Cylinder>(player)->removeThing(shield, 1);
	std::static_pointer_cast<Cylinder>(player)->removeThing(weapon, 1);
	Item::items.getItems().resize(savedSize);
}

TEST_F(KnightStancePlayer, AutoAttackReductionCoversSplitDamageAndLeavesSpellPending) {
	player->attackReduction().apply(50, static_cast<uint64_t>(OTSYS_TIME()));
	CombatParams params;
	params.aggressive = false;
	params.combatType = COMBAT_PHYSICALDAMAGE;
	auto hit = [&](CombatOrigin origin, int reduction) {
		CombatDamage damage;
		damage.origin = origin;
		damage.primary.type = COMBAT_PHYSICALDAMAGE;
		damage.primary.value = -1000;
		damage.secondary.type = COMBAT_FIREDAMAGE;
		damage.secondary.value = -400;
		// Cancel the configured base critical chance so this arithmetic test is deterministic.
		damage.criticalChance = -static_cast<int32_t>(player->getBaseCritical().chance * 10000);
		const auto before = target->getHealth();
		Combat::doCombatHealth(player, target, damage, params);
		EXPECT_EQ(damage.attackReductionPercent, reduction);
		// PvP halves both components; mitigation truncates each before suppression.
		const auto mitigation = target->getMitigation();
		const int physical = static_cast<int>(500 - 500 * mitigation / 100.);
		const int fire = static_cast<int>(200 - 200 * mitigation / 100.);
		EXPECT_EQ(before - target->getHealth(), physical * (100 - reduction) / 100 + fire * (100 - reduction) / 100);
	};
	hit(ORIGIN_SPELL, 0);
	{
		AttackReductionState::Swing swing(player->attackReduction(), static_cast<uint64_t>(OTSYS_TIME()));
		hit(ORIGIN_MELEE, 50);
		hit(ORIGIN_MELEE, 50); // Cleave recipient in the same weapon swing.
		swing.finish(true);
	}
	hit(ORIGIN_RANGED, 0);
	player->attackReduction().apply(75, static_cast<uint64_t>(OTSYS_TIME()));
	hit(ORIGIN_FIST, 75);
}

TEST_F(KnightStancePlayer, ShieldHitAppliesBaseAndAugmentedSuppressionOnlyToVictim) {
	CombatParams params;
	params.aggressive = false;
	params.combatType = COMBAT_PHYSICALDAMAGE;
	params.nextAttackReduction = 50;
	for (int bonus : { 0, 25 }) {
		CombatDamage damage;
		damage.origin = ORIGIN_SPELL;
		damage.primary.type = COMBAT_PHYSICALDAMAGE;
		damage.primary.value = -1000;
		damage.damageReductionMultiplier = bonus;
		Combat::doCombatHealth(player, target, damage, params);
		EXPECT_EQ(target->attackReduction().snapshot(static_cast<uint64_t>(OTSYS_TIME())), 50 + bonus);
		EXPECT_EQ(player->attackReduction().snapshot(static_cast<uint64_t>(OTSYS_TIME())), 0);
	}
}

TEST_F(KnightStancePlayer, TargetedCombatReportsRejectionBeforeApplyingShieldState) {
	Combat combat;
	combat.setParam(COMBAT_PARAM_TYPE, COMBAT_PHYSICALDAMAGE);
	combat.setParam(COMBAT_PARAM_NEXTATTACKREDUCTION, 50);
	EXPECT_FALSE(combat.doCombat(player, player)); // Aggressive self target.
	EXPECT_FALSE(combat.doCombat(player, std::shared_ptr<Creature>{}));
	combat.setParam(COMBAT_PARAM_AGGRESSIVE, 0);
	target->attackReduction().apply(75, static_cast<uint64_t>(OTSYS_TIME()));
	target->setRemoved();
	EXPECT_FALSE(combat.doCombat(player, target));
	EXPECT_EQ(target->attackReduction().snapshot(static_cast<uint64_t>(OTSYS_TIME())), 0);
}

TEST_F(KnightStancePlayer, LuaCombatReportsValidAndRejectedTargetsAndRegistersShieldBindings) {
	std::unique_ptr<lua_State, decltype(&lua_close)> state { luaL_newstate(), &lua_close };
	auto L = state.get();
	luaL_openlibs(L);
	CreatureFunctions::init(L);
	LuaEnums::init(L);
	player->setGUID(990001);
	target->setGUID(990002);
	player->setID();
	target->setID();
	player->setName("Shield Lua Caster");
	target->setName("Shield Lua Target");
	struct Registration {
		std::shared_ptr<Player> value;
		explicit Registration(std::shared_ptr<Player> p) : value(std::move(p)) { g_game().addPlayer(value); }
		~Registration() { g_game().removePlayer(value); }
	} source(player), destination(target);
	lua_pushinteger(L, player->getID());
	lua_setglobal(L, "casterId");
	lua_pushinteger(L, target->getID());
	lua_setglobal(L, "targetId");
	ASSERT_EQ(luaL_dostring(L, R"(
		assert(CONST_ME_SHIELD_BASH == 318)
		assert(Player(casterId):getShieldSpellDefense() == nil)
		combat = Combat()
		assert(combat:setParameter(COMBAT_PARAM_TYPE, COMBAT_PHYSICALDAMAGE))
		assert(combat:setParameter(COMBAT_PARAM_NEXTATTACKREDUCTION, 50))
		assert(combat:setParameter(COMBAT_PARAM_AGGRESSIVE, false))
		assert(combat:execute(casterId, Variant(targetId)))
		assert(combat:execute(casterId, Variant("Shield Lua Target")))
		assert(combat:setParameter(COMBAT_PARAM_AGGRESSIVE, true))
		assert(not combat:execute(casterId, Variant(casterId)))
	)"), LUA_OK) << lua_tostring(L, -1);
	target->setRemoved();
	ASSERT_EQ(luaL_dostring(L, R"(
		assert(not combat:execute(casterId, Variant(targetId)))
		assert(not combat:execute(casterId, Variant("Shield Lua Target")))
	)"), LUA_OK) << lua_tostring(L, -1);
}

TEST_F(KnightStancePlayer, HealingFormulaUsesEffectiveSkillsAndProficiencyChannelsStaySeparate) {
	std::unique_ptr<lua_State, decltype(&lua_close)> state { luaL_newstate(), &lua_close };
	auto L = state.get();
	luaL_openlibs(L);
	CreatureFunctions::init(L);
	LuaEnums::init(L);
	player->setGUID(990003);
	player->setID();
	player->setName("Knight Healing Fixture");
	struct Registration {
		std::shared_ptr<Player> value;
		explicit Registration(std::shared_ptr<Player> p) : value(std::move(p)) { g_game().addPlayer(value); }
		~Registration() { g_game().removePlayer(value); }
	} source(player);
	lua_pushinteger(L, player->getID());
	lua_setglobal(L, "casterId");
	ASSERT_EQ(luaL_dofile(L, "data/scripts/lib/register_knight_spells.lua"), LUA_OK) << lua_tostring(L, -1);
	player->setLevel(2000);
	player->setVarStats(STAT_MAGICPOINTS, 15 - player->getMagicLevel());
	player->setVarSkill(SKILL_SHIELD, 130 - player->getSkillLevel(SKILL_SHIELD));
	player->setVarSkill(SKILL_SWORD, 130 - player->getSkillLevel(SKILL_SWORD));
	CombatDamage healing;
	healing.origin = ORIGIN_SPELL;
	healing.primary.type = COMBAT_HEALING;
	healing.instantSpellName = "Fair Wound Cleansing";
	ValueCallback callback(COMBAT_FORMULA_LEVELMAGIC);
	auto bounds = [&] {
		lua_pushinteger(L, callback.getMagicLevelSkill(player, healing));
		lua_setglobal(L, "healingMagicLevel");
		const auto result = luaL_dostring(L, "return KnightSpellFormula.healingBounds(Player(casterId), Player(casterId):getLevel(), healingMagicLevel, 225)");
		EXPECT_EQ(result, LUA_OK) << (result == LUA_OK ? "" : lua_tostring(L, -1));
		const std::pair<int32_t, int32_t> values {lua_tointeger(L, -2), lua_tointeger(L, -1)};
		lua_settop(L, 0);
		return values;
	};
	EXPECT_EQ(bounds(), (std::pair<int32_t, int32_t> {1846, 2607}));
	ASSERT_TRUE(player->setKnightStance(KnightStance::Protector));
	EXPECT_EQ(bounds(), (std::pair<int32_t, int32_t> {2127, 3028}));
	ASSERT_TRUE(player->setKnightStance(KnightStance::BloodRage));
	EXPECT_EQ(bounds(), (std::pair<int32_t, int32_t> {1846, 2607}));
	ASSERT_TRUE(player->setKnightStance(KnightStance::None));
	player->setSpecializedMagicLevel(COMBAT_HEALING, 5);
	EXPECT_EQ(bounds(), (std::pair<int32_t, int32_t> {2026, 2877}));
	player->setSpecializedMagicLevel(COMBAT_HEALING, -5);

	const auto savedSize = Item::items.getItems().size();
	const auto weaponId = static_cast<uint16_t>(savedSize);
	const auto shieldId = static_cast<uint16_t>(savedSize + 1);
	Item::items.getItems().resize(savedSize + 2);
	auto &weaponType = Item::items.getItems()[weaponId];
	weaponType.id = weaponId;
	weaponType.weaponType = WEAPON_SWORD;
	auto &shieldType = Item::items.getItems()[shieldId];
	shieldType.id = shieldId;
	shieldType.weaponType = WEAPON_SHIELD;
	shieldType.m_primaryType = "shields";
	const auto weapon = Item::CreateItem(weaponId);
	const auto shield = Item::CreateItem(shieldId);
	std::static_pointer_cast<Cylinder>(player)->internalAddThing(CONST_SLOT_LEFT, weapon);
	player->weaponProficiency().addSkillPercentage(SKILL_MAGLEVEL, SkillPercentage_t::SpellHealing, 10);
	player->weaponProficiency().addSkillPercentage(SKILL_SWORD, SkillPercentage_t::SpellDamage, 0.1);
	for (const auto type : {COMBAT_HEALING, COMBAT_PHYSICALDAMAGE, COMBAT_FIREDAMAGE}) {
		auto damage = healing;
		damage.primary.type = type;
		damage.primary.value = type == COMBAT_HEALING ? 1000 : -1000;
		player->weaponProficiency().applySkillSpellPercentage(damage, false);
		player->weaponProficiency().applySkillSpellPercentage(damage, true);
		EXPECT_EQ(damage.primary.value, type == COMBAT_HEALING ? 1150 : -1013);
	}
	player->wheel().setSpellInstant("Battle Healing", true);
	CombatParams params;
	params.aggressive = false;
	params.combatType = COMBAT_HEALING;
	for (const bool equippedShield : {false, true}) {
		if (equippedShield) std::static_pointer_cast<Cylinder>(player)->internalAddThing(CONST_SLOT_RIGHT, shield);
		auto damage = healing;
		damage.primary.value = bounds().first;
		player->changeHealth(1000 - player->getHealth(), false);
		Combat::doCombatHealth(player, player, damage, params);
		// 1846 base + 150 proficiency, then exactly one 10%/20% Battle Healing.
		EXPECT_EQ(player->getHealth(), 1000 + (equippedShield ? 2395 : 2195));
		player->changeHealth(player->getMaxHealth() - 10 - player->getHealth(), false);
		damage = healing;
		damage.primary.value = bounds().first;
		Combat::doCombatHealth(player, player, damage, params);
		EXPECT_EQ(player->getHealth(), player->getMaxHealth());
		if (equippedShield) std::static_pointer_cast<Cylinder>(player)->removeThing(shield, 1);
	}
	std::static_pointer_cast<Cylinder>(player)->removeThing(weapon, 1);
	Item::items.getItems().resize(savedSize);
}

TEST_F(KnightStancePlayer, OfficialKnightWheelSlotsAndCumulativeBonuses) {
	IOWheel io;
	ASSERT_TRUE(io.initializeGlobalData());
	struct Expected { uint8_t slot; uint16_t points; const char *name; };
	// Independent installed-client table, Knight conviction column, 1-based IDs.
	for (const auto &[slot, maximum, name] : {
		Expected{6,200,"Front Sweep"}, Expected{21,50,"Front Sweep"},
		Expected{8,100,"Shield Slam"}, Expected{24,100,"Shield Slam"},
		Expected{11,100,"Groundshaker"}, Expected{26,100,"Groundshaker"},
		Expected{13,100,"Intense Wound Cleansing"}, Expected{29,100,"Intense Wound Cleansing"},
		Expected{16,50,"Fierce Berserk"}, Expected{31,200,"Fierce Berserk"}}) {
		for (const auto points : { maximum - 1, int(maximum) }) {
			PlayerWheelMethodsBonusData bonus;
			player->wheel().setPointsBySlotType(slot, points);
			io.getWheelMapFunctions().at(static_cast<WheelSlots_t>(slot))(player, points, VOCATION_KNIGHT_CIP, bonus);
			if (points == maximum) {
				ASSERT_EQ(bonus.spells.size(), 1);
				EXPECT_EQ(bonus.spells[0], name);
			} else EXPECT_TRUE(bonus.spells.empty());
		}
	}
	const auto &data = io.getWheelBonusData().spells.knight;
	EXPECT_EQ(data[0].grade[1].increase.damage, 40);
	EXPECT_TRUE(data[0].grade[2].increase.area);
	EXPECT_EQ(data[1].grade[1].decrease.cooldown, 2);
	EXPECT_DOUBLE_EQ(data[1].grade[2].increase.damage, 12.5);
	EXPECT_EQ(data[2].grade[1].leech.life, 15);
	EXPECT_EQ(data[2].grade[2].increase.damageReduction, 25);
	EXPECT_EQ(data[3].grade[2].decrease.cooldown, 60);
	std::unordered_map<std::pair<uint16_t,uint8_t>,double,PairHash,PairEqual> augments;
	for (uint8_t grade : {1,2}) player->wheel().getAugmentsByVocation(316,"Shield Slam",grade,data,augments);
	EXPECT_DOUBLE_EQ(augments[std::make_pair(316, magic_enum::enum_integer(Augment_t::LifeLeech))], .15);
	EXPECT_DOUBLE_EQ(augments[std::make_pair(316, magic_enum::enum_integer(Augment_t::IncreasedDamageReduction))], .25);
}

TEST_F(KnightStancePlayer, RegisteredWheelPreservesFractionalDamageAndShieldReduction) {
	// Isolated injector owns these registrations; preserve any pre-existing fixture data.
	const auto saved = g_spells().getInstantSpells();
	struct Restore {
		std::map<std::string,std::shared_ptr<InstantSpell>> saved;
		~Restore() { g_spells().clear(); for (const auto &[word,spell] : saved) g_spells().setInstantSpell(word,spell); }
	} restore{saved};
	for (const auto name : {"Shield Slam", "Groundshaker", "Terra Wave"}) {
		auto spell = std::make_shared<InstantSpell>();
		spell->setName(name);
		spell->setWords(name);
		g_spells().setInstantSpell(name,spell);
	}
	IOWheel io;
	ASSERT_TRUE(io.initializeGlobalData());
	player->wheel().upgradeSpell("Groundshaker");
	player->wheel().upgradeSpell("Groundshaker");
	CombatDamage ground;
	ground.instantSpellName = "Groundshaker";
	ASSERT_NE(player->wheel().getCombatDataSpell(ground), nullptr);
	EXPECT_DOUBLE_EQ(ground.damageMultiplier, 12.5);
	auto terra = g_spells().getInstantSpellByName("Terra Wave");
	EXPECT_DOUBLE_EQ(terra->getWheelOfDestinyBoost(WheelSpellBoost_t::DAMAGE, WheelSpellGrade_t::REGULAR),6.5);
	player->wheel().upgradeSpell("Shield Slam");
	CombatDamage first;
	first.instantSpellName = "Shield Slam";
	player->wheel().getCombatDataSpell(first);
	EXPECT_EQ(first.lifeLeech, 1500);
	EXPECT_EQ(first.damageReductionMultiplier, 0);
	player->wheel().upgradeSpell("Shield Slam");
	CombatDamage second;
	second.instantSpellName = "Shield Slam";
	player->wheel().getCombatDataSpell(second);
	EXPECT_EQ(second.lifeLeech, 1500);
	EXPECT_EQ(second.damageReductionMultiplier, 25);
}

TEST(KnightWheel, MasteryScalingUsesMissingHealthAndBoundsArithmetic) {
	EXPECT_DOUBLE_EQ(KnightWheel::masteryPercent(0,1,100,false),0);
	EXPECT_DOUBLE_EQ(KnightWheel::masteryPercent(1,86,100,false),1);
	EXPECT_DOUBLE_EQ(KnightWheel::masteryPercent(2,76,100,true),4);
	EXPECT_DOUBLE_EQ(KnightWheel::masteryPercent(3,50,100,false),5);
	EXPECT_DOUBLE_EQ(KnightWheel::masteryPercent(3,105,100,true),0);
	EXPECT_DOUBLE_EQ(KnightWheel::masteryPercent(3,INT32_MIN,INT32_MAX,true),20);
	EXPECT_EQ(KnightWheel::scaleDamage(INT32_MAX,120),INT32_MAX);
}

TEST_F(KnightStancePlayer, IntenseHealingCooldownCombinesWheelProficiencyAndEquipmentWithFloor) {
	const auto saved = g_spells().getInstantSpells();
	struct RestoreSpells {
		std::map<std::string, std::shared_ptr<InstantSpell>> saved;
		~RestoreSpells() {
			g_spells().clear();
			for (const auto &[word, spell] : saved) g_spells().setInstantSpell(word, spell);
		}
	} restore { saved };
	auto spell = std::make_shared<InstantSpell>();
	spell->setName("Intense Wound Cleansing");
	spell->setWords("exura gran ico");
	spell->setSpellId(158);
	spell->setCooldown(120000);
	spell->setGroup(SPELLGROUP_HEALING);
	spell->setGroupCooldown(2000);
	spell->setAggressive(false);
	g_spells().setInstantSpell("exura gran ico", spell);
	IOWheel io;
	ASSERT_TRUE(io.initializeGlobalData());
	EXPECT_DOUBLE_EQ(spell->getWheelOfDestinyBoost(WheelSpellBoost_t::COOLDOWN, WheelSpellGrade_t::UPGRADED), 60000);

	const auto savedSize = Item::items.getItems().size();
	const auto armorId = static_cast<uint16_t>(savedSize);
	Item::items.getItems().resize(savedSize + 1);
	auto &armorType = Item::items.getItems()[armorId];
	armorType.id = armorId;
	armorType.augments.emplace_back(std::make_shared<AugmentInfo>("Intense Wound Cleansing", Augment_t::Cooldown, 24000));
	const auto armor = Item::CreateItem(armorId);
	ASSERT_NE(armor, nullptr);

	// Exercise independent sources before combining them; a seconds/ms mismatch
	// or a second 80% proficiency reduction must not hide behind the 50% floor.
	for (const bool wheel : {false, true}) {
		if (wheel) {
			player->wheel().upgradeSpell("Intense Wound Cleansing");
			player->wheel().upgradeSpell("Intense Wound Cleansing");
		}
		for (const bool proficiency : {false, true}) {
			player->weaponProficiency().clearAllStats();
			if (proficiency) {
				WeaponProficiencySpells::Bonus bonus;
				bonus.decrease.cooldown = 30000;
				player->weaponProficiency().addSpellBonus(158, bonus);
			}
			for (const bool equipment : {false, true}) {
				if (equipment) std::static_pointer_cast<Cylinder>(player)->internalAddThing(CONST_SLOT_ARMOR, armor);
				player->removeCondition(CONDITION_SPELLCOOLDOWN);
				player->removeCondition(CONDITION_SPELLGROUPCOOLDOWN);
				spell->postCastSpell(player, true, false);
				const auto own = player->getCondition(CONDITION_SPELLCOOLDOWN, CONDITIONID_DEFAULT, 158);
				const auto groupCooldown = player->getCondition(CONDITION_SPELLGROUPCOOLDOWN, CONDITIONID_DEFAULT, SPELLGROUP_HEALING);
				EXPECT_NE(own, nullptr);
				EXPECT_NE(groupCooldown, nullptr);
				const auto expected = wheel ? 60000 : 120000 - (proficiency ? 30000 : 0) - (equipment ? 24000 : 0);
				if (own) EXPECT_EQ(own->getTicks(), expected);
				if (groupCooldown) EXPECT_EQ(groupCooldown->getTicks(), 2000);
				if (equipment) std::static_pointer_cast<Cylinder>(player)->removeThing(armor, 1);
			}
		}
	}
	Item::items.getItems().resize(savedSize);
}

TEST_F(KnightStancePlayer, CombatMasteryUsesEachTargetsHealthAndBattleHealingOnlyBoostsSpells) {
	player->wheel().setStage(WheelStage_t::COMBAT_MASTERY,3);
	target->wheel().setStage(WheelStage_t::COMBAT_MASTERY,3);
	for (const int health : {10000,5000,2000}) {
		target->changeHealth(health-target->getHealth(),false);
		CombatDamage damage;
		damage.origin=ORIGIN_MELEE;
		damage.primary.type=COMBAT_PHYSICALDAMAGE;
		damage.primary.value=-1000;
		damage.secondary.type=COMBAT_FIREDAMAGE;
		damage.secondary.value=-1000;
		ASSERT_TRUE(g_game().combatChangeHealth(player,target,damage));
		const int bonus=(10000-health)/1000;
		const int expected=(1000*(100+bonus)/100)*(100-bonus)/100;
		EXPECT_EQ(health-target->getHealth(),2*expected);
	}
	player->wheel().setSpellInstant("Battle Healing",true);
	EXPECT_EQ(player->wheel().getBattleHealingPercent(),10);
	for (bool spell : {false,true}) {
		player->changeHealth(5000-player->getHealth(),false);
		CombatDamage heal;
		heal.primary.type=COMBAT_HEALING;
		heal.primary.value=1000;
		if (spell) heal.instantSpellName="Wound Cleansing";
		ASSERT_TRUE(g_game().combatChangeHealth(player,player,heal));
		EXPECT_EQ(player->getHealth(),spell ? 6100 : 6000);
	}
}
