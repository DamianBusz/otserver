local combat = Combat()
combat:setParameter(COMBAT_PARAM_TYPE, COMBAT_HOLYDAMAGE)
combat:setParameter(COMBAT_PARAM_EFFECT, CONST_ME_DIVINE_BARRAGE)
combat:setArea(createCombatArea(AREA_CIRCLE2X2))

function onGetDivineBarrageValues(player, level, maglevel)
	local min, max = PaladinSpellFormula.bounds(level, maglevel, 25, 120, 140)
	return -min, -max
end
combat:setCallback(CALLBACK_PARAM_LEVELMAGICVALUE, "onGetDivineBarrageValues")

local spell = Spell("instant")
function spell.onCastSpell(creature, variant)
	return combat:execute(creature, variant)
end
spell:name("Divine Barrage")
spell:words("exori dir san")
spell:id(302)
spell:group("attack")
spell:level(70)
spell:mana(175)
spell:range(7)
spell:blockWalls(true)
spell:isPremium(true)
spell:cooldown(4000)
spell:groupCooldown(2000)
spell:vocation("paladin;true", "royal paladin;true")
spell:register()
