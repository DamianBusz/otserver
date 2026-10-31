-- LuaJIT tests/lua/test_paladin_wheel.lua ../otclient
local root = assert(arg[1])
local env = setmetatable({WheelOfDestiny = {vocationId = 2, pointInvested = {}, passivePoints = {0,0,0,0}, extraPassivePoints = {}}}, {__index = _G})
local function load(path) setfenv(assert(loadfile(root .. '/' .. path)), env)() end
load('modules/game_wheel/classes/icons.lua')
load('modules/gamelib/util.lua') -- Real startup overrides the older corelib colour helper.
env.table = setmetatable({size = function(t) local n=0; for _ in pairs(t) do n=n+1 end; return n end}, {__index=table})
env.WheelOfDestiny.isLitFull = function(index)
    return env.WheelOfDestiny.pointInvested[index] == env.WheelBonus[index-1].maxPoints
end
env.WheelOfDestiny.isLit = function(index) return env.WheelOfDestiny.pointInvested[index] > 0 end
load('modules/game_wheel/classes/bonus.lua')
local function plain(t)
    if type(t) == 'string' then return t end
    local result={}; for i=1,#t,2 do result[#result+1]=t[i] end; return table.concat(result)
end
for id in pairs(env.WheelBonus) do env.WheelOfDestiny.pointInvested[id+1] = 0 end
-- Both copies of each saved node retain their identity and use the new artwork.
for _, node in ipairs({6,21}) do assert(env.WheelIcons[env.PALADIN][node].iconRect == "660 0 30 30") end
for _, node in ipairs({13,29}) do assert(env.WheelIcons[env.PALADIN][node].iconRect == "570 0 30 30") end
for _, row in ipairs({{6,21,'Ethereal Barrage','10% life leech','+10% Critical Hit Chance'}, {13,29,'Divine Barrage','+8% Base Damage','+12% Base Damage'}, {11,26,'Divine Dazzle','+2 additional','-8s'}, {16,31,'Divine Caldera','-20 Mana Cost','+10% Base Damage'}}) do
    for _, node in ipairs({row[1],row[2]}) do
        local tooltip=plain(env.getConvictionBonusTooltip(node)):gsub('\n',' ')
        for j=3,5 do assert(tooltip:find(row[j],1,true), tooltip..' missing '..row[j]) end
        env.WheelOfDestiny.pointInvested[node]=env.WheelBonus[node-1].maxPoints
        local text=plain(env.getConvictionBonus(node,true)):gsub('\n',' ')
        for j=3,5 do assert(text:find(row[j],1,true), text..' missing '..row[j]) end
    end
end
local names={}
for _,perk in pairs(env.getConvictionPerks()) do names[perk.perk]=plain(perk.tooltip) end
assert(names['Aug. Ethereal Barrage']:find('10% life leech',1,true))
assert(names['Aug. Divine Barrage']:find('+12% Base Damage',1,true))
assert(not names['Aug. Sharpshooter'] and not names['Aug. Swift Foot'])
print('PASS: paladin saved nodes, replacement icons, full node descriptions and summary tooltips')
