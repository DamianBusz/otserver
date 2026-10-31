/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 */
#include "creatures/players/components/weapon_proficiency.hpp"
#include "creatures/players/player.hpp"
#include "items/tile.hpp"
#include "items/cylinder.hpp"
#include "kv/value_wrapper.hpp"
#include "utils/tools.hpp"

#ifndef USE_PRECOMPILED_HEADERS
	#include <algorithm>
	#include <cmath>
	#include <limits>
	#include <set>
#endif

std::map<uint16_t, WeaponProficiency::ShapeDefinition> WeaponProficiency::shapeDefinitions;

namespace {
constexpr uint16_t ASCENSION_ORB = 53695;
constexpr uint8_t MAX_SHAPES = 2;
constexpr uint8_t MAX_RANK = 10;
constexpr uint16_t RESHAPE_COST = 250;
constexpr std::array<uint16_t, MAX_RANK> REFINE_COSTS { 125, 200, 275, 350, 425, 500, 575, 650, 725, 800 };

bool readUnsigned(const nlohmann::json &j, std::string_view key, uint32_t maximum, uint32_t &out) {
	const auto it = j.find(key);
	if (it == j.end() || !it->is_number_integer()) {
		return false;
	}
	const auto n = it->get<int64_t>();
	if (n < 0 || static_cast<uint64_t>(n) > maximum) {
		return false;
	}
	out = static_cast<uint32_t>(n);
	return true;
}
}

std::string WeaponProficiency::serializeShapes(const WeaponProficiencyData &data) {
	nlohmann::json rows = nlohmann::json::array();
	for (const auto &shape : data.shapes) {
		rows.push_back({ { "level", shape.level }, { "index", shape.index }, { "id", shape.id }, { "rank", shape.rank }, { "offers", shape.offers } });
	}
	return nlohmann::json { { "version", 1 }, { "revision", data.shapingRevision }, { "shapes", rows } }.dump();
}

void WeaponProficiency::deserializeShapes(WeaponProficiencyData &data, const std::string &text) {
	data.shapes.clear();
	data.shapingRevision = 0;
	if (text.empty() || text.size() > 4096) {
		return;
	}
	const auto j = nlohmann::json::parse(text, nullptr, false);
	uint32_t version = 0;
	if (!j.is_object() || !readUnsigned(j, "version", 1, version) || version != 1 || !j.contains("shapes") || !j["shapes"].is_array() || j["shapes"].size() > MAX_SHAPES) {
		return;
	}
	uint32_t revision = 0;
	if (!readUnsigned(j, "revision", UINT32_MAX, revision)) {
		return;
	}
	data.shapingRevision = revision;
	for (const auto &row : j["shapes"]) {
		uint32_t level = 0, index = 0, id = 0, rank = 0;
		if (!row.is_object() || !readUnsigned(row, "level", 6, level) || !readUnsigned(row, "index", 2, index) || !readUnsigned(row, "id", UINT16_MAX, id) || !readUnsigned(row, "rank", MAX_RANK, rank)) {
			continue;
		}
		ProficiencyShape shape { static_cast<uint8_t>(level), static_cast<uint8_t>(index), static_cast<uint8_t>(rank), static_cast<uint16_t>(id), {} };
		if (row.contains("offers") && row["offers"].is_array() && row["offers"].size() <= 3) {
			for (const auto &offer : row["offers"]) {
				if (offer.is_number_integer() && offer.get<int64_t>() > 0 && offer.get<int64_t>() <= UINT16_MAX) {
					shape.offers.push_back(offer.get<uint16_t>());
				}
			}
		}
		data.shapes.push_back(std::move(shape));
	}
}

uint8_t WeaponProficiency::shapingVocation(uint16_t weaponId) const {
	if (!isValidWeaponId(weaponId)) {
		return 0;
	}
	const auto &item = Item::items[weaponId];
	const auto vocation = asLowerCaseString(item.vocationString);
	if (vocation.find("druid") != std::string::npos) return 8;
	if (vocation.find("sorcerer") != std::string::npos) return 4;
	if (vocation.find("monk") != std::string::npos) return 16;
	if (vocation.find("paladin") != std::string::npos) return 2;
	if (vocation.find("knight") != std::string::npos) return 1;
	// Low-level weapons can have no vocation restriction.
	switch (item.weaponType) {
		case WEAPON_SWORD: case WEAPON_AXE: case WEAPON_CLUB: return 1;
		case WEAPON_DISTANCE: return 2;
		case WEAPON_FIST: return 16;
		default: return 0;
	}
}

