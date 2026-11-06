/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/players/components/weapon_proficiency.hpp"
#include "creatures/players/player.hpp"
#include "config/configmanager.hpp"
#include "game/game.hpp"
#include "kv/value_wrapper.hpp"
#include "kv/kv.hpp"
#include "lib/logging/in_memory_logger.hpp"
#include "creatures/monsters/monster.hpp"
#include "creatures/monsters/monsters.hpp"
#include "creatures/players/grouping/groups.hpp"
#include "creatures/appearance/mounts/mounts.hpp"
#include "creatures/players/vocations/vocation.hpp"
#include "creatures/combat/spells.hpp"
#include "creatures/interactions/chat.hpp"
#include "items/tile.hpp"
#include "items/weapons/weapons.hpp"
#include "lua/creature/movement.hpp"
#include "lua/creature/events.hpp"
#include "map/spectators.hpp"
#include "map/utils/mapsector.hpp"
#include "kv/in_memory_kv.hpp"
#include <appearances.pb.h>
#include "items/containers/container.hpp"
#include "lua/functions/creatures/creature_functions.hpp"
#include "lua/functions/lua_functions_loader.hpp"

#ifndef USE_PRECOMPILED_HEADERS
	#include <array>
	#include <fstream>
	#include <limits>
#endif

TEST(Moonsilver, LevelBasedMissileDamageIsBounded) {
	EXPECT_EQ(-2000, WeaponProficiency::homingMissileDamage(1000, 2));
	EXPECT_EQ(-3000, WeaponProficiency::homingMissileDamage(1000, 3));
	EXPECT_EQ(-INT32_MAX, WeaponProficiency::homingMissileDamage(UINT32_MAX, 3));
	EXPECT_EQ(0, WeaponProficiency::homingMissileDamage(1000, -1));
	EXPECT_EQ(0, WeaponProficiency::homingMissileDamage(1000, std::numeric_limits<double>::infinity()));
	EXPECT_EQ(0, WeaponProficiency::homingMissileDamage(1000, std::numeric_limits<double>::quiet_NaN()));
}

class MoonsilverInjector : public di::extension::injector<> {
public:
	void releaseRuntimeOwners() {
		cfg().data<Weapons>().reset();
		cfg().data<MoveEvents>().reset();
		cfg().data<Chat>().reset();
		cfg().data<Events>().reset();
		cfg().data<Spells>().reset();
		cfg().data<Monsters>().reset();
		cfg().data<Game>().reset();
	}
};

class MoonsilverData : public testing::Test {
protected:
	void SetUp() override {
		previous = DI::getTestContainer();
		InMemoryLogger::install(injector);
		KVMemory::install(injector);
		DI::setTestContainer(&injector);
		g_configManager().setConfigFileLua("config.lua.dist");
		ASSERT_TRUE(g_configManager().load());
		originalProficiencies = std::move(WeaponProficiency::getProficiencies());
		ASSERT_TRUE(WeaponProficiency::loadFromJson(true));
	}
	void TearDown() override {
		WeaponProficiency::getProficiencies() = std::move(originalProficiencies);
		injector.releaseRuntimeOwners();
		DI::setTestContainer(previous);
	}
	MoonsilverInjector injector {};
	di::extension::injector<>* previous = nullptr;
	std::unordered_map<uint16_t, Proficiency> originalProficiencies;
};

TEST_F(MoonsilverData, AllOfficialWeaponsResolveTheirOwnSevenLevelTrees) {
	Canary::protobuf::appearances::Appearances appearances;
	std::ifstream file("data/items/appearances.dat", std::ios::binary);
	ASSERT_TRUE(appearances.ParseFromIstream(&file));
	constexpr std::array<uint16_t, 22> ids { 478, 489, 485, 496, 475, 486, 480, 491, 476, 487, 482, 493, 479, 490, 477, 488, 484, 495, 481, 492, 483, 494 };
	size_t count = 0;
	for (const auto &object : appearances.object()) {
		if (object.id() < 53207 || object.id() > 53228) {
			continue;
		}
		++count;
		const auto id = ids[object.id() - 53207];
		ASSERT_EQ(id, object.flags().proficiency().proficiency_id());
		const auto &tree = WeaponProficiency::getProficiencies().at(id);
		ASSERT_EQ(7, tree.level.size());
		ASSERT_EQ(3, tree.level[2].perks.size());
		const auto &missile = tree.level[2].perks[1];
		EXPECT_EQ(WeaponProficiencyBonus_t::HOMING_MISSILE, missile.type);
		EXPECT_DOUBLE_EQ(0.01, missile.probability);
		EXPECT_DOUBLE_EQ(object.id() % 2 == 0 ? 3.0 : 2.0, missile.multiplier);
		EXPECT_NE(COMBAT_NONE, missile.element);
		EXPECT_GT(missile.missileId, 0);
	}
	EXPECT_EQ(ids.size(), count);
}

