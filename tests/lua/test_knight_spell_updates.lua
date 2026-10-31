-- LuaJIT tests/lua/test_knight_spell_updates.lua ../otclient
local client=setmetatable({},{__index=_G})
setfenv(assert(loadfile(assert(arg[1])..'/modules/gamelib/spells.lua')),client)()
local function environment()
 local combats,spells={},{}
 local env=setmetatable({},{__index=function(_,key)
  if _G[key]~=nil then return _G[key] end
  if key:match('^COMBAT_') or key:match('^CALLBACK_') or key:match('^CONST_') or key:match('^SOUND_') or key:match('^AREA_') then return key end
 end})
 env.createCombatArea=function(a) return a end
 env.Combat=function()
  local c={callbacks={}};combats[#combats+1]=c
  function c:setParameter() end
  function c:setArea(a) self.area=a end
  function c:setCallback(k,name) self.callbacks[k]=assert(env[name]);env[name]=nil end
  function c:execute(creature,var) self.variant=var;return creature.success end
  return c
 end
 env.Spell=function()
  local s={settings={}};spells[#spells+1]=s
  return setmetatable(s,{__index=function(_,key) return function(_,...) s.settings[key]={...} end end})
 end
 return env,combats,spells
end
for _,row in ipairs({{'berserk','Berserk',125},{'fierce_berserk','Fierce Berserk',360},{'groundshaker','Groundshaker',200}}) do
 local env,combats,spells=environment()
 setfenv(assert(loadfile('data/scripts/spells/attack/'..row[1]..'.lua')),env)()
 assert(spells[1].settings.mana[1]==row[3])
 assert(client.SpellInfo.Default[row[2]].mana==row[3])
end
local env,combats,spells=environment()
setfenv(assert(loadfile('data/scripts/lib/register_knight_spells.lua')),env)()
setfenv(assert(loadfile('data/scripts/spells/attack/front_sweep.lua')),env)()
assert(#combats==2 and spells[1].settings.needDirection[1])
for index,c in ipairs(combats) do
 local n=0;for _,row in ipairs(c.area) do for _,cell in ipairs(row) do if cell==1 or cell==3 then n=n+1 end end end
 assert(n==(index==1 and 3 or 5))
 -- The engine anchors marker 3 at the one-tile-ahead cast position.
 assert(c.area[1][2]==3 and #c.area==(index==1 and 1 or 2))
 local north={}
 for y,row in ipairs(c.area) do for x,cell in ipairs(row) do
  if cell==1 or cell==3 then north[(x-2)..','..(y-2)]=true end
 end end
 assert(north['-1,-1'] and north['0,-1'] and north['1,-1'])
 assert(not north['0,0'] and not north['0,-2'])
 assert(not not north['-1,0']==(index==2) and not not north['1,0']==(index==2))
 local lo,hi=c.callbacks.CALLBACK_PARAM_SKILLVALUE({getLevel=function() return 2000 end},150,60,999)
 assert(lo==-880 and hi==-1250) -- flat 325; powers 60..100, scaling 9.25.
end
local creature={getPlayer=function(self) return self end,getWheelSpellAdditionalArea=function(self,name) assert(name=='Front Sweep');return self.augmented end}
for _,augmented in ipairs({false,true}) do
 creature.augmented=augmented
 for _,success in ipairs({false,true}) do
  creature.success=success
  local variant={}
  assert(spells[1].onCastSpell(creature,variant)==success)
  assert(combats[augmented and 2 or 1].variant==variant)
 end
end
env,combats,spells=environment()
setfenv(assert(loadfile('data/scripts/spells/support/chivalrous_challenge.lua')),env)()
local targets,range=combats[1].callbacks.CALLBACK_PARAM_CHAINVALUE({getPlayer=function() return nil end})
assert(targets==4 and range==7)
-- A client refresh must restore the server-selected Knight stance.
local events={}
env=setmetatable({Player={getKnightStance=true},CreatureEvent=function(name)
 local e={};function e:register() events[name]=self end;return e
end},{__index=_G})
setfenv(assert(loadfile('data/scripts/creaturescripts/player/aimed_spells.lua')),env)()
for _,stance in ipairs({'blood-rage','protector'}) do
 local sent
 local player={getKnightStance=function() return stance end,sendExtendedOpcode=function(_,opcode,value) assert(opcode==86);sent=value end}
 events.AimedSpellRequest.onExtendedOpcode(player,86,'')
 assert(sent==stance)
end
print('PASS: Knight spell costs, Front Sweep power/areas/callbacks, Challenge target count/range and stance refresh')
