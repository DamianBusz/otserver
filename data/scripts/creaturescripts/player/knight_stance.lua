local login = CreatureEvent("RestoreKnightStance")

function login.onLogin(player)
	if not Player.setKnightStance then
		return true
	end
	-- Strip a saved legacy timed buff even when no new stance was saved yet.
	player:setKnightStance("none")
	local stance = player:kv():get("knight-stance")
	if type(stance) == "string" then
		if not player:setKnightStance(stance) then
			player:setKnightStance("none")
			player:kv():set("knight-stance", "none")
		elseif stance ~= "none" then
			player:sendTextMessage(MESSAGE_STATUS, "Knight stance: " .. stance .. ".")
		end
	end
	return true
end

login:register()