TEST_F(MoonsilverData, MissilePerkSurvivesPersistenceAndOldPerksRemainReadable) {
	auto player = std::make_shared<Player>();
	const auto &perk = WeaponProficiency::getProficiencies().at(489).level[2].perks[1];
	const auto restored = WeaponProficiency::deserializePerk(player->weaponProficiency().serializePerk(perk));
	EXPECT_EQ(perk.type, restored.type);
	EXPECT_EQ(perk.element, restored.element);
	EXPECT_EQ(perk.missileId, restored.missileId);
	EXPECT_DOUBLE_EQ(perk.multiplier, restored.multiplier);
	EXPECT_DOUBLE_EQ(perk.probability, restored.probability);
	const ValueWrapper old { { "type", static_cast<IntType>(0) }, { "value", 1.0 } };
	const auto restoredOld = WeaponProficiency::deserializePerk(old);
	EXPECT_DOUBLE_EQ(1, restoredOld.value);
	EXPECT_EQ(0, restoredOld.missileId);
	EXPECT_DOUBLE_EQ(0, restoredOld.probability);
}

TEST_F(MoonsilverData, StellarSpellAugmentKeepsFractionalDamage) {
	auto player = std::make_shared<Player>();
	const auto &perk = WeaponProficiency::getProficiencies().at(489).level[2].perks[2];
	ASSERT_EQ(WeaponProficiencyBonus_t::SPELL_AUGMENT, perk.type);
	ASSERT_DOUBLE_EQ(0.075, perk.value);
	WeaponProficiencySpells::Bonus bonus;
	bonus.increase.damage = perk.value;
	player->weaponProficiency().addSpellBonus(perk.spellId, bonus);
	CombatDamage damage;
	player->weaponProficiency().applySpellAugment(damage, perk.spellId);
	EXPECT_DOUBLE_EQ(7.5, damage.damageMultiplier);
}

class MoonsilverCombat : public MoonsilverData {
protected:
	void SetUp() override {
		MoonsilverData::SetUp();
		ASSERT_TRUE(g_vocations().loadFromXml());
		oldItemCount = Item::items.size();
		weaponId = static_cast<uint16_t>(std::max<size_t>(oldItemCount, 1));
		Item::items.getItems().resize(weaponId + 1);
		auto &type = Item::items.getItems()[weaponId];
		type.id = weaponId;
		type.weaponType = WEAPON_SWORD;
		type.proficiencyId = 478;
		// Force only the random proc roll, retaining real selection and damage code.
		WeaponProficiency::getProficiencies().at(478).level[2].perks[1].probability = 1;
		caster = std::make_shared<Player>();
		caster->setGroup(std::make_shared<Group>());
		caster->setLevel(1000);
		ASSERT_TRUE(caster->setVocation(VOCATION_ELITE_KNIGHT));
		caster->setID();
		place(caster, Position(120, 120, 7));
		std::static_pointer_cast<Cylinder>(caster)->internalAddThing(CONST_SLOT_LEFT, Item::CreateItem(weaponId));
		caster->weaponProficiency().addExperience(UINT32_MAX, weaponId);
		caster->weaponProficiency().setSelectedPerk(2, 1, weaponId);
		ASSERT_EQ(1, caster->weaponProficiency().getSelectedPerks(weaponId).size());
		auto targetType = std::make_shared<MonsterType>("Moonsilver test target");
		targetType->info.health = targetType->info.healthMax = 10000;
		target = std::make_shared<Monster>(targetType);
		target->setID();
		place(target, Position(121, 120, 7));
	}
	void place(const std::shared_ptr<Creature> &creature, const Position &pos) {
		oldTiles.emplace_back(pos, g_game().map.getTile(pos));
		auto tile = std::make_shared<DynamicTile>(pos);
		g_game().map.getBestMapSector(pos.x, pos.y)->createFloor(pos.z)->setTile(pos.x, pos.y, tile);
		tile->addThing(creature);
		g_game().map.getMapSector(pos.x, pos.y)->addCreature(creature);
		mapped.push_back(creature);
		Spectators::clearCache();
	}
	void TearDown() override {
		Spectators::clearCache();
		for (const auto &creature : mapped) {
			const auto pos = creature->getPosition();
			g_game().map.getMapSector(pos.x, pos.y)->removeCreature(creature);
			creature->setParent({});
		}
		for (const auto &[pos, tile] : oldTiles) {
			g_game().map.getBestMapSector(pos.x, pos.y)->createFloor(pos.z)->setTile(pos.x, pos.y, tile);
		}
		mapped.clear();
		caster.reset();
		target.reset();
		Item::items.getItems().resize(oldItemCount);
		MoonsilverData::TearDown();
	}
	size_t oldItemCount = 0;
	uint16_t weaponId = 0;
	std::shared_ptr<Player> caster;
	std::shared_ptr<Monster> target;
	CreatureVector mapped;
	std::vector<std::pair<Position, std::shared_ptr<Tile>>> oldTiles;
};

