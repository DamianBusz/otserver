/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#pragma once

#include "game/movement/position.hpp"

#ifndef USE_PRECOMPILED_HEADERS
	#include <algorithm>
	#include <cstdint>
	#include <limits>
#endif

enum class PaladinStance : uint8_t {
	None,
	Sharpshooter,
	DivineDefiance
};

namespace PaladinStances {
	// Apply once, after equipment, conditions, Wheel and proficiency bonuses.
	constexpr uint16_t distanceSkill(PaladinStance stance, int64_t total) {
		const auto value = static_cast<uint64_t>(std::clamp<int64_t>(total, 0, std::numeric_limits<uint16_t>::max()));
		const auto boosted = stance == PaladinStance::Sharpshooter ? value * 132 / 100 : value;
		return static_cast<uint16_t>(std::min<uint64_t>(boosted, std::numeric_limits<uint16_t>::max()));
	}

	constexpr uint16_t specializedMagic(PaladinStance stance, uint16_t distance) {
		return stance == PaladinStance::DivineDefiance ? static_cast<uint32_t>(distance) * 6 / 100 : 0;
	}

	inline bool canDodge(PaladinStance stance, const Position &defender, const Position &attacker) {
		return stance == PaladinStance::DivineDefiance && defender.z == attacker.z
			&& (Position::getDistanceX(defender, attacker) > 1 || Position::getDistanceY(defender, attacker) > 1);
	}
}
