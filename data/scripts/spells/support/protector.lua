local spell = Spell("instant")

function spell.onCastSpell(creature, variant)
	local player = creature and creature:getPlayer()
	if not player then
		return false
	end
	local selection = player:getKnightStance() == "protector" and "none" or "protector"
	if not player:setKnightStance(selection) then
		return false
	end
	player:kv():set("knight-stance", selection)
	player:getPosition():sendMagicEffect(CONST_ME_BLOW_GREEN)
	player:sendTextMessage(MESSAGE_STATUS, selection == "none" and "Knight stance deactivated." or "Protector is active.")
	return true
end

spell:name("Protector")
spell:words("utamo tempo")
spell:group("support", "stance")
spell:vocation("elite knight;true")
spell:castSound(SOUND_EFFECT_TYPE_SPELL_PROTECTOR)
spell:id(132)
spell:cooldown(2000)
spell:groupCooldown(2000, 2000)
spell:level(20)
spell:mana(20)
spell:isSelfTarget(true)
spell:isAggressive(false)
spell:isPremium(true)
spell:register()