TEST_F(MoonsilverCombat, SelectedMissileHitsNearbyTargetWithoutAttackTarget) {
	ASSERT_EQ(nullptr, caster->getAttackedCreature());
	ASSERT_TRUE(caster->weaponProficiency().tryHomingMissile());
	EXPECT_EQ(8000, target->getHealth());
	caster->weaponProficiency().clearSelectedPerks(weaponId);
	EXPECT_FALSE(caster->weaponProficiency().tryHomingMissile());
	EXPECT_EQ(8000, target->getHealth());
}

TEST_F(MoonsilverCombat, ProtectionZoneAndDeadCasterPreventMissile) {
	target->getTile()->setFlag(TILESTATE_PROTECTIONZONE);
	EXPECT_FALSE(caster->weaponProficiency().tryHomingMissile());
	EXPECT_EQ(10000, target->getHealth());
	caster->changeHealth(-caster->getHealth(), false);
	EXPECT_FALSE(caster->weaponProficiency().tryHomingMissile());
	EXPECT_EQ(10000, target->getHealth());
}

class MoonsilverItems : public MoonsilverData {
protected:
	void SetUp() override {
		MoonsilverData::SetUp();
		ASSERT_TRUE(g_vocations().loadFromXml());
		previousItems = std::move(Item::items.getItems());
		previousNames = std::move(Item::items.nameToItems);
		ASSERT_EQ(ERROR_NONE, g_game().loadAppearanceProtobuf("data/items/appearances.dat"));
		Item::items.loadFromProtobuf();
		pugi::xml_document document;
		ASSERT_TRUE(document.load_file("data/items/items.xml"));
		for (const auto node : document.child("items").children("item")) {
			const auto id = node.attribute("id").as_uint();
			if ((id >= 53207 && id <= 53233) || (id >= 60001 && id <= 60006) || id == 53695 || id == 36827 || (id >= 36723 && id <= 36728) || id == 2854 || id == ITEM_STORE_INBOX) {
				Item::items.parseItemNode(node, id);
			}
		}
	}
	void TearDown() override {
		Item::items.getItems() = std::move(previousItems);
		Item::items.nameToItems = std::move(previousNames);
		MoonsilverData::TearDown();
	}
	std::vector<ItemType> previousItems;
	Items::NameMap previousNames;
};

TEST_F(MoonsilverItems, NativeLoaderRegistersWeaponsEquipmentAndRestrictions) {
	for (uint16_t id = 53207; id <= 53233; ++id) {
		SCOPED_TRACE(id);
		const auto item = Item::CreateItem(id);
		ASSERT_NE(nullptr, item);
		const auto &type = Item::items[id];
		EXPECT_TRUE(type.loaded);
		EXPECT_EQ(4, type.upgradeClassification);
		const auto event = g_moveEvents().getEvent(item, MOVE_EVENT_EQUIP);
		ASSERT_NE(nullptr, event);
		EXPECT_EQ(id <= 53228 ? 1000 : 800, event->getReqLevel());
		if (id <= 53228) {
			ASSERT_NE(nullptr, g_weapons().getWeapon(item));
			EXPECT_GE(type.proficiencyId, 475);
			EXPECT_LE(type.proficiencyId, 496);
		}
	}
}

