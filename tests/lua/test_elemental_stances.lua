-- LuaJIT tests/lua/test_elemental_stances.lua [path/to/otclient]
local spells, events = {}, {}
local env = setmetatable({
	Player = { setElementalStance = function() end },
	CONST_ME_EXPLOSIONAREA = 5, CONST_ME_BLOW_BLUE = 287, CONST_ME_BLOW_WHITE = 285,
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
loadScript("data/scripts/spells/support/elemental_stances.lua")
loadScript("data/scripts/creaturescripts/player/elemental_stance.lua")

local persisted, effects, messages = {}, {}, {}
local player = { allowed = true }
function player:getPlayer() return self end
function player:setElementalStance(key)
	if not self.allowed then return false end
	self.stance = key
	return true
end
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

assert(#spells == 3)
for i, key in ipairs({ "flames", "thunder", "decay" }) do
	assert(spells[i].onCastSpell(player, {}))
	assert(player.stance == key, "spell closure selected the wrong stance")
	assert(persisted["sorcerer-elemental-stance"] == key)
end
assert(effects[1] == 5 and effects[2] == 287 and effects[3] == 285)
assert(#messages == 3)
print("PASS: all stance casts select, persist and announce their own element")

player.allowed = false
assert(spells[1].onCastSpell(player, {}) == false)
assert(spells[1].onCastSpell(nil, {}) == false)
assert(#effects == 3 and #messages == 3 and persisted["sorcerer-elemental-stance"] == "decay")
print("PASS: rejected casts have no persistence or visual side effects")

player.allowed = true
player.stance = nil
assert(events.RestoreElementalStance.onLogin(player))
assert(player.stance == "decay")
persisted["sorcerer-elemental-stance"] = 123 -- malformed persisted state is ignored
player.stance = nil
assert(events.RestoreElementalStance.onLogin(player))
assert(player.stance == nil)
print("PASS: login restores valid saved selection and ignores wrong types")

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
		assert(client.Spells.getImageClip(info.clientId):match("^%d+ 0 32 32$"))
	end
	assert(client.SpellGroups[12] == "Stance")
	print("PASS: stance metadata and cooldown groups agree with OTClient")
end
