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
	#include <cstdint>
#endif

enum class DruidStance : uint8_t {
	None,
	SharedConservation,
	ElementalSynthesis
};

namespace DruidStances {
	// Use general magic only; specialized magic must never feed back into this bonus.
	constexpr uint32_t specializedMagic(DruidStance stance, uint32_t magic) {
		return stance == DruidStance::ElementalSynthesis ? magic / 10 : 0;
	}
}