class ProficiencyShaping : public MoonsilverCombat {
protected:
 void SetUp() override {
  MoonsilverCombat::SetUp();
  caster->getTile()->setFlag(TILESTATE_PROTECTIONZONE);
  caster->setForgeDusts(10000);
 }
 nlohmann::json act(const std::string &action, uint8_t level = 2, uint8_t index = 1) {
  return caster->weaponProficiency().shapingRequest({{"version",1},{"action",action},{"weaponId",weaponId},{"revision",caster->weaponProficiency().shapingState(weaponId)["revision"]},{"level",level},{"index",index}});
 }
};

TEST_F(ProficiencyShaping, ChargesOnceAndPersistsDistinctOffersUntilKeepOrReplace) {
 auto initial = caster->weaponProficiency().shapingState(weaponId);
 ASSERT_EQ(64, initial["pool"].size());
 auto state = act("modify");
 ASSERT_FALSE(state.contains("error")) << state.dump();
 ASSERT_EQ(1, state["shapes"].size());
 EXPECT_EQ(9750, caster->getForgeDusts());
 auto repeated = caster->weaponProficiency().shapingRequest({{"version",1},{"action","modify"},{"weaponId",weaponId},{"revision",0},{"level",2},{"index",1}});
 EXPECT_TRUE(repeated.contains("error"));
 EXPECT_EQ(9750, caster->getForgeDusts());
 state = act("refine");
 ASSERT_FALSE(state.contains("error")) << state.dump();
 EXPECT_EQ(1,state["shapes"][0]["rank"]);
 EXPECT_EQ(9625,caster->getForgeDusts());
 state = act("reshape");
 ASSERT_FALSE(state.contains("error")) << state.dump();
 EXPECT_EQ(9375,caster->getForgeDusts());
 const auto offers = state["shapes"][0]["offers"].get<std::vector<uint16_t>>();
 ASSERT_EQ(3,offers.size());
 EXPECT_NE(offers[0],offers[1]); EXPECT_NE(offers[1],offers[2]); EXPECT_NE(offers[0],offers[2]);
 for (auto id: offers) EXPECT_NE(id,state["shapes"][0]["id"]);
 caster->weaponProficiency().save(weaponId);
 caster->weaponProficiency().load();
 state=caster->weaponProficiency().shapingState(weaponId);
 EXPECT_EQ(offers,state["shapes"][0]["offers"].get<std::vector<uint16_t>>());
 EXPECT_TRUE(act("refine").contains("error"));
 EXPECT_EQ(9375,caster->getForgeDusts());
 auto replaced=caster->weaponProficiency().shapingRequest({{"version",1},{"action","replace"},{"weaponId",weaponId},{"revision",state["revision"]},{"level",2},{"index",1},{"choice",1}});
 ASSERT_FALSE(replaced.contains("error")) << replaced.dump();
 EXPECT_EQ(offers[1],replaced["shapes"][0]["id"]);
 EXPECT_EQ(1,replaced["shapes"][0]["rank"]);
 EXPECT_TRUE(replaced["shapes"][0]["offers"].empty());
 EXPECT_EQ(9375,caster->getForgeDusts());
 EXPECT_FALSE(act("reshape").contains("error"));
 EXPECT_FALSE(act("keep").contains("error"));
 EXPECT_EQ(9125,caster->getForgeDusts());
 auto cleared=act("clear");
 EXPECT_TRUE(cleared["shapes"].empty());
 EXPECT_EQ(9125,caster->getForgeDusts());
 const auto selected=caster->weaponProficiency().getSelectedPerks(weaponId);
 ASSERT_EQ(1,selected.size());
 EXPECT_EQ(WeaponProficiencyBonus_t::HOMING_MISSILE,selected[0].type);
}

