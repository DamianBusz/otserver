-- Client identity: data/items/client-15.33-manifest.json.
-- HP/XP source: https://www.tibia.com/library/?subtopic=creatures&race=wingedjaracal
-- Attacks, defense, resistance magnitudes and loot chances are server tuning.
-- No map placements or coordinate-dependent encounter mechanics.
local mType = Game.createMonsterType("Winged Jaracal")
local monster = {}

monster.description = "a winged jaracal"
monster.experience = 7830
monster.health = 8200
monster.maxHealth = 8200
monster.speed = 200
monster.corpse = 54647
monster.race = "blood"
monster.manaCost = 0
monster.outfit = {
	lookType = 1961,
	lookAddons = 1,
	lookHead = 113,
	lookBody = 40,
	lookLegs = 95,
	lookFeet = 95,
	lookMount = 0,
}

monster.raceId = 2802
monster.Bestiary = {
	class = "Mammal",
	race = BESTY_RACE_MAMMAL,
	toKill = 2500,
	FirstUnlock = 100,
	SecondUnlock = 1000,
	CharmsPoints = 50,
	Stars = 4,
	Occurrence = 0,
	Locations = "No map placement configured.",
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
	{ name = "melee", interval = 2000, chance = 100, minDamage = 0, maxDamage = -800 },
	{ name = "combat", interval = 2000, chance = 20, type = COMBAT_DEATHDAMAGE, minDamage = -266, maxDamage = -800, range = 7, target = true, effect = CONST_ME_MORTAREA },
	{ name = "combat", interval = 4000, chance = 15, type = COMBAT_FIREDAMAGE, minDamage = -266, maxDamage = -800, radius = 3, target = false, effect = CONST_ME_FIREAREA },
}

monster.defenses = { defense = 40, armor = 80 }
monster.elements = {
	{ type = COMBAT_PHYSICALDAMAGE, percent = 10 },
	{ type = COMBAT_FIREDAMAGE, percent = 10 },
	{ type = COMBAT_EARTHDAMAGE, percent = -10 },
	{ type = COMBAT_DEATHDAMAGE, percent = 10 },
	{ type = COMBAT_ENERGYDAMAGE, percent = -10 },
	{ type = COMBAT_HOLYDAMAGE, percent = 10 },
	{ type = COMBAT_ICEDAMAGE, percent = -10 },
}
monster.immunities = {
	{ type = "paralyze", condition = true },
	{ type = "invisible", condition = true },
}

monster.loot = {
	{ id = 3035, chance = 100000, maxCount = 15 },
	{ id = 3043, chance = 3000, maxCount = 1 },
}

mType:register(monster)
