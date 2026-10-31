-- LuaJIT tests/lua/test_druid_spells.lua ../otclient
local constants = {}
local f = assert(io.open('src/utils/utils_definitions.hpp'))
for name, value in f:read('*a'):gmatch('(CONST_ME_[A-Z_]+)%s*=%s*(%d+)') do constants[name] = tonumber(value) end
f:close()
local specs = {
 { 'strong_ice_strike', 'Strong Ice Strike', 152, 115, 115, 8000, 7 },
 { 'strong_terra_strike', 'Strong Terra Strike', 153, 115, 115, 8000, 7 },
 { 'ultimate_ice_strike', 'Ultimate Ice Strike', 156, 195, 195, 30000, 7 },
 { 'ultimate_terra_strike', 'Ultimate Terra Strike', 157, 195, 195, 30000, 7 },
 { 'strong_ice_wave', 'Strong Ice Wave', 43, 100, 180, 4000 },
 { 'wrath_of_nature', 'Wrath of Nature', 56, 140, 210, 40000 },
 { 'forked_glacier', 'Forked Glacier', 317, 88, 92, 6000, 7, 324 },
 { 'forked_thorns', 'Forked Thorns', 318, 95, 99, 6000, 7, 325 },
}
local client
if arg[1] then
 client = setmetatable({}, {__index = _G})
 setfenv(assert(loadfile(arg[1] .. '/modules/gamelib/spells.lua')), client)()
end
for _, spec in ipairs(specs) do
 local combats, spells = {}, {}
 local env = setmetatable({}, {__index = function(_, key)
  if _G[key] ~= nil then return _G[key] end
  if constants[key] then return constants[key] end
  if key:match('^COMBAT_') or key:match('^CALLBACK_') or key:match('^SOUND_') or key:match('^CONST_ANI') then return key end
 end})
 env.Creature = {}
 env.Player = {}
 env.createCombatArea = function(area) assert(type(area) == 'table'); return area end
 env.Combat = function()
  local c = { parameters = {} }; combats[#combats + 1] = c
  function c:setParameter(k, v) assert(v ~= nil, k); self.parameters[k] = v end
  function c:setCallback(k, name)
   self.formula = assert(rawget(env, name), 'missing callback: ' .. name)
   env[name] = nil -- LuaScriptInterface::getEvent consumes the named global.
  end
  function c:setArea(area) self.area = area end
  function c:execute(creature, variant) self.variant = variant; return creature.success end
  return c
 end
 env.Spell = function()
  local s = {settings={}}; spells[#spells + 1] = s
  return setmetatable(s, {__index = function(_, k) return function(_, ...) s.settings[k]={...} end end})
 end
 setfenv(assert(loadfile('data/scripts/lib/register_spells.lua')), env)()
 setfenv(assert(loadfile('data/scripts/lib/register_druid_spells.lua')), env)()
 setfenv(assert(loadfile('data/scripts/spells/attack/' .. spec[1] .. '.lua')), env)()
 local spell = assert(spells[1]); local settings = spell.settings
 assert(settings.name[1] == spec[2])
 assert(settings.id[1] == spec[3], spec[2] .. ' id')
 assert(settings.cooldown[1] == spec[6], spec[2] .. ' cooldown')
 if spec[7] then assert(settings.range[1] == spec[7]) end
 if spec[8] then assert(combats[1].parameters.COMBAT_PARAM_EFFECT == spec[8]) end
 for _, combat in ipairs(combats) do
  for _, case in ipairs({{100, 80, 20}, {500, 100, 100}, {1100, 120, 200}, {2000, 150, 325}}) do
   local lo, hi = combat.formula({}, case[1], case[2])
   assert(lo == -(case[3] + math.floor(spec[4] * (case[2]/25 + .25))), spec[2] .. ' low bound')
   assert(hi == -(case[3] + math.floor(spec[5] * (case[2]/25 + .25))), spec[2] .. ' high bound')
  end
 end
 local variant={}
 local function creature(success) return {success=success, getPlayer=function() return nil end} end
 assert(spell.onCastSpell(creature(false), variant) == false, spec[2] .. ' must propagate failure')
 assert(spell.onCastSpell(creature(true), variant) == true)
 assert(combats[1].variant == variant)
 if spec[1] == 'strong_ice_wave' then
  local augmentedCaster = {getPlayer=function() return {getWheelSpellAdditionalArea=function() return true end} end}
  for _, success in ipairs({false, true}) do
   augmentedCaster.success = success
   assert(spell.onCastSpell(augmentedCaster, variant) == success)
   assert(combats[2].variant == variant)
  end
  local count, centers = 0, 0
  for _, row in ipairs(combats[1].area) do for _, cell in ipairs(row) do
   if cell == 1 or cell == 3 then count = count+1 end
   if cell == 3 then centers = centers+1 end
  end end
  assert(count==10 and centers==1, 'official Strong Ice Wave footprint')
  assert(#combats[1].area==4 and combats[1].area[4][2]==3, 'directional anchor must be the affected tile one step ahead')
 end
 if client then
  local info=assert(client.SpellInfo.Default[spec[2]])
  for sk, ck in pairs({id='id',words='words',level='level',mana='mana',cooldown='exhaustion'}) do
   assert(settings[sk][1] == info[ck],spec[2] .. ' client ' .. ck)
  end
  if spec[7] then assert(info.range==spec[7],spec[2] .. ' client range') end
 end
end
print('PASS: druid attack formulas at ordinary/high levels, metadata, official wave footprint and failed-cast propagation')