TEST_F(ProficiencyShaping, RankLimitsCostsAndSecondSlotRequireMastery) {
 ASSERT_FALSE(act("modify").contains("error"));
 for (uint8_t rank=0;rank<10;++rank) {
  auto state=act("refine"); ASSERT_FALSE(state.contains("error")) << state.dump();
  EXPECT_EQ(rank+1,state["shapes"][0]["rank"]);
 }
 EXPECT_EQ(5125,caster->getForgeDusts());
 EXPECT_TRUE(act("refine").contains("error"));
 EXPECT_TRUE(act("maximise").contains("error"));
 EXPECT_EQ(5125,caster->getForgeDusts());
 caster->weaponProficiency().setSelectedPerk(0,0,weaponId);
 ASSERT_FALSE(act("modify",0,0).contains("error"));
 EXPECT_EQ(4125,caster->getForgeDusts());
 caster->weaponProficiency().setSelectedPerk(1,0,weaponId);
 EXPECT_TRUE(act("modify",1,0).contains("error"));
 EXPECT_EQ(4125,caster->getForgeDusts());
}

TEST_F(ProficiencyShaping, RejectsOutsideProtectionZoneUnaffordableAndMalformedActions) {
 caster->getTile()->resetFlag(TILESTATE_PROTECTIONZONE);
 EXPECT_TRUE(act("modify").contains("error"));
 EXPECT_EQ(10000,caster->getForgeDusts());
 caster->getTile()->setFlag(TILESTATE_PROTECTIONZONE);
 caster->setForgeDusts(249);
 EXPECT_TRUE(act("modify").contains("error"));
 EXPECT_EQ(249,caster->getForgeDusts());
 EXPECT_TRUE(caster->weaponProficiency().shapingRequest({{"weaponId",-1},{"action","state"}}).contains("error"));
 EXPECT_TRUE(act("unknown").contains("error"));
 EXPECT_TRUE(act("modify",6,250).contains("error"));
 WeaponProficiencyData saved;
 EXPECT_NO_THROW(WeaponProficiency::deserializeShapes(saved,R"({"version":"bad","revision":0,"shapes":[]})"));
 EXPECT_TRUE(saved.shapes.empty());
}

TEST_F(ProficiencyShaping, UnmasteredWeaponsCannotModifySecondPerk) {
 auto &wp = caster->weaponProficiency();
 WeaponProficiencyData data;
 data.experience = wp.getExperienceArray(weaponId)[2];
 data.perks = {WeaponProficiency::getProficiencies().at(478).level[2].perks[1], WeaponProficiency::getProficiencies().at(478).level[0].perks[0]};
 caster->kv()->scoped("weapon-proficiency")->set(std::to_string(weaponId),wp.serialize(data));
 wp.load();
 ASSERT_FALSE(wp.shapingState(weaponId)["mastered"].get<bool>());
 ASSERT_FALSE(act("modify").contains("error"));
 EXPECT_TRUE(act("modify",0,0).contains("error"));
 EXPECT_EQ(9750,caster->getForgeDusts());
 EXPECT_TRUE(act("modify",6,0).contains("error"));
 EXPECT_EQ(9750,caster->getForgeDusts());
}

TEST_F(ProficiencyShaping, HighestSkillRankInterpolationDrivesDistinctCombatChannels) {
 auto &wp=caster->weaponProficiency();
 caster->setVarSkill(SKILL_AXE, 90);
 ASSERT_EQ(100,caster->getSkillLevel(SKILL_AXE));
 for (uint16_t id : {26,27,28}) {
  for (uint8_t rank : {0,4,10}) {
   SCOPED_TRACE(id);
   SCOPED_TRACE(rank);
   WeaponProficiencyData data;
   data.experience=wp.getMaxExperience(weaponId);
   data.perks={WeaponProficiency::getProficiencies().at(478).level[2].perks[1]};
   data.shapes={{2,1,rank,id,{}}};
   caster->kv()->scoped("weapon-proficiency")->set(std::to_string(weaponId),wp.serialize(data));
   wp.load(); wp.clearAllStats(); wp.applyPerks(weaponId,false);
   const auto selected=wp.getSelectedPerks(weaponId);
   ASSERT_EQ(1,selected.size());
   ASSERT_TRUE(selected[0].highestCombatSkill);
   const auto state=wp.shapingState(weaponId);
   double minimum=0,maximum=0;
   for (const auto &entry: state["pool"]) if (entry["Id"]==id) {minimum=entry["Perk"]["Value"];maximum=entry["MaximumValue"];}
   const auto expected=minimum+(maximum-minimum)*rank/10;
   EXPECT_DOUBLE_EQ(expected,selected[0].value);
   const auto bonus=static_cast<int32_t>(std::ceil(100*expected));
   for (auto type : {COMBAT_PHYSICALDAMAGE,COMBAT_FIREDAMAGE,COMBAT_HEALING}) {
    CombatDamage damage;
    damage.instantSpellName="Test shaped spell";
    damage.primary.type=type;
    const auto original=type==COMBAT_HEALING ? 100 : -100;
    damage.primary.value=original;
    wp.applySkillSpellPercentage(damage,false);wp.applySkillSpellPercentage(damage,true);
    const auto applies=(id==27 && type!=COMBAT_HEALING)||(id==28 && type==COMBAT_HEALING);
    EXPECT_EQ(original+(applies?(original<0?-bonus:bonus):0),damage.primary.value);
   }
   CombatDamage attack;
   attack.origin=ORIGIN_MELEE;attack.primary.type=COMBAT_PHYSICALDAMAGE;attack.primary.value=-100;
   wp.applySkillAutoAttackPercentage(attack);
   EXPECT_EQ(-100-(id==26?bonus:0),attack.primary.value);
  }
 }
}

