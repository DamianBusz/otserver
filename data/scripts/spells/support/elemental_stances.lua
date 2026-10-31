-- Requires the elemental-stance combat engine; older published images must
-- not register spells that only display an effect without changing combat.
if not Player.setElementalStance then
	logger.warn("Elemental stances require a server build with Player.setElementalStance")
	return
end

local stances = {
	{ key = "flames", name = "Master of Flames", words = "uteta flam", id = 304, effect = CONST_ME_EXPLOSIONAREA },
	{ key = "thunder", name = "Master of Thunder", words = "uteta vis", id = 305, effect = CONST_ME_BLOW_BLUE },
	{ key = "decay", name = "Master of Decay", words = "uteta mort", id = 306, effect = CONST_ME_BLOW_WHITE },
}

for _, stance in ipairs(stances) do
	local spell = Spell("instant")
	function spell.onCastSpell(creature, variant)
		local player = creature and creature:getPlayer()
		if not player or not player:setElementalStance(stance.key) then
			return false
		end
		player:kv():set("sorcerer-elemental-stance", stance.key)
		player:getPosition():sendMagicEffect(stance.effect)
		player:sendTextMessage(MESSAGE_STATUS, stance.name .. " is active.")
		return true
	end
	spell:name(stance.name)
	spell:words(stance.words)
	spell:id(stance.id)
	spell:group("support", "stance")
	spell:level(20)
	spell:mana(400)
	spell:isPremium(true)
	spell:isSelfTarget(true)
	spell:isAggressive(false)
	spell:cooldown(30 * 1000)
	spell:groupCooldown(2 * 1000, 30 * 1000)
	spell:vocation("sorcerer;true", "master sorcerer;true")
	spell:register()
end
