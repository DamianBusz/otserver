-- LuaJIT tests/lua/test_sorcerer_wheel.lua ../otclient
local root = assert(arg[1])
local env = setmetatable({WheelOfDestiny = {vocationId = 3, pointInvested = {}, passivePoints = {0,0,0,0}, extraPassivePoints = {}}}, {__index = _G})
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
-- The current official atlas moved these replacement images: 26 is Special
-- Spells, 36 is Death Echo. Persisted node identities do not change with artwork.
for _, node in ipairs({8,24}) do assert(env.WheelIcons[env.SORCERER][node].iconRect == "780 0 30 30") end
for _, node in ipairs({11,26}) do assert(env.WheelIcons[env.SORCERER][node].iconRect == "1080 0 30 30") end
for _, row in ipairs({{6,21,'Focus Spells','+5% Base Damage','-4s Cooldown'}, {16,31,'Great Fire Wave','15% critical extra','+5% Base Damage'}, {8,24,'Special Spells','-4s Cooldown','+50% Base Damage'}, {11,26,'Death Echo','-2s Cooldown','+12% Base Damage'}, {13,29,'Energy Wave','Affected area enlarged','+10% Base Damage'}}) do
    for _, i in ipairs({row[1],row[2]}) do
        local tooltip = env.getConvictionBonusTooltip(i)
        for j=3,5 do assert(tooltip:find(row[j],1,true), tooltip .. ' missing ' .. row[j]) end
        env.WheelOfDestiny.pointInvested[i]=env.WheelBonus[i-1].maxPoints
        local text=plain(env.getConvictionBonus(i,true))
        for j=3,5 do assert(text:find(row[j],1,true), text .. ' missing ' .. row[j]) end
    end
end
local perks=env.getConvictionPerks()
local names={}
for _, p in pairs(perks) do names[p.perk]=plain(p.tooltip) end
assert(names['Aug. Focus Spells']:find('+5% Base Damage',1,true))
assert(not names['Aug. Focus Spells']:find('+8%',1,true))
assert(names['Aug. Focus Spells']:find('Focus secondary group cooldown -4s',1,true))
assert(names['Aug. Great Fire Wave']:find('+5% Base Damage',1,true))
assert(names['Aug. Special Spells']:find('+50%',1,true))
assert(names['Aug. Death Echo']:find('+12%',1,true))
assert(names['Aug. Energy Wave']:find('+10%',1,true))
assert(not names['Aug. Magic Shield'] and not names['Aug. Sap Strength'])
for _, points in ipairs({0,250,500,1000}) do
    env.WheelOfDestiny.passivePoints[2]=points
    env.WheelOfDestiny.passivePoints[3]=points
    local name, detail=env.getPassiveInfo(2); detail=plain(detail)
    assert(name:find('Beam Mastery',1,true) and not name:find('unlock',1,true))
    for _, pct in ipairs({'25%','40%','70%'}) do assert(detail:find(pct,1,true)) end
    name,detail=env.getPassiveInfo(3);detail=plain(detail)
    assert(name:find('Lord of Destruction',1,true))
    assert(detail:find('22.5%',1,true) and detail:find('45/52.5/60%',1,true))
end
assert(env.WheelConsts.special_2[3][2]:find('2 seconds',1,true))
assert(env.WheelConsts.mitigation == 0.075)
local _, gift = env.getPassiveInfo(1)
assert(plain(gift):find('20/25/30% of maximum mana',1,true))
-- Other vocations retain their existing augments.
env.WheelOfDestiny.vocationId=env.DRUID
assert(plain(env.getConvictionBonus(8,true)):find('Mass Healing',1,true))
print('PASS: Sorcerer Wheel slices, summary, stage descriptions and other-vocation isolation')
