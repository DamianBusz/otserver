local combat = Combat()
combat:setParameter(COMBAT_PARAM_TYPE, COMBAT_PHYSICALDAMAGE)
combat:setParameter(COMBAT_PARAM_EFFECT, CONST_ME_SHIELD_BASH)
combat:setParameter(COMBAT_PARAM_BLOCKARMOR, 1)
combat:setParameter(COMBAT_PARAM_NEXTATTACKREDUCTION, 50)

function onGetFormulaValues(player, level, magicLevel)
	return KnightSpellFormula.shieldBounds(player, level, 55)
end
combat:setCallback(CALLBACK_PARAM_LEVELMAGICVALUE, "onGetFormulaValues")

local spell = Spell("instant")
function spell.onCastSpell(creature, var)
	if not KnightSpellFormula.canUseShield(creature) then
		return false
	end
	return combat:execute(creature, var)
end

spell:group("attack")
spell:id(315)
spell:name("Shield Bash")
spell:words("exori ico scu")
spell:level(18)
spell:mana(30)
spell:isPremium(true)
spell:range(1)
spell:needTarget(true)
spell:blockWalls(true)
spell:cooldown(4 * 1000)
spell:groupCooldown(2 * 1000)
spell:vocation("knight;true", "elite knight;true")
spell:register()
