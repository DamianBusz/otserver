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
	#include <optional>
#endif

// Owned by the victim, never by a delayed callback or a recyclable creature ID.
class AttackReductionState {
	struct Snapshot {
		bool resolved = false;
		uint8_t percent = 0;
	};
public:
	AttackReductionState() = default;
	AttackReductionState(const AttackReductionState &) = delete;
	AttackReductionState &operator=(const AttackReductionState &) = delete;
	AttackReductionState(AttackReductionState &&) = delete;
	AttackReductionState &operator=(AttackReductionState &&) = delete;

	void apply(uint8_t percent, uint64_t now) {
		if (percent != 50 && percent != 75) return;
		// Reapplication refreshes duration without weakening an existing Slam II.
		pending = expires > now ? std::max(pending, percent) : percent;
		expires = now > UINT64_MAX - 10000 ? UINT64_MAX : now + 10000;
	}
	void clear() {
		pending = 0;
		expires = 0;
		current.reset();
		++epoch;
	}
	uint8_t snapshot(uint64_t now) {
		if (!current) return consume(now);
		if (!current->resolved) {
			current->percent = consume(now);
			current->resolved = true;
		}
		return current->percent;
	}

	// Synchronous whole-swing scope, encompassing cleave and scripted ammunition.
	// Failed/out-of-range attempts preserve pending state; a successful miss consumes it.
	class Swing {
	public:
		Swing(AttackReductionState &owner, uint64_t now) : state(owner), previous(owner.current), generation(owner.epoch), time(now) {
			state.current = Snapshot {};
		}
		~Swing() {
			if (state.epoch == generation) state.current = previous;
		}
		Swing(const Swing &) = delete;
		Swing &operator=(const Swing &) = delete;
		Swing(Swing &&) = delete;
		Swing &operator=(Swing &&) = delete;
		void finish(bool success) {
			if (success && state.epoch == generation) (void)state.snapshot(time);
		}
	private:
		AttackReductionState &state;
		std::optional<Snapshot> previous;
		uint64_t generation;
		uint64_t time;
	};
private:
	uint8_t consume(uint64_t now) {
		const uint8_t result = now < expires ? pending : 0;
		pending = 0;
		expires = 0;
		return result;
	}
	uint8_t pending = 0;
	uint64_t expires = 0;
	uint64_t epoch = 0;
	std::optional<Snapshot> current;
};
