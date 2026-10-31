local combat = Combat()
combat:setParameter(COMBAT_PARAM_TYPE, COMBAT_EARTHDAMAGE)
combat:setParameter(COMBAT_PARAM_EFFECT, CONST_ME_FORKED_THORNS)

function onGetFormulaValues(player, level, magicLevel)
	local min, max = DruidSpellFormula.bounds(level, magicLevel, 25, 95, 99)
	return -min, -max
end
combat:setCallback(CALLBACK_PARAM_LEVELMAGICVALUE, "onGetFormulaValues")

local spell = Spell("instant")
function spell.onCastSpell(creature, variant)
	return combat:execute(creature, variant)
end
spell:name("Forked Thorns")
spell:words("exevo fur tera")
spell:id(318)
spell:group("attack")
spell:level(80)
spell:mana(180)
spell:range(7)
spell:needTarget(true)
spell:blockWalls(true)
spell:isPremium(true)
spell:cooldown(6000)
spell:groupCooldown(2000)
spell:vocation("druid;true", "elder druid;true")
spell:register()
