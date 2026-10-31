local login = CreatureEvent("RestoreElementalStance")

function login.onLogin(player)
	if not Player.setElementalStance then
		return true
	end
	local stance = player:kv():get("sorcerer-elemental-stance")
	if type(stance) == "string" and player:setElementalStance(stance) then
		player:sendTextMessage(MESSAGE_STATUS, "Elemental stance: " .. stance .. ".")
	end
	return true
end

login:register()
