local combat = Combat()
combat:setParameter(COMBAT_PARAM_TYPE, COMBAT_PHYSICALDAMAGE)
combat:setParameter(COMBAT_PARAM_EFFECT, CONST_ME_ETHEREAL_BARRAGE)
combat:setParameter(COMBAT_PARAM_BLOCKARMOR, true)
combat:setArea(createCombatArea(AREA_CIRCLE2X2))

function onGetEtherealBarrageValues(player, level, maglevel)
	-- This spell scales only with total distance skill. A level/magic callback
	-- deliberately avoids the weapon callback's elemental split and attack stat.
	local skill = player:getEffectiveSkillLevel(SKILL_DISTANCE)
	local min, max = PaladinSpellFormula.bounds(level, skill, 17.8571, 37, 43)
	return -min, -max
end
combat:setCallback(CALLBACK_PARAM_LEVELMAGICVALUE, "onGetEtherealBarrageValues")

local spell = Spell("instant")
function spell.onCastSpell(creature, variant)
	return combat:execute(creature, variant)
end
spell:name("Ethereal Barrage")
spell:words("exori dir moe")
spell:id(303)
spell:group("attack")
spell:level(60)
spell:mana(135)
spell:range(7)
spell:blockWalls(true)
spell:isPremium(true)
spell:cooldown(4000)
spell:groupCooldown(2000)
spell:vocation("paladin;true", "royal paladin;true")
spell:register()
