-- Run from the repository root with LuaJIT:
-- luajit tests/lua/test_sorcerer_spell_contract.lua [path/to/otclient]
-- Exercises spell dispatch without a Wheel unlock and, optionally, the
-- server/client metadata boundary. Engine combat rules require in-game testing.
local attackDirectory = "data/scripts/spells/attack/"

local function loadSpell(file)
	local definition, combats = {}, {}
	local env = setmetatable({}, { __index = _G })
	for _, name in ipairs({
		"CALLBACK_PARAM_LEVELMAGICVALUE", "COMBAT_PARAM_TYPE", "COMBAT_PARAM_EFFECT",
		"COMBAT_PARAM_DISTANCEEFFECT", "COMBAT_DEATHDAMAGE", "COMBAT_ENERGYDAMAGE",
		"COMBAT_FIREDAMAGE", "CONST_ME_MORTAREA", "CONST_ME_ENERGYAREA", "CONST_ME_FIREATTACK",
		"CONST_ANI_ENERGY", "CONST_ANI_FIRE", "AREA_BEAM8", "SOUND_EFFECT_TYPE_SPELL_LIGHTNING",
		"SOUND_EFFECT_TYPE_SPELL_OR_RUNE", "SOUND_EFFECT_TYPE_SPELL_STRONG_ENERGY_STRIKE",
		"SOUND_EFFECT_TYPE_SPELL_STRONG_FLAME_STRIKE", "SOUND_EFFECT_TYPE_SPELL_ULTIMATE_ENERGY_STRIKE",
		"SOUND_EFFECT_TYPE_SPELL_ULTIMATE_FLAME_STRIKE", "AREA_BEAM5", "AREADIAGONAL_BEAM5",
		"CONST_ME_ENERGYHIT", "SOUND_EFFECT_TYPE_SPELL_ENERGY_BEAM", "SOUND_EFFECT_TYPE_SPELL_GREAT_ENERGY_BEAM",
	}) do
		env[name] = name
	end
	env.createCombatArea = function(area)
		assert(area, "undefined combat area")
		return area
	end
	env.Combat = function()
		local combat = { callbacks = {}, parameters = {}, calls = {}, result = true }
		function combat:setCallback(key, name)
			assert(type(env[name]) == "function", "missing callback " .. name)
			self.callbacks[key] = env[name]
		end
		function combat:setParameter(key, value)
			assert(key and value, "undefined combat parameter")
			self.parameters[key] = value
		end
		function combat:setArea(area)
			self.area = area
		end
		function combat:execute(caster, variant)
			table.insert(self.calls, { caster, variant })
			return self.result
		end
		table.insert(combats, combat)
		return combat
	end
	local setters = {
		"group", "id", "name", "words", "level", "mana", "isPremium", "needDirection",
		"blockWalls", "cooldown", "groupCooldown", "vocation", "needLearn", "range",
		"needCasterTargetOrDirection", "castSound", "impactSound",
	}
	env.Spell = function(kind)
		assert(kind == "instant")
		local spell = {}
		for _, key in ipairs(setters) do
			spell[key] = function(_, ...)
				definition[key] = { ... }
			end
		end
		function spell:register()
			definition.onCastSpell = assert(self.onCastSpell)
		end
		return spell
	end
	local chunk = assert(loadfile(attackDirectory .. file .. ".lua"))
	setfenv(chunk, env)()
	return definition, combats
end

local beam, combats = loadSpell("great_death_beam")
assert(#combats == 1, "a regular beam must not select Wheel-grade combat objects")
local combat = combats[1]
-- The caster deliberately has no Wheel APIs: an unlocked ordinary spell must
-- dispatch without consulting a learned-spell grade or revelation stage.
local player = { isPlayer = function() return true end }
local variant = {}
assert(beam.onCastSpell(player, variant))
assert(combat.calls[1][1] == player and combat.calls[1][2] == variant)
combat.result = false
assert(beam.onCastSpell(player, variant) == false, "propagate combat failure")
local previousCalls = #combat.calls
assert(beam.onCastSpell(nil, variant) == false)
assert(beam.onCastSpell({ isPlayer = function() return false end }, variant) == false)
assert(#combat.calls == previousCalls, "invalid casters must not dispatch damage")
assert(not beam.needLearn, "ordinary beam must not require a Wheel spell unlock")
print("PASS: Great Death Beam dispatch without a Wheel unlock")

local spells = { beam }
for file, area in pairs({energy_beam = "AREA_BEAM5", great_energy_beam = "AREA_BEAM8"}) do
	local definition, objects = loadSpell(file)
	assert(#objects == 1 and objects[1].area == area, "Beam Mastery must preserve the central beam length")
	assert(definition.onCastSpell(player, variant))
	objects[1].result = false
	assert(not definition.onCastSpell(player, variant), "failed beam must not retry a second combat")
	assert(#objects[1].calls == 2)
	assert(not definition.onCastSpell(nil, variant))
	table.insert(spells, definition)
end
print("PASS: energy beams keep fixed central lengths and propagate failure once")

for _, file in ipairs({ "lightning", "strong_energy_strike", "strong_flame_strike", "ultimate_energy_strike", "ultimate_flame_strike" }) do
	local spell = loadSpell(file)
	table.insert(spells, spell)
end
print("PASS: spell scripts register their callbacks")

if arg[1] then
	local clientEnv = setmetatable({}, { __index = _G })
	local chunk = assert(loadfile(arg[1] .. "/modules/gamelib/spells.lua"))
	setfenv(chunk, clientEnv)()
	local vocationIds = { ["sorcerer;true"] = 1, ["master sorcerer;true"] = 5 }
	for _, spell in ipairs(spells) do
		local name = spell.name[1]
		local client = assert(clientEnv.SpellInfo.Default[name], name .. " missing in client")
		for serverKey, clientKey in pairs({ id = "id", level = "level", mana = "mana", cooldown = "exhaustion", isPremium = "premium", words = "words" }) do
			assert(spell[serverKey][1] == client[clientKey], name .. ": mismatched " .. clientKey)
		end
		if spell.range then
			assert(spell.range[1] == client.range, name .. ": mismatched range")
		end
		assert(#spell.vocation == #client.vocations, name .. ": mismatched vocations")
		for i, vocation in ipairs(spell.vocation) do
			assert(vocationIds[vocation] == client.vocations[i], name .. ": mismatched vocation")
		end
	end
	assert(clientEnv.SpellInfo.Default[beam.name[1]].special == false)
	print("PASS: server/client spell metadata agrees")
else
	print("SKIP: client metadata comparison (pass the OTClient checkout path)")
end
