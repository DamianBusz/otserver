-- LuaJIT tests/lua/test_wheel_text_rendering.lua ../otclient [staged-wheelclass]
-- Exercise real Wheel UI handlers against the native string-only text contract.
local root = assert(arg[1])
local env = setmetatable({UIWidget = {}}, {__index = _G})
local function load(path) setfenv(assert(loadfile(path)), env)() end
load(root .. '/modules/corelib/ui/uiwidget.lua')
load(arg[2] or root .. '/modules/game_wheel/classes/wheelclass.lua')
load(root .. '/modules/game_wheel/classes/icons.lua')
load(root .. '/modules/game_wheel/classes/buttons.lua')
load(root .. '/modules/gamelib/util.lua') -- Use the real global helper, never a test replacement.
env.table = setmetatable({size = function(t) local n=0; for _ in pairs(t) do n=n+1 end; return n end}, {__index=table})
load(root .. '/modules/game_wheel/classes/bonus.lua')
local function widget()
    local w = {text = ''}
    function w:setText(v) assert(type(v)=='string', 'native text must be a string'); self.text=v end
    function w:setColoredText(v)
        assert(type(v)=='string', 'native coloured text must be a markup string, not a Lua table')
        self.markup=v
        self.text=v:gsub('{([^}]+),%s*[^}]+}', '%1')
    end
    function w:setTooltip(v) assert(v==nil or type(v)=='string', 'plain tooltip must be a string'); self.tooltip=v end
    function w:removeTooltip() self.tooltip=nil; self.coloredTooltip=nil end
    function w:parseColoreDisplayToolTip(v) assert(type(v)=='string'); self.coloredTooltip=v end
    w.parseColoredText=env.UIWidget.parseColoredText
    function w:recursiveGetChildById(id) return self[id] end
    for _, method in ipairs({'setColor','setVisible','setOpacity','setImageSource','setValue','setPercent','destroyChildren'}) do
        w[method]=function() end
    end
    return setmetatable(w, {__index=function(t,k) local child=widget();rawset(t,k,child);return child end})
end
local window=widget()
env.wheelOfDestinyWindow=window
env.wheelPanel=widget()
local wheel=env.WheelOfDestiny
local selected=window.selection.tabContent.conviction
local hovered=window.info.tabContent.information.tabContent.conviction2
local index=1
wheel.getSliceIndex=function() return index end
-- Locked and unlocked nodes must both render, for every vocation.
for vocation=1,5 do
    wheel.vocationId=vocation
    for _, filled in ipairs({false,true}) do
        for node=1,36 do wheel.pointInvested[node]=filled and env.WheelBonus[node-1].maxPoints or 0 end
        for node=1,36 do
            index=node
            wheel.configureConviction(node)
            assert(not selected.text:find('�',1,true), 'broken grade glyph')
            assert(#selected.text>0, 'blank selected node '..node..' for vocation '..vocation)
            wheel.mouseIndex=0
            wheel.onMouseMove(nil, {}, {})
            assert(#hovered.text>0, 'blank hover node '..node..' for vocation '..vocation)
        end
    end
end
wheel.vocationId=env.SORCERER
wheel.configureConviction(6)
assert(selected.text:find('+5%% Base Damage'))
assert(selected.markup:find('#C0C0C0',1,true), 'unlocked colours must survive conversion')
wheel.configureConviction(11)
assert(selected.text:find('Augmented Death Echo',1,true) and selected.text:find('+12%',1,true))
-- Summary tooltips use the actual UIWidget BBCode parser, not tostring(table).
local rows={}
env.g_ui={createWidget=function() local w=widget();rows[#rows+1]=w;return w end}
wheel.configureConvictionPerk()
local tooltipRenderer=widget()
local foundDeath=false
for _, row in ipairs(rows) do
    local markup=rawget(row.info,'coloredTooltip')
    if markup then
        assert(rawget(row.info,'tooltip')==nil)
        tooltipRenderer:parseColoredText(markup)
        assert(#tooltipRenderer.text>0 and not tooltipRenderer.text:find('table:',1,true))
        if row.perk.text=='Aug. Death Echo' then
            foundDeath=true
            assert(tooltipRenderer.text:find('-2s Cooldown',1,true))
            assert(tooltipRenderer.text:find('+12% Base Damage',1,true))
        end
    end
end
assert(foundDeath, 'Death Echo summary tooltip must render')
-- Gem descriptions use the same legacy text/color data and native renderer.
env.tr=string.format
wheel.resetPassiveFocus=function() end
local gem
local filled=3
env.GemAtelier={getFilledVesselCount=function() return filled end, getEquipedGem=function() return gem end}
env.Workshop={getGemInformationByBonus=function(bonus) return 'Gem bonus '..bonus,0 end}
for quality=0,2 do
    gem={gemType=quality,gemID=1,lesserBonus=1,regularBonus=2,supremeBonus=3}
    wheel.onGemVesselClick(0)
    local content=window.selection.gemContent
    assert(content.modification0.text=='Gem bonus 1')
    if quality>=1 then assert(content.modification1.text=='Gem bonus 2') end
    if quality>=2 then assert(content.modification2.text=='Gem bonus 3') end
    assert(content.VRBonus.text:find('Damage and Healing',1,true))
    assert(#content.gemName.text>0)
    assert(not content.gemName.text:find('�',1,true), 'broken gem grade glyph')
    assert(content.gemName.text:find('(I)',1,true), 'missing readable gem grade')
end
print('PASS: Wheel selected/hover text for all vocations, summary tooltip renderer and gem descriptions')
