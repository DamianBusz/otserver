local login = CreatureEvent("RestoreDruidStance")

function login.onLogin(player)
	if not Player.setDruidStance then
		return true
	end
	local stance = player:kv():get("druid-stance")
	if type(stance) == "string" then
		if not player:setDruidStance(stance) then
			player:setDruidStance("none")
			player:kv():set("druid-stance", "none")
		elseif stance ~= "none" then
			player:sendTextMessage(MESSAGE_STATUS, "Druid stance: " .. stance .. ".")
		end
	end
	return true
end

login:register()
