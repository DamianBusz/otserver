local function sweepFormula(player, skill, attack, factor)
	local level = KnightSpellFormula.levelBonus(player:getLevel())
	local scaling = skill * attack / 1000 + 0.25
	return -(level + math.floor(60 * scaling + 0.5)), -(level + math.floor(100 * scaling + 0.5))
end

local function makeSweep(area)
	local combat = Combat()
	combat:setParameter(COMBAT_PARAM_TYPE, COMBAT_PHYSICALDAMAGE)
	combat:setParameter(COMBAT_PARAM_EFFECT, CONST_ME_HITAREA)
	combat:setParameter(COMBAT_PARAM_BLOCKARMOR, 1)
	combat:setParameter(COMBAT_PARAM_USECHARGES, 1)
	combat:setArea(createCombatArea(area))
	-- Named callback registration consumes the global on each call.
	onGetFormulaValues = sweepFormula
	combat:setCallback(CALLBACK_PARAM_SKILLVALUE, "onGetFormulaValues")
	return combat
end

-- Directional variants already point one tile ahead. Marker 3 is that affected
-- front tile, not the caster. Augment II adds the two squares beside the caster.
local combat = makeSweep({ { 1, 3, 1 } })
local augmentedCombat = makeSweep({ { 1, 3, 1 }, { 1, 0, 1 } })
local spell = Spell("instant")
function spell.onCastSpell(creature, var)
	local player = creature:getPlayer()
	if player and player:getWheelSpellAdditionalArea("Front Sweep") then
		return augmentedCombat:execute(creature, var)
	end
	return combat:execute(creature, var)
end

spell:group("attack")
spell:id(59)
spell:name("Front Sweep")
spell:words("exori min")
spell:castSound(SOUND_EFFECT_TYPE_SPELL_FRONT_SWEEP)
spell:level(70)
spell:mana(200)
spell:isPremium(true)
spell:needDirection(true)
spell:needWeapon(true)
spell:cooldown(6 * 1000)
spell:groupCooldown(2 * 1000)

spell:vocation("knight;true", "elite knight;true")
spell:register()
