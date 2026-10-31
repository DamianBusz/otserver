-- LuaJIT tests/lua/test_druid_wheel.lua ../otclient
local root = assert(arg[1])
local env = setmetatable({WheelOfDestiny = {vocationId = 4, pointInvested = {}, passivePoints = {0,0,0,0}, extraPassivePoints = {}}}, {__index = _G})
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
-- Independently extracted from the installed official client, not our slot table.
-- SHA-256 and table provenance: docs/systems/druid-rebalance.md.
local officialPerks = {28,5,37,34,38,29,37,32,6,38,33,5,30,34,37,31,6,38,39,5,29,40,34,32,6,33,39,5,30,40,31,39,34,40,6,35}
for node, perk in ipairs(officialPerks) do
    assert(env.WheelIcons[env.DRUID][node].iconRect == (perk * 30) .. " 0 30 30",
        "official druid perk artwork mismatch at node " .. node)
end
local officialPoints = {200,150,100,100,150,200,150,100,75,75,100,150,100,75,50,50,75,100,100,75,50,50,75,100,150,100,75,75,100,150,200,150,100,100,150,200}
local officialDedications = {2,4,0,1,0,2,4,0,1,3,1,0,0,1,3,4,3,1,4,0,1,0,4,3,3,4,0,4,3,1,2,3,4,3,1,2}
local dedicationNames = {[0]='health', 'mana', 'lifemana', 'capacity', 'mitigation'}
for node = 1,36 do
    assert(env.WheelBonus[node-1].maxPoints == officialPoints[node])
    assert(env.WheelBonus[node-1].dedication == dedicationNames[officialDedications[node]])
end
load('modules/game_wheel/classes/buttons.lua')
assert(env.WheelButtons[31].colorImageBase:find('/bottom_left/slot9/',1,true))
assert(env.WheelButtons[16].colorImageBase:find('/top_right/slot1/',1,true))
for _, row in ipairs({
    {16,31,'Strong Ice Wave','+6% Base Damage','Affected area enlarged'},
    {8,24,'Mass Healing','+4% Base Healing','Affected area enlarged'},
    {6,21,'Forked Spells','-2s Cooldown','+1 Target'},
    {13,29,'Terra Wave','+6.5% Base Damage','10% life leech'},
    {11,26,'Heal Friend','+4% Base Healing','+6% Base Healing'}
}) do
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
assert(names['Aug. Forked Spells']:find('+1 Target',1,true))
assert(names['Aug. Heal Friend']:find('+6% Base Healing',1,true))
assert(names['Aug. Terra Wave']:find('+6.5% Base Damage',1,true))
assert(names['Aug. Mass Healing']:find('+4% Base Healing',1,true))
assert(not names["Aug. Nature's Embrace"])
assert(env.WheelConsts.special_1[env.DRUID][2]:find('25%',1,true))
for _, points in ipairs({0,250,500,1000}) do
    env.WheelOfDestiny.passivePoints[2]=points
    local name, detail=env.getPassiveInfo(2); detail=plain(detail)
    assert(name:find('Blessing of the Grove',1,true))
    for _, text in ipairs({'critically heal','self-healing','5%','7.5%','10%','below 60%','doubles below 30%'}) do
        assert(detail:find(text,1,true), detail..' missing '..text)
    end
end
assert(env.SupremeGemDescription[60].text:find('-5s Cooldown',1,true))
assert(env.VocationSupremeMods[6][60].tooltip:find('-5s Cooldown',1,true)) -- protocol vocation: Elder Druid
print('PASS: druid Wheel node copies, locked/full descriptions, cumulative summaries, Grove stages, gem identities and artwork')
