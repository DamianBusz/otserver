/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#pragma once

#ifndef USE_PRECOMPILED_HEADERS
	#include <algorithm>
	#include <cstdint>
	#include <limits>
	#include <string_view>
#endif

namespace DruidHealing {
	constexpr bool isSharedSpell(std::string_view name) {
		return name == "Heal Friend" || name == "Nature's Embrace";
	}

	constexpr int32_t fraction(int32_t value, int64_t basisPoints) {
		const int64_t amount = std::max<int64_t>(0, value) * std::clamp<int64_t>(basisPoints, 0, INT32_MAX) / 10000;
		return static_cast<int32_t>(std::min<int64_t>(amount, INT32_MAX));
	}

	constexpr int32_t add(int32_t value, int64_t extra) {
		return static_cast<int32_t>(std::clamp<int64_t>(int64_t(value) + extra, 0, INT32_MAX));
	}

	constexpr int32_t boost(int32_t value, int64_t basisPoints) {
		return add(value, fraction(value, basisPoints));
	}

	constexpr int32_t groveBasisPoints(uint8_t stage, int32_t health, int32_t maximum) {
		if (stage == 0 || maximum <= 0 || health <= 0 || int64_t(health) * 100 >= int64_t(maximum) * 60) return 0;
		const int32_t base = stage >= 3 ? 1000 : stage == 2 ? 750 : 500;
		return int64_t(health) * 100 < int64_t(maximum) * 30 ? base * 2 : base;
	}
}