void WeaponProficiency::normalizeShapes(uint16_t weaponId) {
	auto &data = proficiency.at(weaponId);
	const auto treeIt = proficiencies.find(Item::items[weaponId].proficiencyId);
	if (treeIt == proficiencies.end()) {
		data.shapes.clear();
		return;
	}
	const auto vocation = shapingVocation(weaponId);
	std::set<uint8_t> used;
	std::erase_if(data.shapes, [&](ProficiencyShape &shape) {
		const auto definition = shapeDefinitions.find(shape.id);
		if (shape.rank > MAX_RANK || shape.level >= getUnlockedLevelCount(weaponId) || shape.level >= treeIt->second.level.size() || shape.index >= treeIt->second.level[shape.level].perks.size() || definition == shapeDefinitions.end() || (definition->second.vocation && definition->second.vocation != vocation) || !used.insert(shape.level).second) {
			return true;
		}
		std::set<uint16_t> seen;
		std::erase_if(shape.offers, [&](uint16_t id) {
			const auto it = shapeDefinitions.find(id);
			return id == shape.id || it == shapeDefinitions.end() || (it->second.vocation && it->second.vocation != vocation) || !seen.insert(id).second;
		});
		return false;
	});
	if (data.shapes.size() > MAX_SHAPES) data.shapes.resize(MAX_SHAPES);
}

ProficiencyPerk WeaponProficiency::shapedPerk(uint16_t weaponId, const ProficiencyPerk &base) const {
	const auto &shapes = proficiency.at(weaponId).shapes;
	for (const auto &shape : shapes) {
		if (shape.level != base.level || shape.index != base.index) continue;
		const auto definition = shapeDefinitions.find(shape.id);
		if (definition == shapeDefinitions.end() || shape.rank > MAX_RANK) continue;
		auto perk = definition->second.perk;
		perk.level = base.level;
		perk.index = base.index;
		perk.value += (definition->second.maximum - perk.value) * shape.rank / MAX_RANK;
		return perk;
	}
	return base;
}

nlohmann::json WeaponProficiency::shapingState(uint16_t weaponId) const {
	nlohmann::json state { { "version", 1 }, { "weaponId", weaponId }, { "dust", m_player.getForgeDusts() }, { "orbs", static_cast<const Cylinder &>(m_player).getItemTypeCount(ASCENSION_ORB) }, { "shapes", nlohmann::json::array() }, { "pool", nlohmann::json::array() }, { "revision", 0 }, { "unlocked", getUnlockedLevelCount(weaponId) }, { "mastered", false }, { "modifyCost", 250 }, { "reshapeCost", RESHAPE_COST }, { "refineCosts", REFINE_COSTS }, { "protectionZone", m_player.getTile() && m_player.getTile()->hasFlag(TILESTATE_PROTECTIONZONE) } };
	if (!isValidWeaponId(weaponId)) return state;
	const auto vocation = shapingVocation(weaponId);
	for (const auto &[id, definition] : shapeDefinitions) {
		if (!definition.vocation || definition.vocation == vocation) state["pool"].push_back(definition.client);
	}
	const auto dataIt = proficiency.find(weaponId);
	if (dataIt == proficiency.end()) return state;
	const auto &data = dataIt->second;
	state["revision"] = data.shapingRevision;
	state["mastered"] = getMaxExperience(weaponId) > 0 && data.experience >= getMaxExperience(weaponId);
	state["modifyCost"] = data.shapes.empty() ? 250 : 1000;
	for (const auto &shape : data.shapes) {
		state["shapes"].push_back({ { "level", shape.level }, { "index", shape.index }, { "id", shape.id }, { "rank", shape.rank }, { "offers", shape.offers } });
	}
	return state;
}

