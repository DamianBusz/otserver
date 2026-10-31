-- LuaJIT tests/lua/test_paladin_spells.lua ../otclient
local function read(path)
    local f=assert(io.open(path)); local s=f:read('*a'); f:close(); return s
end
local function loadSpell(path)
    local env=setmetatable({Creature={},Player={}}, {__index=_G})
    for _, header in ipairs({'src/utils/utils_definitions.hpp','src/creatures/creatures_definitions.hpp'}) do
        for symbol in read(header):gmatch('([A-Z][A-Z_0-9]+)%s*[=,]') do env[symbol]=symbol end
    end
    local attributes=assert(read('src/utils/utils_definitions.hpp'):match('enum class AttrSubId_t%s*{(.-)}'))
    for name in attributes:gmatch('(%w+)%s*,') do env['AttrSubId_'..name]='AttrSubId_'..name end
    local function load(file) setfenv(assert(loadfile(file)),env)() end
    load('data/scripts/lib/register_spells.lua')
    load('data/scripts/lib/register_paladin_spells.lua')
    env.createCombatArea=function(area) assert(area);return area end
    local combats, spells={},{}
    env.Combat=function()
        local c={params={},callbacks={},conditions={},result=true}
        function c:setParameter(k,v) assert(k and v~=nil,'undefined parameter');self.params[k]=v end
        function c:setCallback(k,name) assert(k and env[name]);self.callbacks[k]=env[name] end
        function c:setArea(a) self.area=a end
        function c:addCondition(a) self.conditions[#self.conditions+1]=a end
        function c:execute(player,variant) self.lastVariant=variant;return self.result end
        combats[#combats+1]=c;return c
    end
    env.Condition=function(kind)
        assert(kind); local c={kind=kind,params={}}
        function c:setParameter(k,v) assert(k and v~=nil,'undefined condition parameter');self.params[k]=v end
        function c:setFormula(...) self.formula={...} end
        return c
    end
    env.Spell=function()
        local s={settings={}}
        for _,k in ipairs({'name','words','id','group','level','mana','range','blockWalls','isPremium','isSelfTarget','isAggressive','cooldown','groupCooldown','vocation','needLearn','castSound'}) do
            s[k]=function(_,...) s.settings[k]={...} end
        end
        function s:register() spells[#spells+1]=self end
        return s
    end
    load('data/scripts/spells/'..path..'.lua')
    assert(#spells==1)
    return spells[1],combats[1],env
end
local client=setmetatable({}, {__index=_G})
setfenv(assert(loadfile(assert(arg[1])..'/modules/gamelib/spells.lua')),client)()
local player={skill=130,stage=1,conditions={}}
function player:getPlayer() return self end
function player:getEffectiveSkillLevel(k) assert(k=='SKILL_DISTANCE');return self.skill end
function player:revelationStageWOD() return self.stage end
function player:getSummons() return {} end
function player:addCondition(c) self.conditions[#self.conditions+1]=c end
local loaded={}
for _,path in ipairs({'attack/divine_barrage','attack/ethereal_barrage','attack/divine_grenade','attack/divine_caldera','healing/salvation','support/swift_foot'}) do
    local s,c,e=loadSpell(path);loaded[path]={s,c,e}
    local info=assert(client.SpellInfo.Default[s.settings.name[1]])
    for a,b in pairs({id='id',words='words',level='level',mana='mana',cooldown='exhaustion',isPremium='premium'}) do assert(s.settings[a][1]==info[b],path..': '..b) end
    local groups={attack=1,healing=2,support=3,focus=7}
    for i,g in ipairs(s.settings.group) do assert(info.group[groups[g]]==s.settings.groupCooldown[i]) end
    local v={position={x=100,y=100,z=7}}
    assert(s.onCastSpell(player,v) and c.lastVariant==v)
    c.result=false
    local count=#player.conditions
    assert(not s.onCastSpell(player,v));assert(#player.conditions==count)
end
print('PASS: spell dispatch propagates failures and all changed metadata matches OTClient')
for _,path in ipairs({'attack/divine_barrage','attack/ethereal_barrage'}) do
    local s,c,e=unpack(loaded[path]);local count=0
    for _,row in ipairs(c.area) do for _,n in ipairs(row) do if n~=0 then count=count+1 end end end
    assert(count==21,'barrages use the diamond-arrow area, not the new 13-tile ammunition area')
    assert(s.settings.range[1]==7 and s.settings.blockWalls[1] and not s.settings.needLearn)
    assert(client.SpellInfo.Default[s.settings.name[1]].groundTarget)
end
local divine=loaded['attack/divine_barrage'][2]
assert(divine.params.COMBAT_PARAM_EFFECT=='CONST_ME_DIVINE_BARRAGE')
local min,max=divine.callbacks.CALLBACK_PARAM_LEVELMAGICVALUE(player,300,30)
assert(min==-234 and max==-263)
local ethereal=loaded['attack/ethereal_barrage'][2]
assert(ethereal.params.COMBAT_PARAM_EFFECT=='CONST_ME_ETHEREAL_BARRAGE')
local cb=ethereal.callbacks.CALLBACK_PARAM_LEVELMAGICVALUE
local a,b=cb(player,300,0);local c,d=cb(player,300,1000)
assert(a==-338 and b==-383 and a==c and b==d,'distance-only damage must not depend on magic level or weapon')
player.skill=171;local boosted=cb(player,300,0);assert(boosted<a)
local grenade=loaded['attack/divine_grenade'][1]
player.stage=0;assert(not grenade.onCastSpell(player,{}))
assert(grenade.settings.needLearn[1])
local e=loaded['attack/divine_barrage'][3]
for _,pair in ipairs({{0,0},{499,99},{500,100},{505,100},{506,101},{1000,183},{1100,200},{1800,300}}) do assert(e.PaladinSpellFormula.levelBonus(pair[1])==pair[2]) end
assert(player.conditions[1].params.CONDITION_PARAM_BUFF_DAMAGEDEALT==70)
assert(player.conditions[1].params.CONDITION_PARAM_SUBID=='AttrSubId_SwiftFoot')
print('PASS: official effects, 21-tile areas, distance-only scaling, level bands, Wheel gate and Swift Foot damage reduction')
