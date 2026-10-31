-- Run with LuaJIT tests/lua/test_spell_hud.lua ../otclient
local root = assert(arg[1], 'OTClient path required')
local function loadIn(path, env)
    return setfenv(assert(loadfile(root .. '/' .. path)), env)()
end
local function widget()
    local w = { children = {}, visible = false, marginTop = 12, height = 38 }
    function w:getChildById(id)
        self.children[id] = self.children[id] or widget()
        return self.children[id]
    end
    w.recursiveGetChildById = w.getChildById
    w.getChildByIndex = w.getChildById
    function w:setMarginTop(v) self.marginTop = v end
    function w:getMarginTop() return self.marginTop end
    function w:getHeight() return self.height end
    function w:isDestroyed() return false end
    function w:setVisible(v) self.visible = v end
    function w:show() self.visible = true end
    function w:hide() self.visible = false end
    function w:setText(v) self.text = v end
    function w:setImageSource(v) self.image = v end
    function w:setImageClip(v) self.clip = v end
    function w:setColor() end
    function w:setId() end
    return w
end
local online, ping, badge, stateCallback = false
local map = widget()
local shared = setmetatable({
    connect = function() end, addEvent = function(fn) fn() end,
    g_app = {}, Keybind = {new = function() end, bind = function() end}, Services = {},
    g_platform = {isMobile = function() return false end},
    g_game = {isOnline = function() return online end, getFeature = function() return true end,
        getProtocolGame = function() return {sendExtendedOpcode = function() end} end},
    g_ui = {
        displayUI = function() return widget() end,
        loadUI = function() ping = widget(); return ping end,
        importStyle = function(path)
            assert(path == '/game_actionbar/otui/stance.otui', 'resource must be module-absolute')
            local f = assert(io.open(root .. '/modules' .. path)); f:close()
        end,
        createWidget = function(style)
            local w = widget()
            if style == 'ActiveStance' then
                badge = w; w.label = widget(); w.icon = widget()
            end
            return w
        end,
    },
    modules = {
        client_options = {getOption = function() return true end},
        game_interface = {currentViewMode = 2, getMapPanel = function() return map end},
    },
    ProtocolGame = {registerExtendedOpcode = function(_, fn) stateCallback = fn end},
    actionBars = {},
}, {__index = _G})
loadIn('modules/gamelib/spells.lua', shared)
local top = setmetatable({}, {__index = shared})
loadIn('modules/client_topmenu/topmenu.lua', top)
shared.modules.client_topmenu = top
-- Avoid unrelated top-menu visibility logic; the real ping lifecycle still runs.
top.init()
top.showGameButtons = function() end
top.hideGameButtons = function() end
local hud = setmetatable({}, {__index = shared})
loadIn('modules/game_actionbar/logics/SpellControls.lua', hud)
hud.initSpellControls()
online = true
hud.startSpellControls()
stateCallback(nil, 86, 'flames') -- state can arrive before the ping panel is created
assert(hud.activeStanceSpellId == 304 and badge.visible)
top.online()
assert(ping.marginTop == 58, 'ping must be below badge bottom (50) plus gap (8)')
assert(badge.icon.image:find('rebalance%-icons'))
stateCallback(nil, 86, 'thunder')
assert(ping.marginTop == 58 and hud.activeStanceSpellId == 305)
stateCallback(nil, 86, 'sharpshooter')
assert(ping.marginTop == 58 and hud.activeStanceSpellId == 313 and badge.visible)
stateCallback(nil, 86, 'divine-defiance')
assert(ping.marginTop == 58 and hud.activeStanceSpellId == 314 and badge.visible)
stateCallback(nil, 86, 'shared-conservation')
assert(ping.marginTop == 58 and hud.activeStanceSpellId == 309 and badge.visible)
assert(badge.label.text == 'Shared Conservation')
stateCallback(nil, 86, 'elemental-synthesis')
assert(ping.marginTop == 58 and hud.activeStanceSpellId == 319 and badge.visible)
assert(badge.label.text == 'Elemental Synthesis')
stateCallback(nil, 86, 'blood-rage')
assert(ping.marginTop == 58 and hud.activeStanceSpellId == 133 and badge.visible)
assert(badge.label.text == 'Blood Rage')
stateCallback(nil, 86, 'protector')
assert(ping.marginTop == 58 and hud.activeStanceSpellId == 132 and badge.visible)
assert(badge.label.text == 'Protector')
stateCallback(nil, 86, 'none')
assert(ping.marginTop == 35 and not badge.visible and hud.activeStanceSpellId == 0)
stateCallback(nil, 86, 'decay')
hud.stopSpellControls()
assert(ping.marginTop == 35 and not badge.visible)
online = false
stateCallback(nil, 86, 'flames')
assert(not badge.visible, 'late stance message after logout must not restore the badge')
print('PASS: stance/ping layout, creation order, switching, clearing and logout')

local S = shared.Spells
for id, index in pairs({[309]=191,[319]=190,[317]=197,[318]=198,[302]=185,[303]=186,[304]=187,[305]=188,[306]=189,[310]=192,[313]=193,[314]=194}) do
    assert(S.getImageClip(id) == (index*32 .. ' 0 32 32'))
    assert(S.getImageClipCooldown(id):match('^' .. index*20 .. ' 0 20'))
    assert(S.getIconFileByClientId(id):find('rebalance%-icons%-32x32'))
    assert(S.getIconFileByClientId(id, nil, true):find('rebalance%-icons%-20x20'))
end
assert(S.getIconFileByClientId(157) == '/images/game/spells/spell-icons-32x32')
assert(S.getImageClip(157) == '5024 0 32 32', 'existing death-beam icon must retain its old atlas slot')
print('PASS: official new-spell icon slots without reordering existing icons')
