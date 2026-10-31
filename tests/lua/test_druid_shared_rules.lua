local env = setmetatable({Party={}}, {__index=_G})
setfenv(assert(loadfile('data/events/scripts/party.lua')),env)()
for _, vocation in ipairs({2,3}) do
 for _, size in ipairs({vocation,4,8}) do
  local party = {getUniqueVocationsCount=function() return vocation end, getMemberCount=function() return size-1 end}
  local expected = math.ceil(1000 * (vocation==2 and 1.35 or 1.70) / size)
  assert(env.Party.onShareExperience(party,1000)==expected)
 end
end
for _, name in ipairs({'ultimate_healing_rune','intense_healing_rune'}) do
 local casts, rune = 0
 local e = setmetatable({Player=function(p) return p end}, {__index=function(_,k)
  if _G[k]~=nil then return _G[k] end
  if k:match('^COMBAT_') or k:match('^CONST_') or k:match('^CONDITION_') or k:match('^CALLBACK_') or k:match('^SOUND_') then return k end
 end})
 e.Combat=function() return {setParameter=function()end,setCallback=function()end,execute=function() casts=casts+1;return true end} end
 e.Spell=function() rune=setmetatable({}, {__index=function()return function()end end});return rune end
 setfenv(assert(loadfile('data/scripts/runes/'..name..'.lua')),e)()
 local player={getId=function()return 100 end,getVocation=function()return {getName=function()return 'elder druid'end}end,
  sendCancelMessage=function()end,getPosition=function()return {sendMagicEffect=function()end}end}
 assert(rune.onCastSpell(player,{getNumber=function()return 101 end})==false)
 assert(casts==0,'other-target rejection must precede combat')
 assert(rune.onCastSpell(player,{getNumber=function()return 100 end})==true)
 assert(casts==1)
end
print('PASS: two/three-vocation party bonuses and self-only healing runes')

local commonBounds
for _,name in ipairs({'avalanche','great_fireball','thunderstorm','stone_shower','explosion'}) do
 local combat
 local e=setmetatable({Creature={},Player={}}, {__index=function(_,k) return _G[k] or k end})
 e.createCombatArea=function(area) return area end
 e.Combat=function()
  combat={setParameter=function() end,setArea=function(self,area) self.area=area end,
   setCallback=function(self,_,callback) self.formula=e[callback] end}
  return combat
 end
 e.Spell=function() return setmetatable({}, {__index=function()return function()end end}) end
 setfenv(assert(loadfile('data/scripts/lib/register_spells.lua')),e)()
 setfenv(assert(loadfile('data/scripts/runes/'..name..'.lua')),e)()
 if name=='explosion' then
  local count=0
  for _,row in ipairs(combat.area) do for _,cell in ipairs(row) do if cell==1 or cell==3 then count=count+1 end end end
  assert(count==9,'Explosion must hit all nine squares')
 else
  local bounds={}
  for _,case in ipairs({{100,80},{2000,150}}) do
   local lo,hi=combat.formula({},case[1],case[2]);bounds[#bounds+1]=lo;bounds[#bounds+1]=hi
  end
  if not commonBounds then commonBounds=bounds else
   for i,value in ipairs(bounds) do assert(value==commonBounds[i],name..' must match 50-power runes') end
  end
 end
end
print('PASS: equal AoE rune power at ordinary/high levels and nine-square Explosion')

-- Concrete compensation examples, including both locally chosen rounding directions.
local itemFile=assert(io.open('data/items/items.xml'))
local itemXml=itemFile:read('*a');itemFile:close()
for id,defense in pairs({[3059]=22,[3412]=18,[3414]=48,[14769]=30}) do
 local block=assert(itemXml:match('<item id="'..id..'".-</item>'))
 assert(tonumber(block:match('<attribute key="defense" value="(%d+)"'))==defense,
  'shield/spellbook compensation: '..id)
end
print('PASS: shield/spellbook defense compensation and integer rounding')
