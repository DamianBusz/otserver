/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/players/components/paladin_stance.hpp"
#include "creatures/players/player.hpp"
#include "creatures/players/grouping/groups.hpp"
#include "creatures/players/vocations/vocation.hpp"
#include "config/configmanager.hpp"
#include "io/io_wheel.hpp"
#include "creatures/combat/spells.hpp"
#include "lib/logging/in_memory_logger.hpp"

TEST(PaladinStance, DistanceBonusUsesTotalAndFloorsWithoutOverflow) {
	EXPECT_EQ(PaladinStances::distanceSkill(PaladinStance::Sharpshooter, 100 + 12 + 3), 151);
	EXPECT_EQ(PaladinStances::distanceSkill(PaladinStance::None, 115), 115);
	EXPECT_EQ(PaladinStances::distanceSkill(PaladinStance::DivineDefiance, 115), 115);
	EXPECT_EQ(PaladinStances::distanceSkill(PaladinStance::Sharpshooter, -1), 0);
	EXPECT_EQ(PaladinStances::distanceSkill(PaladinStance::Sharpshooter, 65535), 65535);
	EXPECT_EQ(PaladinStances::distanceSkill(PaladinStance::None, 65536), 65535);
	EXPECT_EQ(PaladinStances::distanceSkill(PaladinStance::Sharpshooter, std::numeric_limits<int64_t>::max()), 65535);
}

TEST(PaladinStance, DivineDefianceFloorsDistanceConversionAndDoesNotStackWithSharpshooter) {
	EXPECT_EQ(PaladinStances::specializedMagic(PaladinStance::DivineDefiance, 116), 6);
	EXPECT_EQ(PaladinStances::specializedMagic(PaladinStance::DivineDefiance, 117), 7);
	EXPECT_EQ(PaladinStances::specializedMagic(PaladinStance::DivineDefiance, 65535), 3932);
	EXPECT_EQ(PaladinStances::specializedMagic(PaladinStance::Sharpshooter, 117), 0);
	EXPECT_EQ(PaladinStances::specializedMagic(PaladinStance::None, 117), 0);
}

TEST(PaladinStance, DodgeRequiresNonAdjacentAttackerOnSameFloor) {
	const Position defender(100, 100, 7);
	for (int dx = -2; dx <= 2; ++dx) {
		for (int dy = -2; dy <= 2; ++dy) {
			const Position attacker(100 + dx, 100 + dy, 7);
			EXPECT_EQ(PaladinStances::canDodge(PaladinStance::DivineDefiance, defender, attacker), std::abs(dx) > 1 || std::abs(dy) > 1);
			EXPECT_FALSE(PaladinStances::canDodge(PaladinStance::Sharpshooter, defender, attacker));
			EXPECT_FALSE(PaladinStances::canDodge(PaladinStance::None, defender, attacker));
		}
	}
	EXPECT_FALSE(PaladinStances::canDodge(PaladinStance::DivineDefiance, defender, Position(103, 100, 8)));
}

class PaladinStancePlayer : public ::testing::Test {
protected:
	static void SetUpTestSuite() {
		InMemoryLogger::install(injector);
		DI::setTestContainer(&injector);
		g_configManager().setConfigFileLua("config.lua.dist");
		ASSERT_TRUE(g_configManager().load());
		ASSERT_TRUE(g_vocations().loadFromXml());
	}
	void SetUp() override {
		player = std::make_shared<Player>();
		group = std::make_shared<Group>();
		group->flags[static_cast<size_t>(PlayerFlags_t::IsAlwaysPremium)] = true;
		player->setGroup(group);
		player->setLevel(20);
		ASSERT_TRUE(player->setVocation(VOCATION_ROYAL_PALADIN));
	}
	inline static di::extension::injector<> injector {};
	std::shared_ptr<Player> player;
	std::shared_ptr<Group> group;
};

TEST_F(PaladinStancePlayer, RealSkillAndSpecializedMagicFollowSelectionAndSkillChanges) {
	const auto distance = player->getSkillLevel(SKILL_DISTANCE);
	player->setVarSkill(SKILL_DISTANCE, 115 - distance);
	ASSERT_EQ(player->getSkillLevel(SKILL_DISTANCE), 115);
	const auto holy = player->getSpecializedMagicLevel(COMBAT_HOLYDAMAGE);
	const auto healing = player->getSpecializedMagicLevel(COMBAT_HEALING);
	const auto fire = player->getSpecializedMagicLevel(COMBAT_FIREDAMAGE);
	const auto magic = player->getMagicLevel();
	ASSERT_TRUE(player->setPaladinStance(PaladinStance::Sharpshooter));
	EXPECT_EQ(player->getSkillLevel(SKILL_DISTANCE), 151);
	ASSERT_TRUE(player->setPaladinStance(PaladinStance::DivineDefiance));
	EXPECT_EQ(player->getSkillLevel(SKILL_DISTANCE), 115);
	EXPECT_EQ(player->getSpecializedMagicLevel(COMBAT_HOLYDAMAGE), holy + 6);
	EXPECT_EQ(player->getSpecializedMagicLevel(COMBAT_HEALING), healing + 6);
	EXPECT_EQ(player->getSpecializedMagicLevel(COMBAT_FIREDAMAGE), fire);
	EXPECT_EQ(player->getMagicLevel(), magic);
	player->setVarSkill(SKILL_DISTANCE, 2);
	EXPECT_EQ(player->getSpecializedMagicLevel(COMBAT_HOLYDAMAGE), holy + 7);
	ASSERT_TRUE(player->setPaladinStance(PaladinStance::None));
	EXPECT_EQ(player->getSpecializedMagicLevel(COMBAT_HOLYDAMAGE), holy);
	EXPECT_EQ(player->getSkillLevel(SKILL_DISTANCE), 117);
}

