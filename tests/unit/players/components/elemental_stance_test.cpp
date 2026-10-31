/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/players/components/elemental_stance.hpp"
#include "creatures/combat/condition.hpp"
#include "creatures/combat/combat.hpp"

namespace {
	ElementalStanceState::Resolution castElement(ElementalStanceState &state, SpellElement natural, bool success = true) {
		ElementalStanceState::CastScope cast(state, "test spell");
		auto result = state.resolve("test spell", natural);
		cast.finish(success);
		return result;
	}
}

TEST(ElementalStance, RequiresNaturalCastBeforeConversionAndConsumesOnce) {
	ElementalStanceState state;
	state.select(SpellElement::Death);
	EXPECT_EQ(castElement(state, SpellElement::Energy).effective, SpellElement::Energy);
	EXPECT_EQ(castElement(state, SpellElement::Death).criticalDamage, 3000);
	EXPECT_TRUE(state.isCharged());
	auto beam = castElement(state, SpellElement::Energy);
	EXPECT_EQ(beam.effective, SpellElement::Death);
	EXPECT_EQ(beam.criticalDamage, 0); // Converted spells do not gain natural-element bonuses.
	EXPECT_FALSE(state.isCharged());
	EXPECT_EQ(castElement(state, SpellElement::Fire).effective, SpellElement::Fire);
}

TEST(ElementalStance, AllThreeElementsConvertTheOtherTwo) {
	for (auto stance : { SpellElement::Fire, SpellElement::Energy, SpellElement::Death }) {
		for (auto spell : { SpellElement::Fire, SpellElement::Energy, SpellElement::Death }) {
			ElementalStanceState state;
			state.select(stance);
			castElement(state, stance);
			EXPECT_EQ(castElement(state, spell).effective, stance);
			EXPECT_EQ(state.isCharged(), stance == spell);
		}
	}
}

TEST(ElementalStance, BonusesOnlyApplyToNaturalElement) {
	ElementalStanceState fire, energy;
	fire.select(SpellElement::Fire);
	energy.select(SpellElement::Energy);
	const auto flame = castElement(fire, SpellElement::Fire);
	EXPECT_EQ(flame.damagePercent, 4);
	EXPECT_EQ(flame.criticalChance, 0);
	EXPECT_EQ(castElement(energy, SpellElement::Energy).criticalChance, 400);
	EXPECT_EQ(castElement(fire, SpellElement::Energy).damagePercent, 0);
}

TEST(ElementalStance, FailedCastsNeitherChargeNorConsume) {
	ElementalStanceState state;
	state.select(SpellElement::Death);
	castElement(state, SpellElement::Death, false);
	EXPECT_FALSE(state.isCharged());
	castElement(state, SpellElement::Death);
	castElement(state, SpellElement::Energy, false);
	EXPECT_TRUE(state.isCharged());
}

TEST(ElementalStance, UnrelatedCombatAndNeutralSpellsLeaveChargeAlone) {
	ElementalStanceState state;
	state.select(SpellElement::Death);
	castElement(state, SpellElement::Death);
	EXPECT_EQ(state.resolve("rune", SpellElement::Fire).effective, SpellElement::Fire);
	{
		ElementalStanceState::CastScope cast(state, "healing spell");
		EXPECT_EQ(state.resolve("rune", SpellElement::Fire).effective, SpellElement::Fire);
		state.resolve("healing spell", SpellElement::None);
		cast.finish(true);
	}
	EXPECT_TRUE(state.isCharged());
}

TEST(ElementalStance, WholeAreaUsesOneDecisionAndSharedCombatDoesNotShareState) {
	ElementalStanceState first, second;
	first.select(SpellElement::Death);
	second.select(SpellElement::Fire);
	castElement(first, SpellElement::Death);
	{
		ElementalStanceState::CastScope cast(first, "beam");
		for (int tile = 0; tile < 8; ++tile) {
			EXPECT_EQ(first.resolve("beam", SpellElement::Energy).effective, SpellElement::Death);
		}
		EXPECT_EQ(castElement(second, SpellElement::Energy).effective, SpellElement::Energy);
		cast.finish(true);
	}
	EXPECT_FALSE(first.isCharged());
}

TEST(ElementalStance, SwitchingAndDeathInvalidateInFlightCharge) {
	ElementalStanceState state;
	state.select(SpellElement::Death);
	{
		ElementalStanceState::CastScope cast(state, "death spell");
		state.resolve("death spell", SpellElement::Death);
		state.select(SpellElement::Fire);
		cast.finish(true);
	}
	EXPECT_EQ(state.getSelected(), SpellElement::Fire);
	EXPECT_FALSE(state.isCharged());
	{
		ElementalStanceState::CastScope cast(state, "fire spell");
		state.resolve("fire spell", SpellElement::Fire);
		state.clearCharge();
		cast.finish(true);
	}
	EXPECT_FALSE(state.isCharged());
}

TEST(ElementalStance, NestedCastCannotRestoreStaleState) {
	ElementalStanceState state;
	state.select(SpellElement::Death);
	castElement(state, SpellElement::Death);
	{
		ElementalStanceState::CastScope outer(state, "death spell");
		state.resolve("death spell", SpellElement::Death);
		castElement(state, SpellElement::Fire);
		outer.finish(true);
	}
	EXPECT_FALSE(state.isCharged());
}

TEST(ElementalStance, ConditionConversionPreservesDamageAndOriginalTemplate) {
	ConditionDamage fire(CONDITIONID_COMBAT, CONDITION_FIRE);
	fire.addDamage(3, 3000, -45);
	const auto death = fire.cloneWithElement(CONDITION_CURSED);
	EXPECT_EQ(fire.getType(), CONDITION_FIRE);
	EXPECT_EQ(death->getType(), CONDITION_CURSED);
	EXPECT_EQ(death->getTotalDamage(), fire.getTotalDamage());
	EXPECT_EQ(fire.cloneWithElement(CONDITION_PARALYZE)->getType(), CONDITION_FIRE);
}

TEST(ElementalStance, FocusSpellsKeepTheirOfficialAnimationFamilies) {
	EXPECT_EQ(Combat::convertedImpact(CONST_ME_BIGCLOUDS, COMBAT_NONE), CONST_ME_BIGCLOUDS);
	EXPECT_EQ(Combat::convertedImpact(CONST_ME_BIGCLOUDS, COMBAT_ENERGYDAMAGE), CONST_ME_BIGCLOUDS);
	EXPECT_EQ(Combat::convertedImpact(CONST_ME_BIGCLOUDS, COMBAT_FIREDAMAGE), 339);
	EXPECT_EQ(Combat::convertedImpact(CONST_ME_BIGCLOUDS, COMBAT_DEATHDAMAGE), 340);
	EXPECT_EQ(Combat::convertedImpact(CONST_ME_FIREAREA, COMBAT_NONE), CONST_ME_FIREAREA);
	EXPECT_EQ(Combat::convertedImpact(CONST_ME_FIREAREA, COMBAT_FIREDAMAGE), CONST_ME_FIREAREA);
	EXPECT_EQ(Combat::convertedImpact(CONST_ME_FIREAREA, COMBAT_ENERGYDAMAGE), 337);
	EXPECT_EQ(Combat::convertedImpact(CONST_ME_FIREAREA, COMBAT_DEATHDAMAGE), 338);
	EXPECT_EQ(Combat::convertedImpact(CONST_ME_NONE, COMBAT_DEATHDAMAGE), CONST_ME_NONE);
}