TEST_F(ProficiencyShaping, SkillBonusesSaturateBeforeNarrowing) {
 auto &wp=caster->weaponProficiency();
 wp.clearAllStats();
 wp.addSkillPercentage(SKILL_SWORD,SkillPercentage_t::AutoAttack,1e20);
 CombatDamage attack;
 attack.origin=ORIGIN_MELEE;attack.primary.type=COMBAT_PHYSICALDAMAGE;attack.primary.value=INT32_MIN+5;
 wp.applySkillAutoAttackPercentage(attack);
 EXPECT_EQ(INT32_MIN,attack.primary.value);
 ProficiencyPerk perk;perk.highestCombatSkill=true;perk.type=WeaponProficiencyBonus_t::SKILL_PERCENTAGE_SPELL_HEALING;perk.value=1e20;
 perk.level=2;perk.index=1;
 WeaponProficiency::getProficiencies().at(478).level[2].perks[1]=perk;
 wp.applyPerks(weaponId,false);
 CombatDamage healing;
 healing.instantSpellName="Boundary healing";healing.primary.type=COMBAT_HEALING;healing.primary.value=INT32_MAX-5;
 wp.applySkillSpellPercentage(healing,true);
 EXPECT_EQ(INT32_MAX,healing.primary.value);
 EXPECT_EQ(0,healing.secondary.value);
}

TEST_F(ProficiencyShaping, MaximiseConsumesOneOrbAndNoDust) {
 ASSERT_FALSE(act("modify").contains("error"));
 EXPECT_TRUE(act("maximise").contains("error"));
 EXPECT_EQ(9750,caster->getForgeDusts());
 Item::items.getItems().resize(53696);
 auto &orbType=Item::items.getItems()[53695];
 orbType.id=53695;orbType.stackable=true;
 auto orb=Item::CreateItem(53695,2);
 ASSERT_NE(nullptr,orb);
 std::static_pointer_cast<Cylinder>(caster)->internalAddThing(CONST_SLOT_RIGHT,orb);
 auto state=act("maximise");
 ASSERT_FALSE(state.contains("error")) << state.dump();
 EXPECT_EQ(10,state["shapes"][0]["rank"]);
 EXPECT_EQ(1,state["orbs"]);
 EXPECT_EQ(9750,caster->getForgeDusts());
 EXPECT_TRUE(act("maximise").contains("error"));
 EXPECT_EQ(1,caster->weaponProficiency().shapingState(weaponId)["orbs"]);
}

TEST_F(MoonsilverData, OutfitOwnershipBindingDoesNotDependOnWearability) {
    auto player = std::make_shared<Player>();
    player->setGroup(std::make_shared<Group>());
    player->addOutfit(600, 1);
    player->addOutfit(601, 3);
    player->addOutfit(602, 0);
    std::unique_ptr<lua_State, decltype(&lua_close)> state {luaL_newstate(), &lua_close};
    auto L = state.get();
    luaL_openlibs(L);
    CreatureFunctions::init(L);
    Lua::pushThing(L, player);
    lua_setglobal(L, "owner");
    for (auto sex : {PLAYERSEX_MALE, PLAYERSEX_FEMALE}) {
        player->setSex(sex);
        ASSERT_EQ(LUA_OK, luaL_dostring(L,
            "assert(owner:getOwnedOutfitAddons(600) == 1); "
            "assert(owner:getOwnedOutfitAddons(601) == 3); "
            "assert(owner:getOwnedOutfitAddons(602) == 0); "
            "assert(owner:getOwnedOutfitAddons(603) == nil)")) << lua_tostring(L, -1);
    }
}

