-- Client identity: data/items/client-15.33-manifest.json.
-- HP/XP source: Server tuning (unpublished official health/combat); see docs/systems/client-15-33-update.md
-- Attacks, defense, resistance magnitudes and loot chances are server tuning.
-- No map placements or coordinate-dependent encounter mechanics.
local mType = Game.createMonsterType("Mimar Haffar")
local monster = {}

monster.description = "Mimar Haffar"
monster.experience = 3500000
monster.health = 450000
monster.maxHealth = 450000
monster.speed = 220
monster.corpse = 54346
monster.race = "blood"
monster.manaCost = 0
monster.outfit = {
	lookType = 1953,
	lookAddons = 0,
	lookHead = 0,
	lookBody = 0,
	lookLegs = 0,
	lookFeet = 0,
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
	rewardBoss = true,
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
	{ name = "melee", interval = 2000, chance = 100, minDamage = 0, maxDamage = -2400 },
	{ name = "combat", interval = 2000, chance = 20, type = COMBAT_ENERGYDAMAGE, minDamage = -800, maxDamage = -2400, range = 7, target = true, effect = CONST_ME_ENERGYAREA },
	{ name = "combat", interval = 4000, chance = 15, type = COMBAT_HOLYDAMAGE, minDamage = -800, maxDamage = -2400, radius = 3, target = false, effect = CONST_ME_HOLYAREA },
}

monster.defenses = { defense = 60, armor = 120 }
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
