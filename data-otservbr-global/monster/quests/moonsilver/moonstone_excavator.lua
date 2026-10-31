-- Client identity: data/items/client-15.33-manifest.json.
-- HP/XP source: https://www.tibia.com/library/?subtopic=creatures&race=moonstoneexcavator
-- Attacks, defense, resistance magnitudes and loot chances are server tuning.
-- No map placements or coordinate-dependent encounter mechanics.
local mType = Game.createMonsterType("Moonstone Excavator")
local monster = {}

monster.description = "a moonstone excavator"
monster.experience = 4670
monster.health = 5100
monster.maxHealth = 5100
monster.speed = 160
monster.corpse = 54342
monster.race = "blood"
monster.manaCost = 0
monster.outfit = {
	lookType = 1952,
	lookAddons = 1,
	lookHead = 0,
	lookBody = 0,
	lookLegs = 0,
	lookFeet = 0,
	lookMount = 0,
}

monster.raceId = 2857
monster.Bestiary = {
	class = "Human",
	race = BESTY_RACE_HUMAN,
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
	{ name = "melee", interval = 2000, chance = 100, minDamage = 0, maxDamage = -650 },
	{ name = "combat", interval = 2000, chance = 20, type = COMBAT_PHYSICALDAMAGE, minDamage = -216, maxDamage = -650, range = 7, target = true, effect = CONST_ME_HITAREA },
	{ name = "combat", interval = 4000, chance = 15, type = COMBAT_ENERGYDAMAGE, minDamage = -216, maxDamage = -650, radius = 3, target = false, effect = CONST_ME_ENERGYAREA },
}

monster.defenses = { defense = 32, armor = 65 }
monster.elements = {
	{ type = COMBAT_PHYSICALDAMAGE, percent = -10 },
	{ type = COMBAT_FIREDAMAGE, percent = -10 },
	{ type = COMBAT_EARTHDAMAGE, percent = 10 },
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
	{ id = 3035, chance = 100000, maxCount = 15 },
	{ id = 3043, chance = 3000, maxCount = 1 },
}

mType:register(monster)
