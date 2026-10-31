-- LuaJIT tests/lua/test_knight_healing.lua ../otclient
-- The numerical expectations pin the documented local calibration, not official samples.
local client = setmetatable({}, {__index = _G})
setfenv(assert(loadfile(assert(arg[1]) .. '/modules/gamelib/spells.lua')), client)()
for _, spec in ipairs({
 {'bruise_bane', 'Bruise Bane', 175, 1, 10, 2000, false, 'exura infir ico', 426, 477},
 {'wound_cleansing', 'Wound Cleansing', 123, 8, 60, 2000, false, 'exura ico', 798, 1035},
 {'fair_wound_cleansing', 'Fair Wound Cleansing', 239, 300, 135, 2000, true, 'exura med ico', 1846, 2607},
 {'intense_wound_cleansing', 'Intense Wound Cleansing', 158, 80, 300, 120000, true, 'exura gran ico', 3705, 5395},
}) do
 local combat, spell
 local env = setmetatable({}, {__index = function(_, key)
  if _G[key] ~= nil then return _G[key] end
  if key:match('^COMBAT_') or key:match('^CALLBACK_') or key:match('^CONST_')
    or key:match('^CONDITION_') or key:match('^SOUND_') or key == 'SKILL_SHIELD' then return key end
 end})
 env.Combat = function()
  combat = {params = {}}
  function combat:setParameter(key, value) self.params[key] = value end
  function combat:setCallback(_, name) self.formula = assert(env[name]); env[name] = nil end
  function combat:execute(creature, variant) self.variant = variant; return creature.success end
  return combat
 end
 env.Spell = function()
  spell = {settings = {}}
  return setmetatable(spell, {__index = function(_, key)
   return function(_, ...) spell.settings[key] = {...} end
  end})
 end
 setfenv(assert(loadfile('data/scripts/lib/register_knight_spells.lua')), env)()
 setfenv(assert(loadfile('data/scripts/spells/healing/' .. spec[1] .. '.lua')), env)()
 local s, c = spell.settings, assert(client.SpellInfo.Default[spec[2]])
 assert(s.id[1] == spec[3] and c.id == spec[3])
 assert(s.level[1] == spec[4] and c.level == spec[4])
 assert(s.mana[1] == spec[5] and c.mana == spec[5])
 assert(s.cooldown[1] == spec[6] and c.exhaustion == spec[6])
 assert((s.isPremium and s.isPremium[1] or false) == spec[7] and c.premium == spec[7])
 assert(s.words[1] == spec[8] and c.words == spec[8])
 assert(s.group[1] == 'healing' and s.groupCooldown[1] == 2000 and c.group[2] == 2000)
 assert(s.vocation[1] == 'knight;true' and s.vocation[2] == 'elite knight;true')
 assert(c.vocations[1] == 4 and c.vocations[2] == 8 and #c.vocations == 2)
 assert(s.isSelfTarget[1] and not s.isAggressive[1])
 assert(combat.params.COMBAT_PARAM_TYPE == 'COMBAT_HEALING')
 assert(combat.params.COMBAT_PARAM_DISPEL == 'CONDITION_PARALYZE')
 assert(combat.params.COMBAT_PARAM_EFFECT == 'CONST_ME_MAGIC_BLUE')
 assert(combat.params.COMBAT_PARAM_AGGRESSIVE == false)
 local player = {shielding = 130}
 function player:getEffectiveSkillLevel(skill) assert(skill == 'SKILL_SHIELD'); return self.shielding end
 function player:getSkillLevel() error('healing must use effective shielding') end
 function player:getShieldSpellDefense() error('healing must not require shield equipment') end
 local lo, hi = combat.formula(player, 2000, 15)
 assert(lo == spec[9] and hi == spec[10], spec[2])
 player.shielding = 169 -- Protector applies 30% to the effective shielding skill.
 local protectedLo, protectedHi = combat.formula(player, 2000, 15)
 assert(protectedLo > lo and protectedHi > hi)
 player.shielding = 130
 local magicLo, magicHi = combat.formula(player, 2000, 20)
 assert(magicLo > lo and magicHi > hi)
 if spec[3] == 239 then
  assert(protectedLo == 2127 and protectedHi == 3028)
  assert(magicLo == 2026 and magicHi == 2877)
 end
 for _, success in ipairs({false, true}) do
  local variant = {}
  assert(spell.onCastSpell({success = success}, variant) == success)
  assert(combat.variant == variant)
 end
end
local bruise = client.SpellInfo.Default['Bruise Bane']
assert(bruise.clientId == 175)
assert(client.Spells.getImageClip(bruise.clientId) == '4192 0 32 32')
assert(client.Spells.getImageClipCooldown(bruise.clientId) == '2620 0 20 22')
assert(client.Spells.getIconFileByClientId(bruise.clientId):match('rebalance%-icons%-32x32$'))
assert(client.Spells.getIconFileByClientId(bruise.clientId, nil, true):match('rebalance%-icons%-20x20$'))
print('PASS: four Knight heal formulas, effective shielding/magic scaling, registrations, client parity and cast results')
