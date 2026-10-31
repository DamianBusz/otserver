/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/players/components/druid_stance.hpp"
#include "creatures/players/player.hpp"
#include "creatures/players/grouping/groups.hpp"
#include "creatures/players/vocations/vocation.hpp"
#include "config/configmanager.hpp"
#include "io/io_wheel.hpp"
#include "creatures/combat/spells.hpp"
#include "lib/logging/in_memory_logger.hpp"

TEST(DruidStance, SpecializedMagicUsesGeneralMagicAndFloorsWithoutOverflow) {
	EXPECT_EQ(DruidStances::specializedMagic(DruidStance::ElementalSynthesis, 119), 11);
	EXPECT_EQ(DruidStances::specializedMagic(DruidStance::ElementalSynthesis, 120), 12);
	EXPECT_EQ(DruidStances::specializedMagic(DruidStance::ElementalSynthesis, 0), 0);
	EXPECT_EQ(DruidStances::specializedMagic(DruidStance::ElementalSynthesis, UINT32_MAX), UINT32_MAX / 10);
	EXPECT_EQ(DruidStances::specializedMagic(DruidStance::SharedConservation, 120), 0);
	EXPECT_EQ(DruidStances::specializedMagic(DruidStance::None, 120), 0);
}

class DruidStancePlayer : public ::testing::Test {
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
		ASSERT_TRUE(player->setVocation(VOCATION_ELDER_DRUID));
	}
	inline static di::extension::injector<> injector {};
	std::shared_ptr<Player> player;
	std::shared_ptr<Group> group;
};

TEST_F(DruidStancePlayer, GeneralMagicChangesRecomputeOnlyIceAndEarthWithoutCompounding) {
	player->setVarStats(STAT_MAGICPOINTS, 119 - player->getMagicLevel());
	const auto ice = player->getSpecializedMagicLevel(COMBAT_ICEDAMAGE);
	const auto earth = player->getSpecializedMagicLevel(COMBAT_EARTHDAMAGE);
	const auto healing = player->getSpecializedMagicLevel(COMBAT_HEALING);
	ASSERT_TRUE(player->setDruidStance(DruidStance::ElementalSynthesis));
	EXPECT_EQ(player->getSpecializedMagicLevel(COMBAT_ICEDAMAGE), ice + 11);
	EXPECT_EQ(player->getSpecializedMagicLevel(COMBAT_EARTHDAMAGE), earth + 11);
	EXPECT_EQ(player->getSpecializedMagicLevel(COMBAT_HEALING), healing);
	EXPECT_EQ(player->getMagicLevel(), 119);
	player->setVarStats(STAT_MAGICPOINTS, 1);
	EXPECT_EQ(player->getSpecializedMagicLevel(COMBAT_ICEDAMAGE), ice + 12);
	ASSERT_TRUE(player->setDruidStance(DruidStance::ElementalSynthesis));
	EXPECT_EQ(player->getSpecializedMagicLevel(COMBAT_ICEDAMAGE), ice + 12);
	ASSERT_TRUE(player->setDruidStance(DruidStance::SharedConservation));
	EXPECT_EQ(player->getSpecializedMagicLevel(COMBAT_ICEDAMAGE), ice);
	ASSERT_TRUE(player->setDruidStance(DruidStance::None));
	EXPECT_EQ(player->getMagicLevel(), 120);
}

TEST_F(DruidStancePlayer, EligibilityAndVocationInvalidation) {
	player->setLevel(19);
	EXPECT_FALSE(player->setDruidStance(DruidStance::SharedConservation));
	player->setLevel(20);
	group->flags[static_cast<size_t>(PlayerFlags_t::IsAlwaysPremium)] = false;
	EXPECT_FALSE(player->setDruidStance(DruidStance::ElementalSynthesis));
	group->flags[static_cast<size_t>(PlayerFlags_t::IsAlwaysPremium)] = true;
	ASSERT_TRUE(player->setDruidStance(DruidStance::ElementalSynthesis));
	EXPECT_FALSE(player->setDruidStance(static_cast<DruidStance>(255)));
	EXPECT_EQ(player->getDruidStance(), DruidStance::ElementalSynthesis);
	ASSERT_TRUE(player->setVocation(VOCATION_DRUID));
	EXPECT_EQ(player->getDruidStance(), DruidStance::None);
	EXPECT_FALSE(player->setDruidStance(DruidStance::ElementalSynthesis));
	ASSERT_TRUE(player->setVocation(VOCATION_MASTER_SORCERER));
	EXPECT_FALSE(player->setDruidStance(DruidStance::SharedConservation));
	EXPECT_TRUE(player->setDruidStance(DruidStance::None));
}

