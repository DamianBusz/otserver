-- The engine snapshots this combat for the fixed-location, half-strength echo.
if not Player.castAimedSpell then
	logger.warn("Death Echo requires a server build with aimed spells and delayed echo combat")
	return
end

local combat = Combat()
combat:setParameter(COMBAT_PARAM_TYPE, COMBAT_DEATHDAMAGE)
combat:setParameter(COMBAT_PARAM_EFFECT, CONST_ME_DEATH_ECHO)
combat:setArea(createCombatArea({
	{ 1, 1, 1, 1, 1 },
	{ 1, 1, 1, 1, 1 },
	{ 1, 1, 3, 1, 1 },
	{ 1, 1, 1, 1, 1 },
	{ 1, 1, 1, 1, 1 },
}))

function onGetDeathEchoValues(player, level, maglevel)
	-- Local 75-base-power scaling, with the usual level contribution.
	local power = 75 * (maglevel * 0.04 + 0.8)
	return -(level / 5 + power * 0.8), -(level / 5 + power * 1.2)
end
combat:setCallback(CALLBACK_PARAM_LEVELMAGICVALUE, "onGetDeathEchoValues")

local spell = Spell("instant")
function spell.onCastSpell(creature, variant)
	return combat:execute(creature, variant)
end
spell:name("Death Echo")
spell:words("exevo mort ora")
spell:id(310)
spell:group("attack")
spell:level(120)
spell:mana(150)
spell:range(7)
spell:blockWalls(true)
spell:isPremium(true)
spell:cooldown(6000)
spell:groupCooldown(2000)
spell:vocation("sorcerer;true", "master sorcerer;true")
spell:register()