TEST_F(PaladinStancePlayer, RejectsInvalidEligibilityAndClearsOnVocationChange) {
	player->setLevel(19);
	EXPECT_FALSE(player->setPaladinStance(PaladinStance::Sharpshooter));
	player->setLevel(20);
	group->flags[static_cast<size_t>(PlayerFlags_t::IsAlwaysPremium)] = false;
	EXPECT_FALSE(player->setPaladinStance(PaladinStance::Sharpshooter));
	group->flags[static_cast<size_t>(PlayerFlags_t::IsAlwaysPremium)] = true;
	ASSERT_TRUE(player->setPaladinStance(PaladinStance::Sharpshooter));
	EXPECT_FALSE(player->setPaladinStance(static_cast<PaladinStance>(255)));
	EXPECT_EQ(player->getPaladinStance(), PaladinStance::Sharpshooter);
	ASSERT_TRUE(player->setVocation(VOCATION_PALADIN));
	EXPECT_EQ(player->getPaladinStance(), PaladinStance::None);
	EXPECT_FALSE(player->setPaladinStance(PaladinStance::Sharpshooter));
	ASSERT_TRUE(player->setVocation(VOCATION_MASTER_SORCERER));
	EXPECT_FALSE(player->setPaladinStance(PaladinStance::DivineDefiance));
}

TEST_F(PaladinStancePlayer, SavedWheelSlotsGrantBarragesWithoutModifyingStanceCooldowns) {
	IOWheel io;
	ASSERT_TRUE(io.initializeGlobalData());
	PlayerWheelMethodsBonusData bonus;
	const auto &slots = io.getWheelMapFunctions();
	for (const auto &[slot, points] : std::array<std::pair<WheelSlots_t, uint16_t>, 4> {{
		{ WheelSlots_t::SLOT_RED_200, 200 }, { WheelSlots_t::SLOT_BLUE_50, 50 },
		{ WheelSlots_t::SLOT_GREEN_BOTTOM_100, 100 }, { WheelSlots_t::SLOT_PURPLE_MIDDLE_100, 100 }
	}}) {
		player->wheel().setPointsBySlotType(static_cast<uint8_t>(slot), points);
		slots.at(slot)(player, points, VOCATION_PALADIN_CIP, bonus);
	}
	EXPECT_EQ(std::ranges::count(bonus.spells, "Ethereal Barrage"), 2);
	EXPECT_EQ(std::ranges::count(bonus.spells, "Divine Barrage"), 2);
	EXPECT_EQ(std::ranges::count(bonus.spells, "Sharpshooter"), 0);
	EXPECT_EQ(std::ranges::count(bonus.spells, "Swift Foot"), 0);
	const auto &data = io.getWheelBonusData().spells.paladin;
	EXPECT_EQ(data[0].grade[1].leech.life, 10);
	EXPECT_EQ(data[0].grade[2].increase.criticalChance, 10);
	EXPECT_EQ(data[3].grade[1].increase.damage, 8);
	EXPECT_EQ(data[3].grade[2].increase.damage, 12);
	EXPECT_EQ(data[2].grade[1].increase.additionalTarget, 2);
	EXPECT_EQ(data[2].grade[2].decrease.cooldown, 8);
	EXPECT_EQ(data[4].grade[2].increase.damage, 10);
	std::unordered_map<std::pair<uint16_t, uint8_t>, double, PairHash, PairEqual> augments;
	for (uint8_t grade : { 1, 2 }) player->wheel().getAugmentsByVocation(302, "Divine Barrage", grade, data, augments);
	EXPECT_DOUBLE_EQ(augments[std::make_pair(302, magic_enum::enum_integer(Augment_t::BaseDamage))], 0.20);
}

TEST_F(PaladinStancePlayer, WheelCriticalChanceUsesHundredthsForEveryVocation) {
	for (const auto &name : { "Ethereal Barrage", "Great Fire Wave", "Greater Flurry of Blows" }) {
		auto spell = std::make_shared<InstantSpell>();
		spell->setName(name);
		spell->setWheelOfDestinyUpgraded(true);
		spell->setWheelOfDestinyBoost(WheelSpellBoost_t::CRITICAL_CHANCE, WheelSpellGrade_t::REGULAR, 10);
		g_spells().setInstantSpell(name, spell);
		player->wheel().upgradeSpell(name);
		CombatDamage damage;
		damage.instantSpellName = name;
		ASSERT_EQ(player->wheel().getCombatDataSpell(damage), spell);
		EXPECT_EQ(damage.criticalChance, 1000);
		WheelSpells::Bonus extra;
		extra.increase.criticalChance = 5;
		player->wheel().addSpellBonus(name, extra);
		damage.criticalChance = 0;
		player->wheel().getCombatDataSpell(damage);
		EXPECT_EQ(damage.criticalChance, 1500);
	}
	g_spells().clear();
}
