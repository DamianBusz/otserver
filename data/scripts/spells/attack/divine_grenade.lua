-- The engine owns the delayed blast and validates caster/session/spell lifetime.
local combat = Combat()
combat:setParameter(COMBAT_PARAM_TYPE, COMBAT_HOLYDAMAGE)
combat:setParameter(COMBAT_PARAM_EFFECT, CONST_ME_HOLYDAMAGE)
combat:setArea(createCombatArea(AREA_CIRCLE2X2))

function onGetDivineGrenadeValues(player, level, maglevel)
	local stage = player:revelationStageWOD("Divine Grenade")
	local power = ({ 190, 220, 250 })[stage]
	if not power then return 0, 0 end
	local min, max = PaladinSpellFormula.bounds(level, maglevel, 25, power - 50, power + 50)
	return -min, -max
end
combat:setCallback(CALLBACK_PARAM_LEVELMAGICVALUE, "onGetDivineGrenadeValues")

local spell = Spell("instant")
function spell.onCastSpell(creature, variant)
	local player = creature and creature:getPlayer()
	if not player or player:revelationStageWOD("Divine Grenade") == 0 then
		return false
	end
	return combat:execute(player, variant)
end
spell:group("attack")
spell:id(258)
spell:name("Divine Grenade")
spell:words("exevo tempo mas san")
spell:level(300)
spell:mana(160)
spell:isPremium(true)
spell:range(7)
spell:blockWalls(true)
spell:cooldown(26000)
spell:groupCooldown(2000)
spell:needLearn(true)
spell:vocation("paladin;true", "royal paladin;true")
spell:register()
