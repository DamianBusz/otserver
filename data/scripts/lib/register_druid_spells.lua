-- Power bounds and scaling references: docs/systems/druid-rebalance.md.
DruidSpellFormula = {}

function DruidSpellFormula.levelBonus(level)
	-- Each successive 100 bonus points takes another level per point:
	-- levels 1..500 use /5, the next 600 use /6, the next 700 use /7.
	local divisor, bonus = 5, 0
	while level >= divisor * 100 do
		level = level - divisor * 100
		bonus = bonus + 100
		divisor = divisor + 1
	end
	return bonus + math.floor(level / divisor)
end

function DruidSpellFormula.bounds(level, skill, factor, minimumPower, maximumPower)
	local bonus = DruidSpellFormula.levelBonus(level)
	local scaling = skill / factor + 0.25
	return bonus + math.floor(minimumPower * scaling), bonus + math.floor(maximumPower * scaling)
end
