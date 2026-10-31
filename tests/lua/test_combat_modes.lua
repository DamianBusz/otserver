-- LuaJIT tests/lua/test_combat_modes.lua [inventory.lua]
local source = arg[1] or '../otclient/modules/game_inventory/inventory.lua'
local function noop() end
local function widget()
 return setmetatable({}, {__index=function(t,k)
  if k=='show' then return function(self) self.visible=true end end
  if k=='hide' then return function(self) self.visible=false end end
  if k:match('^set') then return function(self,v) self[k:sub(4)]=v end end
  local child=widget();rawset(t,k,child);return child
 end})
end
for _,modern in ipairs({false,true}) do
 for _,compact in ipairs({false,true}) do
  local calls,mode={},2
  local saved={Fixture={fightMode=2,chaseMode=0,safeFight=true}}
  local player={getSoul=function()return 0 end,getFreeCapacity=function()return 0 end,
   getInventoryItem=function()return nil end,isMonk=function()return false end}
  local env=setmetatable({},{__index=_G})
  for i,name in ipairs({'Head','Neck','Back','Body','Right','Left','Leg','Feet','Finger','Ammo'}) do env['InventorySlot'..name]=i end
  env.InventorySlotFirst=1;env.InventorySlotPurse=11
  env.FightOffensive=1;env.FightBalanced=2;env.FightDefensive=3
  env.ChaseOpponent=1;env.DontChase=0
  env.GameTacticsWithoutFightMode='modes';env.GamePVPMode='pvp'
  env.GameVocationMonk='monk';env.GameThingClock='clock';env.GameThingCounter='counter';env.GamePurseSlot='purse'
  env.g_game={getFeature=function(k)return k=='modes' and modern end,
   getLocalPlayer=function()return player end,getCharacterName=function()return 'Fixture' end,
   getFightMode=function()return mode end,setFightMode=function(v)mode=v;calls[#calls+1]=v end,
   getChaseMode=function()return 0 end,setChaseMode=noop,setSafeFight=noop,isSafeFight=function()return true end,
   isOnline=function()return false end,isAttacking=function()return false end,getClientVersion=function()return 1525 end}
  env.g_settings={getNode=function()return saved end,setNode=function(_,v)saved=v end,getBoolean=function()return compact end}
  env.modules={game_interface={getMainRightPanel=function()return {} end},client_options={getOption=function()return false end},game_mainpanel={reloadMainPanelSizes=noop}}
  env.ItemsDatabase={setTier=noop}
  env.table=setmetatable({empty=function(t)return next(t)==nil end},{__index=table})
  env.Controller={new=function()return {ui=widget(),setUI=noop,registerEvents=function()return {execute=noop}end}end}
  setfenv(assert(loadfile(source)),env)()
  env.inventoryController:onGameStart()
  assert(#calls==1 and calls[1]==(modern and 1 or 2),'saved mode restoration')
  for _,panel in ipairs({env.inventoryController.ui.onPanel,env.inventoryController.ui.offPanel}) do
   for _,name in ipairs({'attack','balanced','defense'}) do
    assert(panel[name].Visible==not modern,'control visibility')
   end
  end
  env.selectCombat('defense')
  assert(#calls==(modern and 1 or 2),'removed controls cannot send mode requests')
  assert(mode==(modern and 1 or 3))
  env.inventoryController:onGameEnd()
  if modern then
   assert(saved.Fixture.fightMode==nil,'removed modes must not persist')
  else
   assert(saved.Fixture.fightMode==3,'legacy mode persists')
  end
  assert(saved.Fixture.chaseMode==0 and saved.Fixture.safeFight==true,'other tactics preserved')
 end
end
print('PASS: modern mode removal and legacy restoration in both inventory layouts')
