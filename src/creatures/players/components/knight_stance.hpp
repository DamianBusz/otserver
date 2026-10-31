/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#pragma once

#include "creatures/creatures_definitions.hpp"

#ifndef USE_PRECOMPILED_HEADERS
	#include <algorithm>
	#include <cstdint>
	#include <limits>
#endif

enum class KnightStance : uint8_t {
	None,
	BloodRage,
	Protector
};

namespace KnightStances {
	// Apply after equipment, conditions, loyalty, Wheel and proficiency, once.
	constexpr uint16_t skill(KnightStance stance, skills_t type, int64_t total) {
		const auto value = std::clamp<int64_t>(total, 0, std::numeric_limits<uint16_t>::max());
		const int percent = stance == KnightStance::BloodRage && type >= SKILL_CLUB && type <= SKILL_AXE ? 125
			: stance == KnightStance::Protector && type == SKILL_SHIELD ? 130 : 100;
		return static_cast<uint16_t>(std::min<int64_t>(value * percent / 100, std::numeric_limits<uint16_t>::max()));
	}

	constexpr int outgoingPercent(KnightStance stance) {
		return stance == KnightStance::Protector ? 85 : 100;
	}

	constexpr int incomingPercent(KnightStance stance) {
		return stance == KnightStance::BloodRage ? 115 : stance == KnightStance::Protector ? 85 : 100;
	}

	// Combat's drain path uses positive magnitudes. Widen before multiplying.
	constexpr int32_t scaleDamage(int32_t value, int percent) {
		return static_cast<int32_t>(std::clamp<int64_t>(int64_t(value) * percent / 100, 0, INT32_MAX));
	}
}

namespace KnightWheel {
	// Preserve proportional missing-HP scaling; widen before subtracting health.
	inline double masteryPercent(uint8_t stage, int32_t health, int32_t maximum, bool doubled) {
		if (stage == 0 || maximum <= 0) return 0;
		const int threshold = stage >= 3 ? 10 : stage == 2 ? 12 : 14;
		const auto missing = std::clamp<int64_t>(int64_t(maximum) - health, 0, maximum);
		return double(missing) * 100.0 * (doubled ? 2 : 1) / (double(maximum) * threshold);
	}
	inline int32_t scaleDamage(int32_t value, double percent) {
		return static_cast<int32_t>(std::clamp(double(value) * percent / 100.0, 0.0, double(INT32_MAX)));
	}
}
