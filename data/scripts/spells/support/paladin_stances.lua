if not Player.setPaladinStance then
	logger.warn("Paladin stances require a server build with Player.setPaladinStance")
	return
end

local stances = {
	{ key = "sharpshooter", name = "Sharpshooter", words = "utori con", id = 313, effect = CONST_ME_EXPLOSIONAREA },
	{ key = "divine-defiance", name = "Divine Defiance", words = "utori hur", id = 314, effect = CONST_ME_BLOW_GREEN },
}

for _, stance in ipairs(stances) do
	local spell = Spell("instant")
	function spell.onCastSpell(creature, variant)
		local player = creature and creature:getPlayer()
		if not player then
			return false
		end
		local selection = player:getPaladinStance() == stance.key and "none" or stance.key
		if not player:setPaladinStance(selection) then
			return false
		end
		player:kv():set("paladin-stance", selection)
		player:getPosition():sendMagicEffect(stance.effect)
		player:sendTextMessage(MESSAGE_STATUS, selection == "none" and "Paladin stance deactivated." or stance.name .. " is active.")
		return true
	end
	spell:name(stance.name)
	spell:words(stance.words)
	spell:id(stance.id)
	spell:group("support", "stance")
	spell:level(20)
	spell:mana(250)
	spell:isPremium(true)
	spell:isSelfTarget(true)
	spell:isAggressive(false)
	spell:cooldown(10000)
	spell:groupCooldown(2000, 10000)
	spell:vocation("royal paladin;true")
	spell:register()
end
