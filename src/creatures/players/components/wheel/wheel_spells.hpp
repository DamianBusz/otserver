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
	#include <array>
	#include <cstdint>
	#include <string_view>
	#include <vector>
#endif

namespace WheelSpells {
	inline constexpr std::array<std::string_view, 2> forkedSpells { "Forked Glacier", "Forked Thorns" };
	inline constexpr std::array<std::string_view, 3> specialSpells { "Lightning", "Strong Energy Strike", "Strong Flame Strike" };

	inline bool matchesSpell(std::string_view group, std::string_view name) {
		if (group == name) return true;
		if (group == "Forked Spells") {
			for (auto spell : forkedSpells) if (spell == name) return true;
		}
		if (group == "Special Spells") {
			for (auto spell : specialSpells) if (spell == name) return true;
		}
		return group == "Any_Focus_Mage_Spell" &&
		       (name == "Hell's Core" || name == "Rage of the Skies" || name == "Eternal Winter" || name == "Wrath of Nature");
	}

	inline constexpr uint8_t beamSideDamagePercent(uint8_t stage) {
		return stage >= 3 ? 70 : stage == 2 ? 40 : stage == 1 ? 25 : 0;
	}

	// The side strips stay perpendicular to the cardinal beam, with no unsigned
	// coordinate wrapping at the map boundaries.
	inline std::vector<Position> beamSidePositions(const Position &center, Direction direction) {
		std::vector<Position> result;
		const bool vertical = direction == DIRECTION_NORTH || direction == DIRECTION_SOUTH;
		const bool horizontal = direction == DIRECTION_EAST || direction == DIRECTION_WEST;
		if (!vertical && !horizontal) return result;
		for (int delta : { -1, 1 }) {
			const int x = int(center.x) + (vertical ? delta : 0);
			const int y = int(center.y) + (horizontal ? delta : 0);
			if (x >= 0 && x < 65535 && y >= 0 && y < 65535) {
				result.emplace_back(x, y, center.z);
			}
		}
		return result;
	}

	struct Increase {
		bool area = false;
		double damage = 0; // percentage points; preserve fractional grades
		int heal = 0;
		int additionalTarget = 0;
		int damageReduction = 0;
		int duration = 0;
		int criticalDamage = 0;
		int criticalChance = 0; // percentage points; CombatDamage stores hundredths
	};

	struct Decrease {
		int cooldown = 0;
		int manaCost = 0;
		int secondaryGroupCooldown = 0;
	};

	struct Leech {
		int mana = 0;
		int life = 0;
	};

	struct Bonus {
		Leech leech;
		Increase increase;
		Decrease decrease;
	};
}
