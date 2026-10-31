-- Run with LuaJIT from the server root. Execute the real potion action.
local registry, mana, removed, flasks = {}, nil, 0, 0
local position = {sendMagicEffect=function() end, sendSingleSoundEffect=function() end}
local env = setmetatable({
  VOCATION={BASE_ID={SORCERER=1,DRUID=2,PALADIN=3,KNIGHT=4,MONK=5}},
  configKeys={EX_ACTIONS_DELAY_INTERVAL=1,REMOVE_POTION_CHARGES=2},
  configManager={getNumber=function() return 1000 end,getBoolean=function() return true end},
  CONTAINER_POSITION=65535,
  table=setmetatable({contains=function(t,v) for _,x in pairs(t) do if x==v then return true end end return false end},{__index=table}),
  Condition=function() return {setParameter=function() end} end,
  Combat=function() return {setParameter=function() end} end,
  Action=function()
    local a={}
    function a:id(id) registry[id]=self end
    function a:register() end
    return a
  end,
  doTargetCombatMana=function(_,target,lo,hi) mana={target,lo,hi} end,
}, {__index=function(_,k) return _G[k] or k end})
setfenv(assert(loadfile('data/scripts/actions/items/potions.lua')),env)()
local function use(id,level,vocation)
  mana,removed,flasks=nil,0,0
  local player={
    getGroup=function() return {getAccess=function() return false end} end,
    getLevel=function() return level end,
    getVocation=function() return {getBaseId=function() return vocation end} end,
    getPosition=function() return position end,
    isInGhostMode=function() return false end,
    say=function() end,addAchievementProgress=function() end,updateSupplyTracker=function() end,
    kv=function() return {get=function() return false end} end,
    addItem=function(_,id,count) assert(id==284 and count==1);flasks=flasks+1 end,
  }
  local item={getId=function() return id end,remove=function(_,count) removed=removed+count end}
  assert(registry[id].onUse(player,item,{x=65535},player,position,false))
  return mana,removed,flasks
end
for _, spec in ipairs({{238,80,150,250},{53162,100,240,360},{53163,130,240,360},{53164,200,425,575}}) do
  for vocation=0,5 do
    local eligible=spec[1]~=53162 or (vocation~=0 and vocation~=4)
    for _,level in ipairs({spec[2]-1,spec[2],2000}) do
      local result,charges,bottles=use(spec[1],level,vocation)
      if eligible and level>=spec[2] then
        assert(result and result[2]==spec[3] and result[3]==spec[4])
        assert(charges==1 and bottles==1)
      else assert(not result and charges==0 and bottles==0) end
    end
  end
end
-- Existing ultimate mana remains restricted, with its original restoration.
local result=use(23373,130,2);assert(result[2]==425 and result[3]==575)
assert(not use(23373,2000,4))
setfenv(assert(loadfile('data/scripts/actions/objects/cask_and_kegs.lua')),env)()
for _,id in ipairs({25891,25910}) do
  local supplied,used,emptied=0,0,0
  local player={
    getItemCount=function(_,flask) return flask==283 and 1 or 0 end,
    removeItem=function(_,flask,count) assert(flask==283);used=used+count end,
    addItem=function(_,potion,count) assert(potion==238);supplied=supplied+count end,
    sendTextMessage=function() end,
  } -- No vocation gate: both refill sources are usable by every vocation.
  local item={getId=function() return id end,getAttribute=function() return 1 end,remove=function() emptied=emptied+1 end}
  assert(registry[id].onUse(player,item))
  assert(supplied==1 and used==1 and emptied==1)
end
print('PASS: potion vocation/level boundaries, exact mana ranges, flask and charge behavior')
