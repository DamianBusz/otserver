-- Run with LuaJIT tests/lua/test_spell_assignment.lua ../otclient.
local root = assert(arg[1])
local function loadIn(path, env) return setfenv(assert(loadfile(root .. '/' .. path)), env)() end
local function widget()
    local w = {children = {}, text = ''}
    function w:setText(v) self.text = v end
    function w:getText() return self.text end
    function w:clearText() self.text = '' end
    function w:setId(v) self.id = tostring(v) end
    function w:getId() return self.id end
    function w:setChecked(v) assert(not self.destroyed, 'selection retained a destroyed row'); self.checked = v end
    function w:getChildren() return self.children end
    function w:destroyChildren() for _, c in ipairs(self.children) do c.destroyed = true end; self.children = {} end
    function w:moveChildToIndex(c, i)
        for n, child in ipairs(self.children) do if c == child then table.remove(self.children, n); break end end
        table.insert(self.children, i, c)
    end
    for _, name in ipairs({'setVisible','setOn','setEnabled','show','raise','setTitle','setImageSource','setImageClip','ensureChildVisible'}) do w[name] = function() end end
    return w
end
local env = setmetatable({UIWidget = {}, signalcall = function(fn, ...) if fn then return fn(...) end end}, {__index = _G})
env.extends = function() return {internalCreate = function() return setmetatable(widget(), {__index = env.UIRadioGroup}) end} end
loadIn('modules/corelib/ui/uiradiogroup.lua', env)
loadIn('modules/gamelib/spells.lua', env)
loadIn('modules/game_actionbar/logics/SpellControls.lua', env)
local widgets, saved, closed
local controller = {}
function controller:loadHtml()
    widgets = {}
    for _, id in ipairs({'spellList','preview','image','paramLabel','paramText','castMode','dev','buttonOk','buttonApply','buttonClose'}) do widgets['#' .. id] = widget() end
    local combo = widgets['#castMode']; combo.options = {}
    function combo:addOption(v) self.options[v] = true end
    function combo:setCurrentOption(v) assert(self.options[v]); self.option = v end
    function combo:getCurrentOption() return {text = self.option} end
    self.ui = widget(); closed = false
end
function controller:unloadHtml() closed = true; self.ui = nil end
function controller:findWidget(id) return assert(widgets[id], id) end
env.ActionBarController = controller
env.table = setmetatable({contains = function(t, v) for _, x in ipairs(t) do if x == v then return true end end end}, {__index = table})
env.player = {getVocation = function() return 0 end, getLevel = function() return 500 end}
env.translateVocation = function(v) return v end
env.modules = {gamelib = env}
env.g_ui = {createWidget = function(_, parent)
    local w = widget(); w.image = widget(); w.image.gray = widget(); w.levelLabel = widget(); w.imageGroup = widget()
    table.insert(parent.children, w); return w
end}
env.ApiJson = {createOrUpdateText = function(bar, button, text, auto, mode) saved = {text=text, mode=mode} end}
env.updateButton = function() end
loadIn('modules/game_actionbar/logics/ActionAssignmentWindows.lua', env)
local button = {cache = {spellData = env.Spells.getSpellDataByParamWords('exevo vis lux'), castMode = 'cursor'}}
function button:getParent() return {getParent = function() return {locked = false} end} end
function button:getId() return '1.17' end
env.assignSpell(button)
assert(widgets['#preview']:getText():find('exevo vis lux', 1, true), 'saved spell preview must be populated')
assert(widgets['#castMode']:getCurrentOption().text == 'At cursor', 'saved mode must survive reopening')
local function clickWords(words)
    for _, row in ipairs(widgets['#spellList'].children) do
        if row:getText():match('\n(.*)') == words then row.onClick(row); return row end
    end
    error('missing spell ' .. words)
end
clickWords('exevo mort ora')
assert(widgets['#preview']:getText():find('Death Echo', 1, true))
assert(widgets['#castMode']:getCurrentOption().text == 'Free aim (click map)')
widgets['#castMode']:setCurrentOption('On current target')
widgets['#buttonApply'].onClick()
assert(saved.text == 'exevo mort ora' and saved.mode == 'target' and not closed)
widgets['#dev'].onClick()
clickWords('uteta flam') -- rebuilt list must discard destroyed radio selections
widgets['#buttonOk'].onClick()
assert(saved.text == 'uteta flam' and saved.mode == 'normal' and closed)
print('PASS: spell selection, saved preview/mode, Apply/OK and Show All list rebuilding')
