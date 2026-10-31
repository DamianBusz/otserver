-- OTClient contract: 86 = authoritative stance state; 87 = aimed cast request.
local request = CreatureEvent("AimedSpellRequest")
local function coordinate(value, maximum)
	return type(value) == "number" and value == math.floor(value) and value >= 0 and value <= maximum
end
function request.onExtendedOpcode(player, opcode, buffer)
	if opcode == 86 then
		local knightStance = Player.getKnightStance and player:getKnightStance()
		if knightStance and knightStance ~= "none" then
			player:sendExtendedOpcode(86, knightStance)
			return
		end
		local druidStance = Player.getDruidStance and player:getDruidStance()
		if druidStance and druidStance ~= "none" then
			player:sendExtendedOpcode(86, druidStance)
			return
		end
		local paladinStance = Player.getPaladinStance and player:getPaladinStance()
		if paladinStance and paladinStance ~= "none" then
			player:sendExtendedOpcode(86, paladinStance)
			return
		end
		if Player.getElementalStance then
			player:sendExtendedOpcode(86, player:getElementalStance())
		end
		return
	end
	if opcode ~= 87 or not Player.castAimedSpell or #buffer > 512 then
		return
	end
	local x, y, z, words, param = buffer:match("^(%d+),(%d+),(%d+)\n([^\n]+)\n([^\n]*)$")
	x, y, z = tonumber(x), tonumber(y), tonumber(z)
	if not words or #words > 100 or #param > 100
		or not coordinate(x, 65534) or not coordinate(y, 65534) or not coordinate(z, 15) then
		return
	end
	player:castAimedSpell(words, Position(x, y, z), param)
end
request:register()

local login = CreatureEvent("RegisterAimedSpells")
function login.onLogin(player)
	player:registerEvent("AimedSpellRequest")
	return true
end
login:register()