nlohmann::json WeaponProficiency::shapingRequest(const nlohmann::json &request) {
	uint32_t weapon = 0;
	if (!request.is_object() || !readUnsigned(request, "weaponId", UINT16_MAX, weapon) || !isValidWeaponId(weapon) || !proficiencies.contains(Item::items[weapon].proficiencyId)) {
		return { { "version", 1 }, { "error", "Invalid weapon." } };
	}
	const auto weaponId = static_cast<uint16_t>(weapon);
	auto reject = [&](const std::string &reason) {
		auto state = shapingState(weaponId); state["error"] = reason; return state;
	};
	uint32_t version = 0;
	if (!readUnsigned(request, "version", 1, version) || version != 1) return reject("Unsupported shaping protocol version.");
	if (!request.contains("action") || !request["action"].is_string()) return reject("Invalid action.");
	const auto action = request["action"].get<std::string>();
	if (action == "state") return shapingState(weaponId);
	if (!m_player.getTile() || !m_player.getTile()->hasFlag(TILESTATE_PROTECTIONZONE)) return reject("You can only adjust perks in a protection zone.");
	auto dataIt = proficiency.find(weaponId);
	if (dataIt == proficiency.end()) return reject("This weapon has no unlocked proficiency.");
	auto &data = dataIt->second;
	uint32_t revision = 0, level = 0, index = 0;
	if (!readUnsigned(request, "revision", UINT32_MAX, revision) || revision != data.shapingRevision || revision == UINT32_MAX) return reject("Your perks changed. Review the refreshed values and try again.");
	if (!readUnsigned(request, "level", 6, level) || !readUnsigned(request, "index", 2, index) || level >= getUnlockedLevelCount(weaponId)) return reject("Select an unlocked perk.");
	const auto &tree = proficiencies.at(Item::items[weaponId].proficiencyId);
	if (level >= tree.level.size() || index >= tree.level[level].perks.size()) return reject("Invalid perk.");
	auto found = std::ranges::find_if(data.shapes, [&](const auto &s) { return s.level == level; });
	const auto selected = getSelectedPerks(weaponId);
	const bool isSelected = std::ranges::any_of(selected, [&](const auto &p) { return p.level == level && p.index == index; });
	uint32_t cost = 0;
	bool useOrb = false;
	auto next = data.shapes;
	auto target = std::ranges::find_if(next, [&](const auto &s) { return s.level == level && s.index == index; });
	const auto vocation = shapingVocation(weaponId);
	auto pool = [&]() {
		std::vector<uint16_t> ids;
		for (const auto &[id, definition] : shapeDefinitions) {
			if ((!definition.vocation || definition.vocation == vocation) && (target == next.end() || id != target->id)) ids.push_back(id);
		}
		return ids;
	};
	if (action == "modify") {
		if (!isSelected) return reject("Apply this perk selection before modifying it.");
		if (getUnlockedLevelCount(weaponId) < 3) return reject("Proficiency level 3 is required.");
		if (found != data.shapes.end()) return reject("This level already has a shaped perk.");
		if (next.size() >= MAX_SHAPES) return reject("Only two perks can be modified per weapon.");
		if (!next.empty() && data.experience < getMaxExperience(weaponId)) return reject("Weapon mastery is required for a second modified perk.");
		cost = next.empty() ? 250 : 1000;
		auto ids = pool();
		if (ids.empty()) return reject("No shaping options available.");
		next.push_back({ static_cast<uint8_t>(level), static_cast<uint8_t>(index), 0, ids[uniform_random(0, static_cast<int32_t>(ids.size() - 1))], {} });
	} else {
		if (target == next.end()) return reject("This perk has not been modified.");
		if (!target->offers.empty() && action != "keep" && action != "replace" && action != "clear") return reject("Keep or replace the current reshape offer first.");
		if (action == "refine" || action == "maximise") {
			if (target->rank >= MAX_RANK) return reject("Maximum rank has been reached.");
			if (action == "refine") { cost = REFINE_COSTS[target->rank]; ++target->rank; }
			else { useOrb = true; target->rank = MAX_RANK; }
		} else if (action == "reshape") {
			cost = RESHAPE_COST;
			auto ids = pool();
			if (ids.size() < 3) return reject("Not enough distinct shaping options.");
			for (size_t i = 0; i < 3; ++i) {
				const auto pos = uniform_random(0, static_cast<int32_t>(ids.size() - 1));
				target->offers.push_back(ids[pos]); ids.erase(ids.begin() + pos);
			}
		} else if (action == "replace") {
			uint32_t choice = 0;
			if (!readUnsigned(request, "choice", 2, choice) || choice >= target->offers.size()) return reject("That reshape offer is no longer available.");
			target->id = target->offers[choice]; target->offers.clear();
		} else if (action == "keep") {
			if (target->offers.empty()) return reject("No reshape offer is pending.");
			target->offers.clear();
		} else if (action == "clear") {
			next.erase(target);
		} else return reject("Unknown shaping action.");
	}
	if (m_player.getForgeDusts() < cost) return reject("Not enough dust available.");
	if (useOrb && !m_player.removeItemOfType(ASCENSION_ORB, 1, -1)) return reject("A Lunar Ascension Orb is required.");
	m_player.setForgeDusts(m_player.getForgeDusts() - cost);
	data.shapes = std::move(next);
	++data.shapingRevision;
	if (weaponId == m_player.getWeaponId(true)) {
		clearAllStats(); applyPerks(weaponId);
	}
	save(weaponId);
	return shapingState(weaponId);
}

double WeaponProficiency::highestCombatSkill() const {
	double result = m_player.getMagicLevel();
	for (const auto skill : { SKILL_FIST, SKILL_CLUB, SKILL_SWORD, SKILL_AXE, SKILL_DISTANCE }) result = std::max(result, static_cast<double>(m_player.getSkillLevel(skill)));
	return result;
}
