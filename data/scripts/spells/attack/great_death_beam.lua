function onGetFormulaValues(player, level, maglevel)
	local min = (level / 5) + (maglevel * 5.5)
	local max = (level / 5) + (maglevel * 9)
	return -min, -max
end

local combat = Combat()
combat:setCallback(CALLBACK_PARAM_LEVELMAGICVALUE, "onGetFormulaValues")
combat:setParameter(COMBAT_PARAM_TYPE, COMBAT_DEATHDAMAGE)
combat:setParameter(COMBAT_PARAM_EFFECT, CONST_ME_MORTAREA)
combat:setArea(createCombatArea(AREA_BEAM8))

local spell = Spell("instant")

function spell.onCastSpell(creature, var)
	if not creature or not creature:isPlayer() then
		return false
	end

	return combat:execute(creature, var)
end

spell:group("attack", "greatbeams")
spell:id(260)
spell:name("Great Death Beam")
spell:words("exevo max mort")
spell:level(66)
spell:mana(140)
spell:isPremium(true)
spell:needDirection(true)
spell:blockWalls(true)
spell:cooldown(6 * 1000)
spell:groupCooldown(2 * 1000, 6 * 1000)
spell:vocation("master sorcerer;true")
spell:register()
