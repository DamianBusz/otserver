local login = CreatureEvent("RestorePaladinStance")

function login.onLogin(player)
	if not Player.setPaladinStance then
		return true
	end
	local stance = player:kv():get("paladin-stance")
	if type(stance) == "string" and player:setPaladinStance(stance) and stance ~= "none" then
		player:sendTextMessage(MESSAGE_STATUS, "Paladin stance: " .. stance .. ".")
	end
	return true
end

login:register()
