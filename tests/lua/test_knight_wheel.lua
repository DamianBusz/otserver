-- LuaJIT tests/lua/test_knight_wheel.lua ../otclient
local root=assert(arg[1])
local env=setmetatable({WheelOfDestiny={vocationId=1,pointInvested={},passivePoints={0,0,0,0},extraPassivePoints={}}},{__index=_G})
local function load(path) setfenv(assert(loadfile(root..'/'..path)),env)() end
load('modules/game_wheel/classes/icons.lua')
load('modules/gamelib/util.lua')
env.table=setmetatable({size=function(t) local n=0;for _ in pairs(t) do n=n+1 end;return n end},{__index=table})
env.WheelOfDestiny.isLitFull=function(i) return env.WheelOfDestiny.pointInvested[i]==env.WheelBonus[i-1].maxPoints end
env.WheelOfDestiny.isLit=function(i) return env.WheelOfDestiny.pointInvested[i]>0 end
load('modules/game_wheel/classes/bonus.lua')
local function plain(t) if type(t)=='string' then return t end;local x={};for i=1,#t,2 do x[#x+1]=t[i] end;return table.concat(x) end
for id in pairs(env.WheelBonus) do env.WheelOfDestiny.pointInvested[id+1]=0 end
-- Independent official Knight column; no inferred replacements from the old slot layout.
local official={8,5,37,7,38,12,37,14,6,38,13,5,11,7,37,10,6,38,39,5,12,40,7,14,6,13,39,5,11,40,10,39,7,40,6,9}
for node,perk in ipairs(official) do assert(env.WheelIcons[env.KNIGHT][node].iconRect==(perk*30)..' 0 30 30','Knight artwork node '..node) end
for _,row in ipairs({
 {6,21,'Front Sweep','+40% Base Damage','Affected area enlarged'},
 {8,24,'Shield Slam','15% life leech','75% total'},
 {11,26,'Groundshaker','-2s Cooldown','+12.5% Base Damage'},
 {13,29,'Intense Wound Cleansing','+125% Base Healing','-60s Cooldown'},
 {16,31,'Fierce Berserk','-30 Mana Cost','+10% Base Damage'},
}) do
 for _,node in ipairs({row[1],row[2]}) do
  for _,full in ipairs({false,true}) do
   env.WheelOfDestiny.pointInvested[node]=full and env.WheelBonus[node-1].maxPoints or 0
   for _,text in ipairs({plain(env.getConvictionBonus(node,true)),plain(env.getConvictionBonusTooltip(node))}) do
    text=text:gsub('\n',' ')
    for j=3,5 do assert(text:find(row[j],1,true),text..' missing '..row[j]) end
   end
  end
 end
end
local names={}
for _,p in pairs(env.getConvictionPerks()) do names[p.perk]=plain(p.tooltip) end
assert(names['Aug. Shield Slam']:find('75% total',1,true))
assert(names['Aug. Groundshaker']:find('12.5%',1,true))
assert(names['Aug. Front Sweep']:find('40%',1,true))
assert(not names['Aug. Chivalrous Cha...'])
for _,points in ipairs({0,250,500,1000}) do
 env.WheelOfDestiny.passivePoints[3]=points
 local name,detail=env.getPassiveInfo(3);detail=plain(detail)
 assert(name:find('Combat Mastery',1,true))
 for _,term in ipairs({'14%','12%','10%','incoming','outgoing','own HP','target HP','shield','two-handed'}) do assert(detail:find(term,1,true),detail..' missing '..term) end
 assert(not detail:find('critical',1,true))
end
for _,text in ipairs({plain(env.getConvictionBonus(36,true)),plain(env.getConvictionBonusTooltip(36)),env.WheelConsts.special_2[1][2]}) do
 assert(text:find('10%',1,true) and text:find('20%',1,true))
 assert(not text:find('challeng',1,true))
end
print('PASS: Knight official Wheel artwork, node pairs, locked/full/hover text, cumulative summaries, Battle Healing and Combat Mastery stages')
