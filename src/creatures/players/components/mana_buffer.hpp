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
#endif

// Player-owned, synchronous state: no scheduled work or external lifetime.
class ManaBufferState {
public:
	struct Result {
		uint32_t manaSpent = 0;
		bool survived = false;
	};

	Result consume(int64_t damage, int32_t health, uint32_t mana, uint32_t maximumMana, uint64_t now) {
		if (health <= 0 || damage < health || mana == 0) return {};
		// Damage is a sum of two int32 components. Clamp arbitrary callers too.
		const auto lethal = static_cast<uint64_t>(std::min<int64_t>(damage, int64_t(INT32_MAX) * 2) - health + 1);
		uint64_t cost = lethal * 10;
		if (now >= nextSurchargeAt) {
			cost += (uint64_t(maximumMana) + 3) / 4;
			nextSurchargeAt = now > UINT64_MAX - 2000 ? UINT64_MAX : now + 2000;
		}
		return { static_cast<uint32_t>(std::min<uint64_t>(mana, cost)), uint64_t(mana) > cost };
	}

	void reset() { nextSurchargeAt = 0; }

private:
	uint64_t nextSurchargeAt = 0;
};