TEST_F(DruidStancePlayer, SavedWheelSlotsGrantBothForkedSpellsAndPreserveOtherVocations) {
	IOWheel io;
	ASSERT_TRUE(io.initializeGlobalData());
	PlayerWheelMethodsBonusData bonus;
	for (auto slot : { WheelSlots_t::SLOT_RED_200, WheelSlots_t::SLOT_BLUE_50 }) {
		const auto points = slot == WheelSlots_t::SLOT_RED_200 ? 200 : 50;
		player->wheel().setPointsBySlotType(static_cast<uint8_t>(slot), points);
		io.getWheelMapFunctions().at(slot)(player, points, VOCATION_DRUID_CIP, bonus);
	}
	EXPECT_EQ(std::ranges::count(bonus.spells, "Forked Glacier"), 2);
	EXPECT_EQ(std::ranges::count(bonus.spells, "Forked Thorns"), 2);
	EXPECT_EQ(std::ranges::count(bonus.spells, "Nature's Embrace"), 0);
	const auto &data = io.getWheelBonusData().spells.druid;
	EXPECT_EQ(data[0].grade[1].increase.damage, 6);
	EXPECT_TRUE(data[0].grade[2].increase.area);
	EXPECT_EQ(data[0].grade[1].leech.mana, 0);
	EXPECT_EQ(data[2].grade[1].decrease.cooldown, 2); // declaration units are seconds
	EXPECT_EQ(data[2].grade[2].increase.additionalTarget, 1);
	EXPECT_EQ(data[3].grade[2].leech.life, 10);
	EXPECT_DOUBLE_EQ(data[3].grade[1].increase.damage, 6.5);
	EXPECT_DOUBLE_EQ(io.getWheelBonusData().spells.knight[1].grade[2].increase.damage, 12.5);
	EXPECT_EQ(data[4].grade[1].increase.heal, 4);
	EXPECT_EQ(data[4].grade[2].increase.heal, 6);
	EXPECT_EQ(data[4].grade[1].decrease.manaCost, 0);
	for (auto name : WheelSpells::forkedSpells) {
		EXPECT_TRUE(WheelSpells::matchesSpell("Forked Spells", name));
		EXPECT_FALSE(WheelSpells::matchesSpell("Special Spells", name));
	}
	EXPECT_FALSE(WheelSpells::matchesSpell("Forked Spells", "Nature's Embrace"));
	std::unordered_map<std::pair<uint16_t, uint8_t>, double, PairHash, PairEqual> augments;
	for (uint8_t grade : { 1, 2 }) player->wheel().getAugmentsByVocation(84, "Heal Friend", grade, data, augments);
	EXPECT_DOUBLE_EQ(augments[std::make_pair(84, magic_enum::enum_integer(Augment_t::BaseHealing))], 0.10);
}

TEST_F(DruidStancePlayer, OfficialDruidWheelPositionsGrantOnlyTheirAssignedSpellAtFullPoints) {
	IOWheel io;
	ASSERT_TRUE(io.initializeGlobalData());
	// Official installed client: table 0x237cd94, druid column, stable 1-based slots.
	struct ExpectedSlot {
		WheelSlots_t slot;
		int maximum;
		const char *spell;
	};
	for (const auto &[slot, maximum, spell] : {
		     ExpectedSlot { WheelSlots_t::SLOT_RED_50, 50, "Strong Ice Wave" },
		     ExpectedSlot { WheelSlots_t::SLOT_BLUE_200, 200, "Strong Ice Wave" },
		     ExpectedSlot { WheelSlots_t::SLOT_RED_MIDDLE_100, 100, "Heal Friend" },
		     ExpectedSlot { WheelSlots_t::SLOT_BLUE_MIDDLE_100, 100, "Heal Friend" } }) {
		for (const auto points : { maximum - 1, maximum }) {
			PlayerWheelMethodsBonusData bonus;
			player->wheel().setPointsBySlotType(static_cast<uint8_t>(slot), points);
			io.getWheelMapFunctions().at(slot)(player, points, VOCATION_DRUID_CIP, bonus);
			if (points == maximum) {
				ASSERT_EQ(bonus.spells.size(), 1);
				EXPECT_EQ(bonus.spells.front(), spell);
			} else {
				EXPECT_TRUE(bonus.spells.empty());
			}
		}
	}
}
