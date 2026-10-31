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
	#include <string_view>
#endif

enum class SpellElement : uint8_t {
	None,
	Fire,
	Energy,
	Death
};

// Player-owned state. A cast samples the selected stance and commits its charge
// only after successful execution. Combat objects are shared by all casters and
// must never own this state or have their parameters mutated for a conversion.
class ElementalStanceState final {
public:
	struct Resolution {
		SpellElement natural = SpellElement::None;
		SpellElement effective = SpellElement::None;
		uint16_t criticalChance = 0; // hundredths of a percentage point
		uint16_t criticalDamage = 0;
		uint16_t damagePercent = 0;
	};

	ElementalStanceState() = default;
	ElementalStanceState(const ElementalStanceState &) = delete;
	ElementalStanceState &operator=(const ElementalStanceState &) = delete;
	ElementalStanceState(ElementalStanceState &&) = delete;
	ElementalStanceState &operator=(ElementalStanceState &&) = delete;

	void select(SpellElement element) {
		selected = element;
		charged = false;
		++revision;
	}
	SpellElement getSelected() const {
		return selected;
	}
	bool isCharged() const {
		return charged;
	}
	void clearCharge() {
		charged = false;
		++revision;
	}

	// Strictly synchronous scope: never retain it or its references in deferred
	// work. Delayed spells must carry a value copy of Resolution instead.
	class CastScope final {
	public:
		CastScope(ElementalStanceState &state, std::string_view name) :
			owner(state), previous(state.active), spellName(name), stance(state.selected),
			wasCharged(state.charged), generation(state.revision) {
			owner.active = this;
		}
		~CastScope() {
			owner.active = previous;
		}
		CastScope(const CastScope &) = delete;
		CastScope &operator=(const CastScope &) = delete;
		CastScope(CastScope &&) = delete;
		CastScope &operator=(CastScope &&) = delete;

		void finish(bool success) {
			if (finished || !success || !resolved || owner.revision != generation) {
				finished = true;
				return;
			}
			owner.charged = resolution.natural == stance;
			++owner.revision;
			finished = true;
		}

	private:
		friend class ElementalStanceState;
		ElementalStanceState &owner;
		CastScope* previous;
		std::string_view spellName;
		SpellElement stance;
		bool wasCharged;
		uint64_t generation;
		bool resolved = false;
		bool finished = false;
		Resolution resolution;
	};

	Resolution resolve(std::string_view spellName, SpellElement natural, uint8_t lordStage = 0) {
		Resolution unchanged { natural, natural };
		if (!active || active->finished || spellName.empty() || active->spellName != spellName
		    || natural == SpellElement::None || active->stance == SpellElement::None
		    || active->generation != revision) {
			return unchanged;
		}
		if (active->resolved) {
			// The same cast may execute several combat objects or hit many targets.
			return active->resolution.natural == natural ? active->resolution : unchanged;
		}
		active->resolved = true;
		auto &result = active->resolution;
		result = unchanged;
		if (natural == active->stance) {
			if (natural == SpellElement::Death) {
				result.criticalDamage = 3000 + (lordStage >= 3 ? 3000 : lordStage == 2 ? 2250 : lordStage == 1 ? 1500 : 0);
			} else if (natural == SpellElement::Energy) {
				result.criticalChance = 400 + (lordStage >= 3 ? 400 : lordStage == 2 ? 300 : lordStage == 1 ? 200 : 0);
			} else {
				result.damagePercent = 4 + (lordStage >= 3 ? 4 : lordStage == 2 ? 3 : lordStage == 1 ? 2 : 0);
			}
		} else if (active->wasCharged) {
			result.effective = active->stance;
		}
		return result;
	}

private:
	SpellElement selected = SpellElement::None;
	bool charged = false;
	uint64_t revision = 0;
	CastScope* active = nullptr;
};
