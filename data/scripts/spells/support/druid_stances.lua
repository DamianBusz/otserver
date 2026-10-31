if not Player.setDruidStance then
	logger.warn("Druid stances require a server build with Player.setDruidStance")
	return
end

local stances = {
	{ key = "shared-conservation", name = "Shared Conservation", words = "utura sio", id = 309, effect = CONST_ME_BLOW_BLUE },
	{ key = "elemental-synthesis", name = "Elemental Synthesis", words = "utito dru", id = 319, effect = CONST_ME_BLOW_PINK },
}

for _, stance in ipairs(stances) do
	local spell = Spell("instant")
	function spell.onCastSpell(creature, variant)
		local player = creature and creature:getPlayer()
		if not player then
			return false
		end
		local selection = player:getDruidStance() == stance.key and "none" or stance.key
		if not player:setDruidStance(selection) then
			return false
		end
		player:kv():set("druid-stance", selection)
		player:getPosition():sendMagicEffect(stance.effect)
		player:sendTextMessage(MESSAGE_STATUS, selection == "none" and "Druid stance deactivated." or stance.name .. " is active.")
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
	spell:cooldown(10000)
	spell:groupCooldown(2000, 10000)
	spell:vocation("elder druid;true")
	spell:register()
end
