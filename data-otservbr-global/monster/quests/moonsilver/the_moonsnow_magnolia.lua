-- Client identity: data/items/client-15.33-manifest.json.
-- HP/XP source: https://www.tibiawiki.com.br/index.php?stable=1&title=The_Moonsnow_Magnolia
-- Attacks, defense, resistance magnitudes and loot chances are server tuning.
-- No map placements or coordinate-dependent encounter mechanics.
local mType = Game.createMonsterType("The Moonsnow Magnolia")
local monster = {}

monster.description = "The Moonsnow Magnolia"
monster.experience = 240000
monster.health = 52000
monster.maxHealth = 52000
monster.speed = 200
monster.corpse = 54722
monster.race = "blood"
monster.manaCost = 0
monster.outfit = {
	lookType = 1068,
	lookAddons = 3,
	lookHead = 85,
	lookBody = 13,
	lookLegs = 109,
	lookFeet = 3,
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
	{ name = "melee", interval = 2000, chance = 100, minDamage = 0, maxDamage = -1400 },
	{ name = "combat", interval = 2000, chance = 20, type = COMBAT_ICEDAMAGE, minDamage = -466, maxDamage = -1400, range = 7, target = true, effect = CONST_ME_ICEAREA },
	{ name = "combat", interval = 4000, chance = 15, type = COMBAT_DEATHDAMAGE, minDamage = -466, maxDamage = -1400, radius = 3, target = false, effect = CONST_ME_MORTAREA },
}

monster.defenses = { defense = 47, armor = 95 }
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
