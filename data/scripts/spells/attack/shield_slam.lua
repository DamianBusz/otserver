local combat = Combat()
combat:setParameter(COMBAT_PARAM_TYPE, COMBAT_PHYSICALDAMAGE)
combat:setParameter(COMBAT_PARAM_EFFECT, CONST_ME_SHIELD_BASH)
combat:setParameter(COMBAT_PARAM_BLOCKARMOR, 1)
combat:setParameter(COMBAT_PARAM_NEXTATTACKREDUCTION, 50)
combat:setArea(createCombatArea({ { 1, 1, 1 }, { 1, 3, 1 }, { 1, 1, 1 } }))

function onGetFormulaValues(player, level, magicLevel)
	return KnightSpellFormula.shieldBounds(player, level, 52)
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
spell:id(316)
spell:name("Shield Slam")
spell:words("exori scu")
spell:level(30)
spell:mana(110)
spell:isPremium(true)
spell:cooldown(6 * 1000)
spell:groupCooldown(2 * 1000)
spell:vocation("knight;true", "elite knight;true")
spell:register()