TEST_F(MoonsilverItems, HuntingRewardBackpacksLoadWithTheirRealCapacities) {
    for (const auto [id, capacity] : {std::pair<uint16_t, uint16_t>{60002, 24}, {60003, 28}, {60005, 24}, {60006, 28}}) {
        auto item = Item::CreateItem(id);
        ASSERT_NE(nullptr, item);
        ASSERT_NE(nullptr, item->getContainer());
        EXPECT_EQ(capacity, item->getContainer()->capacity());
        EXPECT_TRUE(Item::items[id].slotPosition & SLOTP_BACKPACK);
    }
    auto mountBag = Item::CreateItem(60004);
    ASSERT_NE(nullptr, mountBag);
    EXPECT_EQ(nullptr, mountBag->getContainer());
    EXPECT_EQ("premium mount reward bag", mountBag->getName());
    auto bag = Item::CreateItem(60001);
    ASSERT_NE(nullptr, bag);
    EXPECT_EQ(nullptr, bag->getContainer());
    EXPECT_EQ("full outfit reward bag", bag->getName());
}

TEST_F(MoonsilverItems, MountOwnershipBindingDoesNotDependOnPremiumOrAccess) {
    ASSERT_TRUE(g_game().mounts->loadFromXml());
    auto player = std::make_shared<Player>();
    auto group = std::make_shared<Group>();
    player->setGroup(group);
    ASSERT_TRUE(player->tameMount(23));
    std::unique_ptr<lua_State, decltype(&lua_close)> state {luaL_newstate(), &lua_close};
    auto L = state.get();
    luaL_openlibs(L);
    CreatureFunctions::init(L);
    Lua::pushThing(L, player);
    lua_setglobal(L, "owner");
    for (const auto access : {false, true}) {
        group->access = access;
        ASSERT_EQ(LUA_OK, luaL_dostring(L,
            "assert(owner:ownsMount(23)); assert(not owner:ownsMount(114)); "
            "assert(not owner:ownsMount(0)); assert(not owner:ownsMount(-1)); "
            "assert(not owner:ownsMount(256)); assert(not owner:ownsMount(279))")) << lua_tostring(L, -1);
    }
    ASSERT_TRUE(player->untameMount(23));
    ASSERT_EQ(LUA_OK, luaL_dostring(L, "assert(not owner:ownsMount(23))")) << lua_tostring(L, -1);
}

TEST_F(MoonsilverItems, HuntingShopPurchasesUseLiveCatalogAndRealInventory) {
    ASSERT_TRUE(g_game().mounts->loadFromXml());
    auto player = std::make_shared<Player>();
    player->setGroup(std::make_shared<Group>());
    player->setGUID(193);
    // Inventory top-parent ownership resolves through the player's world tile.
    auto tile = std::make_shared<DynamicTile>(Position(120, 120, 7));
    player->setParent(tile);
    player->addTaskHuntingPoints(1000000);
    auto backpack = Item::CreateItem(60003);
    auto storeInbox = Item::CreateItem(ITEM_STORE_INBOX);
    ASSERT_NE(nullptr, backpack->getContainer());
    ASSERT_NE(nullptr, storeInbox->getContainer());
    std::static_pointer_cast<Cylinder>(player)->internalAddThing(CONST_SLOT_BACKPACK, backpack);
    std::static_pointer_cast<Cylinder>(player)->internalAddThing(CONST_SLOT_STORE_INBOX, storeInbox);
    std::unique_ptr<lua_State, decltype(&lua_close)> state {luaL_newstate(), &lua_close};
    auto L = state.get();
    Lua::load(L);
    ASSERT_TRUE(Lua::reserveScriptEnv());
    struct ResetEnvironment { ~ResetEnvironment() { Lua::resetScriptEnv(); } } resetEnvironment;
    Lua::pushThing(L, player);
    lua_setglobal(L, "buyer");
    ASSERT_EQ(LUA_OK, luaL_dofile(L, "tests/lua/native_hunting_shop_inventory.lua")) << lua_tostring(L, -1);
}
