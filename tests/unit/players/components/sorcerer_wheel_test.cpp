/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/players/components/elemental_stance.hpp"
#include "creatures/players/components/wheel/player_wheel.hpp"
#include "creatures/players/player.hpp"
#include "io/io_wheel.hpp"
#include "lib/logging/in_memory_logger.hpp"

TEST(SorcererWheel, LordOfDestructionStagesOnlyBoostNaturalElements) {
	const std::array<uint16_t, 4> fire { 4, 6, 7, 8 }, energy { 400, 600, 700, 800 }, death { 3000, 4500, 5250, 6000 };
	for (uint8_t stage = 0; stage < 4; ++stage) {
		for (auto element : { SpellElement::Fire, SpellElement::Energy, SpellElement::Death }) {
			ElementalStanceState state;
			state.select(element);
			{
				ElementalStanceState::CastScope cast(state, "natural");
				auto value = state.resolve("natural", element, stage);
				EXPECT_EQ(value.damagePercent, element == SpellElement::Fire ? fire[stage] : 0);
				EXPECT_EQ(value.criticalChance, element == SpellElement::Energy ? energy[stage] : 0);
				EXPECT_EQ(value.criticalDamage, element == SpellElement::Death ? death[stage] : 0);
				cast.finish(true);
			}
			ElementalStanceState::CastScope cast(state, "converted");
			auto value = state.resolve("converted", element == SpellElement::Fire ? SpellElement::Death : SpellElement::Fire, stage);
			EXPECT_EQ(value.effective, element);
			EXPECT_EQ(value.damagePercent + value.criticalChance + value.criticalDamage, 0);
		}
	}
}

TEST(SorcererWheel, SideBeamsUseCardinalPerpendicularTilesAndBoundedCoordinates) {
	const Position center(100, 100, 7);
	for (auto direction : { DIRECTION_NORTH, DIRECTION_EAST, DIRECTION_SOUTH, DIRECTION_WEST }) {
		auto sides = WheelSpells::beamSidePositions(center, direction);
		ASSERT_EQ(sides.size(), 2);
		EXPECT_NE(sides[0], sides[1]);
		for (const auto &side : sides) {
			EXPECT_EQ(side.z, center.z);
			if (direction == DIRECTION_NORTH || direction == DIRECTION_SOUTH) {
				EXPECT_EQ(side.y, center.y);
				EXPECT_EQ(Position::getDistanceX(side, center), 1);
			} else {
				EXPECT_EQ(side.x, center.x);
				EXPECT_EQ(Position::getDistanceY(side, center), 1);
			}
		}
	}
	EXPECT_TRUE(WheelSpells::beamSidePositions(center, DIRECTION_NORTHEAST).empty());
	for (const auto &pos : { Position(0, 0, 7), Position(65534, 65534, 7) }) {
		EXPECT_EQ(WheelSpells::beamSidePositions(pos, DIRECTION_NORTH).size(), 1);
		EXPECT_EQ(WheelSpells::beamSidePositions(pos, DIRECTION_EAST).size(), 1);
	}
	EXPECT_EQ(WheelSpells::beamSideDamagePercent(0), 0);
	EXPECT_EQ(WheelSpells::beamSideDamagePercent(1), 25);
	EXPECT_EQ(WheelSpells::beamSideDamagePercent(2), 40);
	EXPECT_EQ(WheelSpells::beamSideDamagePercent(3), 70);
}

class SorcererWheelData : public ::testing::Test {
protected:
	static void SetUpTestSuite() {
		InMemoryLogger::install(injector);
		DI::setTestContainer(&injector);
	}
	inline static di::extension::injector<> injector {};
};

TEST_F(SorcererWheelData, SavedSlotsGrantReplacementSpellsAndCumulativeAugments) {
	IOWheel io;
	ASSERT_TRUE(io.initializeGlobalData());
	auto player = std::make_shared<Player>();
	PlayerWheelMethodsBonusData bonus;
	const auto &slots = io.getWheelMapFunctions();
	for (auto slot : { WheelSlots_t::SLOT_GREEN_MIDDLE_100, WheelSlots_t::SLOT_PURPLE_TOP_100,
	                   WheelSlots_t::SLOT_RED_MIDDLE_100, WheelSlots_t::SLOT_BLUE_MIDDLE_100 }) {
		player->wheel().setPointsBySlotType(static_cast<uint8_t>(slot), 100);
		slots.at(slot)(player, 100, VOCATION_SORCERER_CIP, bonus);
	}
	EXPECT_EQ(std::ranges::count(bonus.spells, "Death Echo"), 2);
	EXPECT_EQ(std::ranges::count(bonus.spells, "Magic Shield"), 0);
	EXPECT_EQ(std::ranges::count(bonus.spells, "Sap Strength"), 0);
	for (auto spell : WheelSpells::specialSpells) EXPECT_EQ(std::ranges::count(bonus.spells, spell), 2);
	const auto &data = io.getWheelBonusData().spells.sorcerer;
	EXPECT_EQ(data[0].grade[1].decrease.cooldown, 4);
	EXPECT_EQ(data[0].grade[2].increase.damage, 50);
	EXPECT_EQ(data[1].grade[1].decrease.cooldown, 2);
	EXPECT_EQ(data[1].grade[2].increase.damage, 12);
	EXPECT_TRUE(data[2].grade[1].increase.area);
	EXPECT_EQ(data[2].grade[2].increase.damage, 10);
	std::unordered_map<std::pair<uint16_t, uint8_t>, double, PairHash, PairEqual> augments;
	for (uint8_t grade : { 1, 2 }) player->wheel().getAugmentsByVocation(310, "Death Echo", grade, data, augments);
	EXPECT_DOUBLE_EQ(augments[std::make_pair(310, magic_enum::enum_integer(Augment_t::Cooldown))], -2);
	EXPECT_DOUBLE_EQ(augments[std::make_pair(310, magic_enum::enum_integer(Augment_t::BaseDamage))], 0.12);
	augments.clear();
	for (uint8_t grade : { 1, 2 }) player->wheel().getAugmentsByVocation(1, "Lightning", grade, data, augments);
	EXPECT_DOUBLE_EQ(augments[std::make_pair(1, magic_enum::enum_integer(Augment_t::Cooldown))], -4);
	EXPECT_DOUBLE_EQ(augments[std::make_pair(1, magic_enum::enum_integer(Augment_t::BaseDamage))], 0.5);
	EXPECT_FALSE(WheelSpells::matchesSpell("Special Spells", "Ultimate Energy Strike"));
	PlayerWheelMethodsBonusData mitigation;
	slots.at(WheelSlots_t::SLOT_GREEN_TOP_150)(player, 100, VOCATION_SORCERER_CIP, mitigation);
	EXPECT_DOUBLE_EQ(mitigation.mitigation, 7.5);

}
