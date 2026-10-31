-- LuaJIT tests/lua/test_knight_shield_spells.lua ../otclient
local client
if arg[1] then
 client = setmetatable({}, {__index=_G})
 setfenv(assert(loadfile(arg[1] .. '/modules/gamelib/spells.lua')), client)()
end
for _, spec in ipairs({
 {'shield_bash', 'Shield Bash', 315, 18, 30, 4000, 55, 'exori ico scu'},
 {'shield_slam', 'Shield Slam', 316, 30, 110, 6000, 52, 'exori scu'},
}) do
 local combat, spell
 local env = setmetatable({}, {__index=function(_, key)
  if _G[key] ~= nil then return _G[key] end
  if key:match('^COMBAT_') or key:match('^CALLBACK_') or key:match('^CONST_ME_') or key=='SKILL_SHIELD' then return key end
 end})
 env.createCombatArea = function(area) return area end
 env.Combat = function()
  combat = {params={}, executions=0}
  function combat:setParameter(k,v) assert(v~=nil); self.params[k]=v end
  function combat:setArea(area) self.area=area end
  function combat:setCallback(_, name) self.formula=assert(env[name]); env[name]=nil end
  function combat:execute(creature,var) self.executions=self.executions+1; self.var=var; return creature.success end
  return combat
 end
 env.Spell = function()
  spell = {settings={}}
  return setmetatable(spell,{__index=function(_, key) return function(_, ...) spell.settings[key]={...} end end})
 end
 setfenv(assert(loadfile('data/scripts/lib/register_knight_spells.lua')), env)()
 setfenv(assert(loadfile('data/scripts/spells/attack/'..spec[1]..'.lua')), env)()
 local s=spell.settings
 assert(s.name[1]==spec[2] and s.id[1]==spec[3])
 assert(s.level[1]==spec[4] and s.mana[1]==spec[5] and s.cooldown[1]==spec[6])
 assert(s.group[1]=='attack' and s.groupCooldown[1]==2000 and s.words[1]==spec[8])
 assert(s.vocation[1]=='knight;true' and s.vocation[2]=='elite knight;true' and s.isPremium[1])
 assert(combat.params.COMBAT_PARAM_NEXTATTACKREDUCTION==50)
 assert(combat.params.COMBAT_PARAM_EFFECT=='CONST_ME_SHIELD_BASH')
 assert(combat.params.COMBAT_PARAM_TYPE=='COMBAT_PHYSICALDAMAGE')
 assert(not s.needWeapon and not combat.params.COMBAT_PARAM_USECHARGES)
 if spec[3]==315 then
  assert(s.range[1]==1 and s.needTarget[1] and s.blockWalls[1] and not combat.area)
 else
  local ring,center=0,0
  for _, row in ipairs(combat.area) do for _, cell in ipairs(row) do
   if cell==1 then ring=ring+1 elseif cell==3 then center=center+1 end
  end end
  assert(ring==8 and center==1 and not s.needTarget)
 end
 local player = {defense=58, shielding=156, success=true}
 function player:getPlayer() return self end
 function player:getShieldSpellDefense() return self.defense end
 function player:getEffectiveSkillLevel(skill) assert(skill=='SKILL_SHIELD'); return self.shielding end
 function player:getSkillLevel() error('base shielding must not be used') end
 function player:sendCancelMessage(message) self.message=message end
 function player:getPosition() return {sendMagicEffect=function(_, effect) assert(effect=='CONST_ME_POFF') end} end
 local lo,hi=combat.formula(player,2000,9999)
 -- Independent level-2000 flat bonus is 325; magic level never affects a shield attack.
 assert(lo==-(325+math.floor((spec[7]-2)*9.298)))
 assert(hi==-(325+math.floor((spec[7]+2)*9.298)))
 player.shielding=120
 local baseLo=combat.formula(player,2000,0)
 assert(baseLo>lo, 'effective Protector shielding must increase damage')
 player.defense=nil
 assert(not spell.onCastSpell(player,{}))
 assert(player.message and combat.executions==0)
 assert(not spell.onCastSpell({getPlayer=function() return nil end},{}))
 player.defense=0 -- Zero defense is a real shield and is distinct from no shield.
 local variant={}
 for _, success in ipairs({false,true}) do
  player.success=success
  assert(spell.onCastSpell(player,variant)==success)
  assert(combat.var==variant)
 end
 if client then
  local info=assert(client.SpellInfo.Default[spec[2]])
  for sk,ck in pairs({id='id',words='words',level='level',mana='mana',cooldown='exhaustion'}) do
   assert(s[sk][1]==info[ck],spec[2]..' client '..ck)
  end
  assert(info.group[1]==2000 and info.vocations[1]==4 and info.vocations[2]==8)
  assert(client.Spells.getIconFileByClientId(spec[3]):find('rebalance%-icons'))
 end
end
-- Weapon scripts are autoattacks; default spell origin breaks suppression and splash charms.
for _, name in ipairs({'burst_arrow','diamond_arrow','poison_arrow','viper_star'}) do
 local source=assert(io.open('data/scripts/weapons/scripts/'..name..'.lua')):read('*a')
 assert(source:find('combat:setOrigin%(ORIGIN_RANGED%)'),name..' needs ranged origin')
end
print('PASS: shield spell metadata, effective shielding, formulas, shield gate, ring, failed-cast propagation and client parity')
