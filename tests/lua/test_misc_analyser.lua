-- Run from the Canary repository root with LuaJIT. Exercises real loot/skinning scripts.
MonsterType = {}
configKeys = { RATE_LOOT = 1 }
configManager = {
	getNumber = function()
		return 1
	end,
}
ITEM_TYPE_CREATUREPRODUCT = 1
CHARM_GUT, CHARM_SCAVENGE = 14, 13
local rolls, bonus, product, gutCount = 105, 20, true, 0
getLootRandom = function()
	return rolls
end
local originalRandom = math.random
math.random = function()
	return 100
end
ItemType = function()
	return {
		getId = function()
			return 1
		end,
		getType = function()
			return product and 1 or 2
		end,
		getCharges = function()
			return 0
		end,
		isStackable = function()
			return false
		end,
	}
end
dofile("data/libs/functions/monstertype.lua")
local monster = setmetatable({}, { __index = MonsterType })
function monster:getLoot()
	return { { itemId = 1, chance = 100 } }
end
function monster:isRewardBoss()
	return false
end
local player = {
	getCharmChance = function()
		return bonus
	end,
	recordCharmProc = function(_, id)
		assert(id == CHARM_GUT)
		gutCount = gutCount + 1
	end,
}
local result = monster:generateLootRoll({ factor = 1, gut = true }, {}, player)
assert(result[1].count == 1 and result[1].gut and gutCount == 1, "Bonus-only loot counts Gut")
rolls = 99
result = monster:generateLootRoll({ factor = 1, gut = true }, {}, player)
assert(result[1].count == 1 and not result[1].gut and gutCount == 1, "Ordinary loot is not a Gut proc")
rolls = 120
result = monster:generateLootRoll({ factor = 1, gut = true }, {}, player)
assert(not result[1] and gutCount == 1, "Failed roll does not count")
rolls, product = 105, false
assert(not monster:generateLootRoll({ factor = 1, gut = true }, {}, player)[1] and gutCount == 1, "Non-product loot excludes Gut")
product, bonus = true, 0
assert(not monster:generateLootRoll({ factor = 1, gut = true }, {}, player)[1] and gutCount == 1, "Zero charm bonus excludes Gut")
print("PASS: actual Gut loot hook, base/bonus threshold, no-proc and no-bonus cases")

local registered
Action = function()
	return {
		id = function() end,
		register = function(self)
			registered = self
		end,
	}
end
local eligible, skinRoll, scavenges = true, 1, 0
ItemType = function()
	return {
		getDecayId = function()
			return 4012
		end,
	}
end
CONTAINER_POSITION, CONST_ME_MAGIC_GREEN, CONST_ME_BLOCKHIT = 65535, 1, 2
table.contains = function(values, v)
	for _, value in ipairs(values) do
		if value == v then
			return true
		end
	end
	return false
end
isInArray = table.contains
local position = { x = 100, y = 100, sendMagicEffect = function() end }
local target = {
	itemid = 4011,
	getId = function()
		return 4011
	end,
	isItem = function()
		return true
	end,
	transform = function() end,
	getPosition = function()
		return position
	end,
}
function target:getType()
	return {
		getDecayId = function()
			return 4012
		end,
	}
end
Tile = function()
	return {
		getTopDownItem = function()
			return target
		end,
	}
end
local tool = {
	itemid = 5908,
	getId = function()
		return 5908
	end,
	getParent = function()
		return { uid = 1 }
	end,
}
Container = function()
	return {
		getEmptySlots = function()
			return 1
		end,
		addItem = function()
			return {}
		end,
	}
end
player = {
	getCharmMonsterType = function()
		return eligible and {
			getCorpseId = function()
				return 4011
			end,
		} or nil
	end,
	getCharmChance = function()
		return 20
	end,
	recordCharmProc = function(_, id)
		assert(id == CHARM_SCAVENGE)
		scavenges = scavenges + 1
	end,
	addItem = function()
		return {}
	end,
	addAchievementProgress = function() end,
	getPosition = function()
		return position
	end,
	sendCancelMessage = function()
		error("Unexpected cancel")
	end,
}
math.random = function()
	return skinRoll
end
dofile("data-otservbr-global/scripts/actions/tools/skinning.lua")
assert(registered)
assert(registered.onUse(player, tool, position, target, position) and scavenges == 1, "Successful eligible skin counts")
skinRoll = 26000
assert(registered.onUse(player, tool, position, target, position) and scavenges == 1, "Failed skin does not count")
eligible, skinRoll = false, 1
assert(registered.onUse(player, tool, position, target, position) and scavenges == 1, "Skin without assigned charm does not count")
math.random = originalRandom
print("PASS: actual Scavenge skinning hook, successful/failed/unassigned cases")
