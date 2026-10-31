-- Client identity: data/items/client-15.33-manifest.json.
-- HP/XP source: https://www.tibia.com/news/?subtopic=newsarchive (2026-10-20 XP update); https://tibianebula.com/en/creatures/true_feverbloom_asura/
-- Attacks, defense, resistance magnitudes and loot chances are server tuning.
-- No map placements or coordinate-dependent encounter mechanics.
local mType = Game.createMonsterType("True Feverbloom Asura")
local monster = {}

monster.description = "a true feverbloom asura"
monster.experience = 8600
monster.health = 8600
monster.maxHealth = 8600
monster.speed = 175
monster.corpse = 54674
monster.race = "blood"
monster.manaCost = 0
monster.outfit = {
	lookType = 1068,
	lookAddons = 1,
	lookHead = 62,
	lookBody = 61,
	lookLegs = 114,
	lookFeet = 15,
	lookMount = 0,
}

monster.raceId = 2805
monster.Bestiary = {
	class = "Demon",
	race = BESTY_RACE_DEMON,
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
	{ name = "melee", interval = 2000, chance = 100, minDamage = 0, maxDamage = -900 },
	{ name = "combat", interval = 2000, chance = 20, type = COMBAT_EARTHDAMAGE, minDamage = -300, maxDamage = -900, range = 7, target = true, effect = CONST_ME_POISONAREA },
	{ name = "combat", interval = 4000, chance = 15, type = COMBAT_DEATHDAMAGE, minDamage = -300, maxDamage = -900, radius = 3, target = false, effect = CONST_ME_MORTAREA },
}

monster.defenses = { defense = 45, armor = 90 }
monster.elements = {
	{ type = COMBAT_PHYSICALDAMAGE, percent = 0 },
	{ type = COMBAT_FIREDAMAGE, percent = 100 },
	{ type = COMBAT_EARTHDAMAGE, percent = 10 },
	{ type = COMBAT_DEATHDAMAGE, percent = -10 },
	{ type = COMBAT_ENERGYDAMAGE, percent = -10 },
	{ type = COMBAT_HOLYDAMAGE, percent = -10 },
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
