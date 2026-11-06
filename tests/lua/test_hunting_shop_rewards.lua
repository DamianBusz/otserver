local action
function Action() return {id=function() end, register=function(self) action=self end} end
GameStore={CoinType={Coin=0,Transferable=1},OfferTypes={OFFER_TYPE_OUTFIT=2,OFFER_TYPE_MOUNT=3}, Categories={{huntingRewardPool="outfits",offers={
 {type=2,price=600,addon=3,name='Full A Outfit',sexId={male=600,female=601}},
 {type=2,price=600,addon=3,name='Full A Outfit',sexId={male=600,female=601}},
 {type=2,price=600,addon=3,name='Full B Outfit',sexId={male=602,female=603}},
 {type=2,price=600,addon=0,name='Base only',sexId={male=604,female=605}}
}}}}
CONST_SLOT_BACKPACK=3;INDEX_WHEREEVER=-1;FLAG_NOLIMIT=1;RETURNVALUE_NOERROR=0;MESSAGE_EVENT_ADVANCE=1
local passed=0
local function test(name,fn) fn();passed=passed+1;print('PASS '..name) end
local function setup(id)
 local p={owned={},mounts={},delivered={},messages={}}
 function p:ownsMount(id) return self.mounts[id] == true end
 function p:hasMount() error("Wearability must not be used for mount ownership") end
 function p:addMount(id) if self.failMount then return false end self.mounts[id]=true return true end
 function p:removeMount(id) self.mounts[id]=nil return true end
 function p:getOwnedOutfitAddons(id) return self.owned[id] end
 function p:hasOutfit() error('Wearability must not be used for ownership') end
 function p:addOutfit(id,addons) if id==self.failOutfit then return false end self.owned[id]=addons return true end
 function p:removeOutfit(id) self.owned[id]=nil end
 function p:removeOutfitAddon(id,addon) local mask=self.owned[id] or 0;if mask==3 then mask=3-addon elseif mask==addon then mask=0 end;self.owned[id]=mask end
 function p:sendCancelMessage(s) self.messages[#self.messages+1]=s end
 p.sendTextMessage=function(self,_,s) self.messages[#self.messages+1]=s end
 function p:getSlotItem() return {isContainer=function()return true end,addItemEx=function(_,item)
  if item.id==self.failItem then return 1 end self.delivered[item.id]=item return 0
 end} end
 local box={id=id,remaining=1}
 function box:getId() return self.id end
 function box:getTopParent() return self.foreign and {} or p end
 function box:remove() if self.failRemove or self.remaining==0 then return false end self.remaining=0 return true end
 Game={createItem=function(id,count)
  if id==p.failCreate then return nil end
  return {id=id,count=count,remove=function(self) p.delivered[self.id]=nil end}
 end}
 return p,box
end
math.random=function() return 1 end
GameStore.Categories[2]={huntingRewardPool='mounts',offers={
 {type=3,price=870,id=23,name='War Horse'}, {type=3,price=870,id=23,name='Duplicate Horse'},
 {type=3,price=870,id=114,name='Unicorn'}, {type=3,price=0,id=1,name='Free mount'},
 {type=3,price=870,coinType=99,id=2,name='Wrong currency'}, {type=3,price=870,id=0,name='Invalid'},
 {type=3,price=870,id=256,name='Overflow'}, {type=3,price=870,id=1.5,name='Fractional'}
}}
GameStore.Categories[3]={offers={
 {type=2,price=600,addon=3,name='Quest Outfit',sexId={male=610,female=611}},
 {type=3,price=870,id=3,name='Tamed Mount'}
}}
GameStore.Categories[1].offers[5]={type=2,price=0,addon=3,name='Free Outfit',sexId={male=612,female=613}}
GameStore.Categories[1].offers[6]={type=2,price=600,coinType=99,addon=3,name='Event Currency',sexId={male=614,female=615}}
package.loaded['gamestore.constants']=GameStore
-- Production's legacy global predates catalog population; only require() owns the live catalog.
GameStore={CoinType=GameStore.CoinType,OfferTypes=GameStore.OfferTypes}
dofile('data/scripts/actions/items/hunting_shop_rewards.lua')
test('one random potion per box, all six outcomes and no double consumption',function()
 for choice=1,6 do
  math.random=function(a,b)assert(a==1 and b==6)return choice end
  local p,box=setup(36827);assert(action.onUse(p,box));assert(box.remaining==0)
  local count=0;for id,item in pairs(p.delivered)do count=count+1;assert(id==36722+choice and item.count==1)end
  assert(count==1)
  p.delivered={};assert(action.onUse(p,box));assert(next(p.delivered)==nil)
 end
 math.random=function()return 1 end
end)
test('failed potion insert keeps box and grants nothing',function()
 local p,box=setup(36827);p.failItem=36723;action.onUse(p,box);assert(next(p.delivered)==nil and box.remaining==1)
end)
test('failed potion creation keeps box',function()
 local p,box=setup(36827);p.failCreate=36723;action.onUse(p,box);assert(next(p.delivered)==nil and box.remaining==1)
end)
test('no backpack keeps potion box',function()
 local p,box=setup(36827);p.getSlotItem=function()return nil end;action.onUse(p,box);assert(box.remaining==1 and next(p.delivered)==nil)
end)
test('failed box consumption rolls back delivery',function()
 local p,box=setup(36827);box.failRemove=true;action.onUse(p,box);assert(next(p.delivered)==nil)
end)
test('pool deduplicates full offers and excludes fully owned pairs',function()
 local p=setup(60001);assert(#HuntingShopRewards.outfitPool(p)==2);p.owned[600]=3;p.owned[601]=3;assert(#HuntingShopRewards.outfitPool(p)==1)
end)
test('bag unlocks both full appearances exactly once',function()
 local p,bag=setup(60001);action.onUse(p,bag);assert(p.owned[600]==3 and p.owned[601]==3 and bag.remaining==0)
 action.onUse(p,bag);assert(p.owned[602]==nil and p.owned[603]==nil)
end)
test('all outfits owned keeps bag',function()
 local p,bag=setup(60001);for id=600,603 do p.owned[id]=3 end;action.onUse(p,bag);assert(bag.remaining==1)
end)
test('partial outfit ownership is completed without duplicates',function()
 local p,bag=setup(60001);p.owned[600]=1;p.owned[601]=2;action.onUse(p,bag);assert(p.owned[600]==3 and p.owned[601]==3)
end)
test('grant failure restores preexisting addons and keeps bag',function()
 local p,bag=setup(60001);p.owned[600]=1;p.owned[601]=2;p.failOutfit=601;action.onUse(p,bag);assert(p.owned[600]==1 and p.owned[601]==2 and bag.remaining==1)
end)
test('consume failure restores original ownership',function()
 local p,bag=setup(60001);p.owned[600]=1;bag.failRemove=true;action.onUse(p,bag);assert(p.owned[600]==1 and p.owned[601]==nil)
end)
test('foreign or ground reward cannot be consumed',function()
 for _,id in ipairs({60001,60004,36827}) do local p,item=setup(id);item.foreign=true;action.onUse(p,item);assert(item.remaining==1 and next(p.owned)==nil and next(p.delivered)==nil) end
end)
test('mount pool uses only paid premium catalog, deduplicates and skips owned',function()
 local p=setup(60004);local pool=HuntingShopRewards.mountPool(p);assert(#pool==2 and pool[1].id==23 and pool[2].id==114)
 p.mounts[23]=true;pool=HuntingShopRewards.mountPool(p);assert(#pool==1 and pool[1].id==114)
end)
test('mount bag unlocks one unowned mount and consumes once',function()
 local p,bag=setup(60004);action.onUse(p,bag);assert(p.mounts[23] and not p.mounts[114] and bag.remaining==0)
 action.onUse(p,bag);assert(not p.mounts[114])
end)
test('mount grant failure keeps bag',function()
 local p,bag=setup(60004);p.failMount=true;action.onUse(p,bag);assert(next(p.mounts)==nil and bag.remaining==1)
end)
test('mount consume failure rolls back only newly selected mount',function()
 local p,bag=setup(60004);p.mounts[23]=true;bag.failRemove=true;action.onUse(p,bag);assert(p.mounts[23] and not p.mounts[114] and bag.remaining==1)
end)
test('all premium mounts owned keeps bag',function()
 local p,bag=setup(60004);p.mounts[23]=true;p.mounts[114]=true;action.onUse(p,bag);assert(bag.remaining==1)
end)

test('purchased bags enter backpack, unlock immediately and leave no bag',function()
 for _,id in ipairs({60001,60004}) do
  local p,bag=setup(id);local inserted=false
  function p:getSlotItem(slot) assert(slot==CONST_SLOT_BACKPACK);return {isContainer=function()return true end,addItemEx=function(_,item,_,flags)assert(item==bag and flags==0);inserted=true;return 0 end} end
  Game.createItem=function(itemId)assert(itemId==id);return bag end
  assert(HuntingShopRewards.purchaseBag(p,id));assert(inserted and bag.remaining==0)
  if id==60001 then assert(p.owned[600]==3 and p.owned[601]==3) else assert(p.mounts[23]) end
 end
end)
test('automatic bags reject absent or full backpack without granting',function()
 for _,id in ipairs({60001,60004}) do
  local p,bag=setup(id)
  function p:getSlotItem()return nil end
  assert(not HuntingShopRewards.purchaseBag(p,id))
  p.getSlotItem=function() return {isContainer=function()return true end,addItemEx=function()return 1 end} end
  Game.createItem=function()return bag end
  assert(not HuntingShopRewards.purchaseBag(p,id));assert(bag.remaining==0 and next(p.owned)==nil and next(p.mounts)==nil)
 end
end)
test('automatic bags roll back grants if consumption fails',function()
 for _,id in ipairs({60001,60004}) do
  local p,bag=setup(id);bag.failRemove=true
  p.getSlotItem=function() return {isContainer=function()return true end,addItemEx=function()return 0 end} end
  Game.createItem=function()return bag end
  assert(not HuntingShopRewards.purchaseBag(p,id));assert(next(p.owned)==nil and next(p.mounts)==nil)
 end
end)
test('automatic bags reject exhausted pools before creating an item',function()
 local p=setup(60001);for id=600,603 do p.owned[id]=3 end;p.mounts[23]=true;p.mounts[114]=true
 Game.createItem=function()error('No item should be created')end
 assert(not HuntingShopRewards.purchaseBag(p,60001));assert(not HuntingShopRewards.purchaseBag(p,60004))
end)
test('legacy repair preserves items and contents, leaves unrelated Store items alone',function()
 local p=setup(60001)
 local pack={isContainer=function()return true end}
 p.getSlotItem=function()return pack end
 p.getGuid=function()return 193 end
 local function item(id,owner)
  return {id=id,owner=owner,store=1234,contents={42},getId=function(self)return self.id end,
   getOwnerId=function(self)return self.owner end,getAttribute=function(self)return self.store end,
   removeAttribute=function(self,key)if key=='owner'then self.owner=nil else self.store=0 end end,
   moveTo=function(self,target,flags)assert(flags==0);self.destination=target;return true end}
 end
 local old=item(60002,193);local other=item(2854,193);local foreign=item(53695,194)
 p.getStoreInbox=function()return {getItems=function()return {old,other,foreign}end}end
 assert(HuntingShopRewards.recoverLegacyItems(p)==1)
 assert(old.destination==pack and old.contents[1]==42 and old.owner==nil and old.store==0)
 assert(other.store==1234 and foreign.store==1234 and not other.destination and not foreign.destination)
 assert(HuntingShopRewards.recoverLegacyItems(p)==0)
end)
test('legacy repair leaves overflow accessible without creating replacements',function()
 local p=setup(60001);p.getGuid=function()return 193 end
 local old={owner=193,store=1234,getId=function()return 53695 end,
  getOwnerId=function(self)return self.owner end,getAttribute=function(self)return self.store end,
  removeAttribute=function(self,key)if key=='owner'then self.owner=nil else self.store=0 end end,
  moveTo=function()return false end}
 p.getStoreInbox=function()return {getItems=function()return {old}end}end
 Game.createItem=function()error('Recovery must never create items')end
 assert(HuntingShopRewards.recoverLegacyItems(p)==1 and old.store==0 and old.owner==nil)
 assert(HuntingShopRewards.recoverLegacyItems(p)==0)
end)

print(passed .. ' hunting reward checks passed')
