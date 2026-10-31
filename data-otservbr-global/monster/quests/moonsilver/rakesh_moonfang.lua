-- Client identity: data/items/client-15.33-manifest.json.
-- HP/XP source: Server tuning (unpublished official health/combat); see docs/systems/client-15-33-update.md
-- Attacks, defense, resistance magnitudes and loot chances are server tuning.
-- No map placements or coordinate-dependent encounter mechanics.
local mType = Game.createMonsterType("Rakesh Moonfang")
local monster = {}

monster.description = "Rakesh Moonfang"
monster.experience = 25500
monster.health = 25500
monster.maxHealth = 25500
monster.speed = 220
monster.corpse = 54386
monster.race = "blood"
monster.manaCost = 0
monster.outfit = {
	lookType = 1648,
	lookAddons = 0,
	lookHead = 0,
	lookBody = 10,
	lookLegs = 52,
	lookFeet = 85,
	lookMount = 0,
}

monster.changeTarget = { interval = 4000, chance = 10 }
monster.strategiesTarget = { nearest = 80, random = 20 }
monster.flags = {
	summonable = false,
	attackable = true,
	hostile = true,
	convinceable = false,
	pushable = false,
	rewardBoss = false,
	illusionable = false,
	canPushItems = true,
	canPushCreatures = true,
	staticAttackChance = 90,
	targetDistance = 1,
	runHealth = 0,
	isBlockable = false,
	canWalkOnEnergy = true,
	canWalkOnFire = true,
	canWalkOnPoison = true,
}

monster.attacks = {
	{ name = "melee", interval = 2000, chance = 100, minDamage = 0, maxDamage = -1100 },
	{ name = "combat", interval = 2000, chance = 20, type = COMBAT_PHYSICALDAMAGE, minDamage = -366, maxDamage = -1100, range = 7, target = true, effect = CONST_ME_HITAREA },
	{ name = "combat", interval = 4000, chance = 15, type = COMBAT_DEATHDAMAGE, minDamage = -366, maxDamage = -1100, radius = 3, target = false, effect = CONST_ME_MORTAREA },
}

monster.defenses = { defense = 40, armor = 80 }
monster.elements = {
	{ type = COMBAT_PHYSICALDAMAGE, percent = 0 },
	{ type = COMBAT_FIREDAMAGE, percent = 0 },
	{ type = COMBAT_EARTHDAMAGE, percent = 0 },
	{ type = COMBAT_DEATHDAMAGE, percent = 0 },
	{ type = COMBAT_ENERGYDAMAGE, percent = 0 },
	{ type = COMBAT_HOLYDAMAGE, percent = 0 },
	{ type = COMBAT_ICEDAMAGE, percent = 0 },
}
monster.immunities = {
	{ type = "paralyze", condition = true },
	{ type = "invisible", condition = true },
}

monster.loot = {
	{ id = 3035, chance = 100000, maxCount = 100 },
	{ id = 3043, chance = 50000, maxCount = 20 },
}

mType:register(monster)
