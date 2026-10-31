-- Client identity: data/items/client-15.33-manifest.json.
-- HP/XP source: https://www.tibia.com/library/?subtopic=creatures&race=devotedradiantacolyte
-- Attacks, defense, resistance magnitudes and loot chances are server tuning.
-- No map placements or coordinate-dependent encounter mechanics.
local mType = Game.createMonsterType("Devoted Radiant Acolyte")
local monster = {}

monster.description = "a devoted radiant acolyte"
monster.experience = 47820
monster.health = 39790
monster.maxHealth = 39790
monster.speed = 150
monster.corpse = 54419
monster.race = "blood"
monster.manaCost = 0
monster.outfit = {
	lookType = 1969,
	lookAddons = 3,
	lookHead = 0,
	lookBody = 0,
	lookLegs = 0,
	lookFeet = 0,
	lookMount = 0,
}

monster.raceId = 2844
monster.Bestiary = {
	class = "Human",
	race = BESTY_RACE_HUMAN,
	toKill = 5000,
	FirstUnlock = 200,
	SecondUnlock = 2000,
	CharmsPoints = 100,
	Stars = 5,
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
	{ name = "melee", interval = 2000, chance = 100, minDamage = 0, maxDamage = -1840 },
	{ name = "combat", interval = 2000, chance = 20, type = COMBAT_FIREDAMAGE, minDamage = -613, maxDamage = -1840, range = 7, target = true, effect = CONST_ME_FIREAREA },
	{ name = "combat", interval = 4000, chance = 15, type = COMBAT_DEATHDAMAGE, minDamage = -613, maxDamage = -1840, radius = 3, target = false, effect = CONST_ME_MORTAREA },
}

monster.defenses = { defense = 52, armor = 105 }
monster.elements = {
	{ type = COMBAT_PHYSICALDAMAGE, percent = 10 },
	{ type = COMBAT_FIREDAMAGE, percent = 10 },
	{ type = COMBAT_EARTHDAMAGE, percent = -10 },
	{ type = COMBAT_DEATHDAMAGE, percent = 10 },
	{ type = COMBAT_ENERGYDAMAGE, percent = -10 },
	{ type = COMBAT_HOLYDAMAGE, percent = 10 },
	{ type = COMBAT_ICEDAMAGE, percent = 10 },
}
monster.immunities = {
	{ type = "paralyze", condition = true },
	{ type = "invisible", condition = true },
}

monster.loot = {
	{ id = 3035, chance = 100000, maxCount = 35 },
	{ id = 3043, chance = 3000, maxCount = 1 },
	{ id = 54640, chance = 10000, maxCount = 2 },
}

mType:register(monster)
