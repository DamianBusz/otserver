-- LuaJIT tests/lua/test_ground_spell_aim.lua ../otclient
local root = assert(arg[1])
local sent, grabber, grabs = {}, nil, 0
local position = { x = 32370, y = 32241, z = 7 }
local map = {
	containsPoint = function() return true end,
	getPosition = function() return position end,
	getClassName = function() return "UIGameMap" end,
}
local env = setmetatable({
	MouseLeftButton = 1, KeyEscape = 27,
	ProtocolGame = { registerExtendedOpcode = function() end },
	g_mouse = { pushCursor = function() end, popCursor = function() end },
	g_window = { getMousePosition = function() return { x = 100, y = 100 } end },
	g_game = {
		getLocalPlayer = function() return { getPosition = function() return position end } end,
		getAttackingCreature = function() error("Free aim must not request a creature") end,
		getProtocolGame = function() return { sendExtendedOpcode = function(_, opcode, data)
			assert(opcode == 87); sent[#sent + 1] = data
		end } end,
	},
	modules = { game_interface = {
		getMapPanel = function() return map end,
		getRootPanel = function() return { recursiveGetChildByPos = function() return map end } end,
	}, game_textmessage = { displayFailureMessage = function() error("Unexpected aim rejection") end } },
	g_ui = { importStyle = function() end, createWidget = function()
		grabber = { setVisible = function() end,
			grabMouse = function() grabs = grabs + 1 end, ungrabMouse = function() grabs = grabs - 1 end,
			grabKeyboard = function() end, ungrabKeyboard = function() end }
		return grabber
	end },
}, { __index = _G })
for _, path in ipairs({ "modules/gamelib/spells.lua", "modules/game_actionbar/logics/SpellControls.lua" }) do
	setfenv(assert(loadfile(root .. "/" .. path)), env)()
end
env.initSpellControls()
for _, name in ipairs({ "Divine Barrage", "Ethereal Barrage", "Divine Grenade", "Death Echo" }) do
	local spell = assert(env.SpellInfo.Default[name])
	assert(spell.groundTarget and not spell.needTarget)
	assert(env.executeSpellAction({ spellData = spell, castMode = "cursor" }))
	assert(sent[#sent] == "32370,32241,7\n" .. spell.words .. "\n")
	local count = #sent
	assert(env.executeSpellAction({ spellData = spell })) -- default is free aim
	assert(grabs == 1 and #sent == count)
	grabber.onMouseRelease(nil, { x = 100, y = 100 }, 1)
	assert(grabs == 0 and #sent == count + 1)
	assert(sent[#sent] == "32370,32241,7\n" .. spell.words .. "\n")
end
print("PASS: barrages, grenade and Death Echo aim at empty ground without a creature target")
