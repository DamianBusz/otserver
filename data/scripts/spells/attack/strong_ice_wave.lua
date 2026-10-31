local combat = Combat()
combat:setParameter(COMBAT_PARAM_TYPE, COMBAT_ICEDAMAGE)
combat:setParameter(COMBAT_PARAM_EFFECT, CONST_ME_ICEAREA)
-- Ten-tile footprint from the installed official spell preview.
combat:setArea(createCombatArea({
	{ 1, 1, 1 },
	{ 1, 1, 1 },
	{ 1, 1, 1 },
	-- Marker 3 is affected and anchors to the directional cast tile one ahead.
	{ 0, 3, 0 },
}))

local function iceWaveFormula(player, level, magicLevel)
	local min, max = DruidSpellFormula.bounds(level, magicLevel, 25, 100, 180)
	return -min, -max
end

onGetFormulaValues = iceWaveFormula
combat:setCallback(CALLBACK_PARAM_LEVELMAGICVALUE, "onGetFormulaValues")

local augmentedCombat = Combat()
augmentedCombat:setParameter(COMBAT_PARAM_TYPE, COMBAT_ICEDAMAGE)
augmentedCombat:setParameter(COMBAT_PARAM_EFFECT, CONST_ME_ICEAREA)
-- Local enlarged footprint pending official-game geometry calibration.
augmentedCombat:setArea(createCombatArea(AREA_WAVE5))
-- Callback registration consumes the named global; expose it again for this combat.
onGetFormulaValues = iceWaveFormula
augmentedCombat:setCallback(CALLBACK_PARAM_LEVELMAGICVALUE, "onGetFormulaValues")

local spell = Spell("instant")

function spell.onCastSpell(creature, var)
	local player = creature:getPlayer()
	if player and player:getWheelSpellAdditionalArea("Strong Ice Wave") then
		return augmentedCombat:execute(creature, var)
	end
	return combat:execute(creature, var)
end

spell:group("attack")
spell:id(43)
spell:name("Strong Ice Wave")
spell:words("exevo gran frigo hur")
spell:castSound(SOUND_EFFECT_TYPE_SPELL_STRONG_ICE_WAVE)
spell:level(40)
spell:mana(170)
spell:needDirection(true)
spell:cooldown(4 * 1000)
spell:groupCooldown(2 * 1000)

spell:vocation("druid;true", "elder druid;true")
spell:register()
