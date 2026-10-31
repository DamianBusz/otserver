-- LuaJIT tests/lua/test_paladin_stances.lua [path/to/otclient]
local spells, events = {}, {}
local env = setmetatable({
	Player = { setPaladinStance = function() end },
	CONST_ME_EXPLOSIONAREA = 5, CONST_ME_BLOW_GREEN = 286,
}, { __index = _G })

-- Use only message names actually exported by this server, rather than
-- inventing constants in the fixture and hiding invalid Lua globals.
local enumsFile = assert(io.open("src/lua/functions/core/game/lua_enums.cpp"))
local enums = enumsFile:read("*a")
enumsFile:close()
for name in enums:gmatch("registerEnum%(L, (MESSAGE_[A-Z_]+)%)") do env[name] = name end

env.Spell = function()
	local spell = { settings = {} }
	for _, name in ipairs({ "name", "words", "id", "group", "level", "mana", "isPremium", "isSelfTarget", "isAggressive", "cooldown", "groupCooldown", "vocation" }) do
		spell[name] = function(_, ...)
			spell.settings[name] = { ... }
		end
	end
	function spell:register()
		table.insert(spells, self)
	end
	return spell
end
env.CreatureEvent = function(name)
	local event = {}
	function event:register() events[name] = self end
	return event
end
local function loadScript(path)
	local chunk = assert(loadfile(path))
	setfenv(chunk, env)()
end
loadScript("data/scripts/spells/support/paladin_stances.lua")
loadScript("data/scripts/creaturescripts/player/paladin_stance.lua")

local persisted, effects, messages = {}, {}, {}
local player = { allowed = true }
function player:getPlayer() return self end
function player:setPaladinStance(key)
	if not self.allowed then return false end
	self.stance = key
	return true
end
function player:getPaladinStance() return self.stance or "none" end
function player:kv()
	return {
		set = function(_, key, value) persisted[key] = value end,
		get = function(_, key) return persisted[key] end,
	}
end
function player:getPosition()
	return { sendMagicEffect = function(_, effect) table.insert(effects, effect) end }
end
function player:sendTextMessage(kind, message)
	assert(kind == env.MESSAGE_STATUS, "stance feedback must use the registered MESSAGE_STATUS type")
	table.insert(messages, message)
end

assert(#spells == 2)
for i, key in ipairs({ "sharpshooter", "divine-defiance" }) do
	assert(spells[i].onCastSpell(player, {}))
	assert(player.stance == key, "spell closure selected the wrong stance")
	assert(persisted["paladin-stance"] == key)
end
assert(effects[1] == 5 and effects[2] == 286)
assert(#messages == 2)
print("PASS: all stance casts select, persist and announce their own stance")

player.allowed = false
assert(spells[1].onCastSpell(player, {}) == false)
assert(spells[1].onCastSpell(nil, {}) == false)
assert(#effects == 2 and #messages == 2 and persisted["paladin-stance"] == "divine-defiance")
print("PASS: rejected casts have no persistence or visual side effects")

player.allowed = true
player.stance = nil
assert(events.RestorePaladinStance.onLogin(player))
assert(player.stance == "divine-defiance")
persisted["paladin-stance"] = 123 -- malformed persisted state is ignored
player.stance = nil
assert(events.RestorePaladinStance.onLogin(player))
assert(player.stance == nil)
print("PASS: login restores valid saved selection and ignores wrong types")

player.stance = "divine-defiance"
assert(spells[2].onCastSpell(player, {}))
assert(player.stance == "none" and persisted["paladin-stance"] == "none")
assert(events.RestorePaladinStance.onLogin(player) and player.stance == "none")
assert(spells[1].onCastSpell(player, {}))
assert(player.stance == "sharpshooter")
for _, spell in ipairs(spells) do
	assert(spell.settings.vocation[1] == "royal paladin;true")
end
print("PASS: recasting toggles off, saves none, and switching selects one stance")

if arg[1] then
	local client = setmetatable({}, { __index = _G })
	setfenv(assert(loadfile(arg[1] .. "/modules/gamelib/spells.lua")), client)()
	for _, spell in ipairs(spells) do
		local settings = spell.settings
		local info = assert(client.SpellInfo.Default[settings.name[1]])
		for serverKey, clientKey in pairs({ id = "id", words = "words", level = "level", mana = "mana", cooldown = "exhaustion", isPremium = "premium" }) do
			assert(settings[serverKey][1] == info[clientKey], "client/server mismatch: " .. clientKey)
		end
		assert(info.group[3] == settings.groupCooldown[1] and info.group[12] == settings.groupCooldown[2])
		assert(#info.vocations == 1 and info.vocations[1] == 7)
		assert(client.Spells.getImageClip(info.clientId):match("^%d+ 0 32 32$"))
	end
	assert(client.SpellGroups[12] == "Stance")
	print("PASS: stance metadata and cooldown groups agree with OTClient")
end
