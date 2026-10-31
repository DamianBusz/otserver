-- Numerical reference and limits: docs/systems/knight-rebalance.md.
KnightSpellFormula = {}

-- Local calibration model: the official client publishes powers and scaling
-- inputs, but not coefficients or random bounds. Keep these tunable together;
-- see docs/systems/knight-rebalance.md before changing them.
local healingMagicPercent = 20
local healingShieldPercent = 4
local healingBasePercent = 25
local healingVariationPercent = 20

function KnightSpellFormula.levelBonus(level)
	local divisor, bonus = 5, 0
	while level >= divisor * 100 do
		level = level - divisor * 100
		bonus = bonus + 100
		divisor = divisor + 1
	end
	return bonus + math.floor(level / divisor)
end

function KnightSpellFormula.healingBounds(player, level, magicLevel, power)
	local shielding = player:getEffectiveSkillLevel(SKILL_SHIELD)
	local scaling = healingBasePercent + magicLevel * healingMagicPercent + shielding * healingShieldPercent
	local flat = KnightSpellFormula.levelBonus(level)
	local mean = power * scaling
	-- Shield equipment is not required. Battle Healing, equipment and proficiency
	-- modifiers belong to the engine's healing path and must not be applied here.
	-- Divide after multiplying integer percentages so half-point rounding is stable.
	return flat + math.floor(mean * (100 - healingVariationPercent) / 10000 + 0.5), flat + math.floor(mean * (100 + healingVariationPercent) / 10000 + 0.5)
end

function KnightSpellFormula.shieldBounds(player, level, power)
	local defense = player:getShieldSpellDefense()
	if not defense then
		return 0, 0
	end
	local scaling = player:getEffectiveSkillLevel(SKILL_SHIELD) * defense / 1000 + 0.25
	local flat = KnightSpellFormula.levelBonus(level)
	return -(flat + math.floor((power - 2) * scaling)), -(flat + math.floor((power + 2) * scaling))
end

function KnightSpellFormula.canUseShield(creature)
	local player = creature:getPlayer()
	if not player or player:getShieldSpellDefense() == nil then
		if player then
			player:sendCancelMessage("You need to equip a shield to cast this spell.")
			player:getPosition():sendMagicEffect(CONST_ME_POFF)
		end
		return false
	end
	return true
end
