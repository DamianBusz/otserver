CORE_DIRECTORY='data'
SERVER_NAME='Hunting inventory regression'
dofile('data/libs/functions/player.lua')
HIRELING_SKILLS=setmetatable({}, {__index=function()return {1}end})
HIRELING_OUTFITS=HIRELING_SKILLS
dofile('data/modules/scripts/gamestore/gamestore.lua')
assert(GameStore.Categories == nil, 'Exercise the stale compatibility global')
assert(#require('gamestore.constants').Categories > 0)
-- Action registration is irrelevant here; all inventory and unlock methods are native.
Action=function()return {id=function()end,register=function(self) huntingRewardAction=self end}end
dofile('data/scripts/actions/items/hunting_shop_rewards.lua')
local api={}
dofile('data/modules/scripts/taskboard/settings.lua')(api)
dofile('data/modules/scripts/taskboard/rules.lua')(api)
local state={general={},wheel={}}
assert(#HuntingShopRewards.outfitPool(buyer)==48)
assert(#HuntingShopRewards.mountPool(buyer)==140)
assert(api.rules.purchaseShopOffer(buyer,state,3), 'Outfit bag purchase')
assert(buyer:getTaskHuntingPoints()==990000)
assert(#HuntingShopRewards.outfitPool(buyer)==47)
assert(buyer:getSlotItem(CONST_SLOT_BACKPACK):getSize()==0, 'Outfit bag consumed')
assert(api.rules.purchaseShopOffer(buyer,state,4), 'Mount bag purchase')
assert(buyer:getTaskHuntingPoints()==980000)
assert(#HuntingShopRewards.mountPool(buyer)==139)
assert(buyer:getSlotItem(CONST_SLOT_BACKPACK):getSize()==0, 'Mount bag consumed')
local inbox=buyer:getStoreInbox()
local pack=buyer:getSlotItem(CONST_SLOT_BACKPACK)
assert(#api.config.shopOffers==5, 'Backpacks removed from the live catalog')
assert(inbox:getSize()==0 and pack:getSize()==0, 'Cosmetic rewards unlock without item delivery')
assert(api.rules.purchaseShopOffer(buyer,state,1), 'Orb purchase')
assert(pack:getItem(0):getId()==53695, 'Usable orb in equipped backpack')
assert(api.rules.purchaseShopOffer(buyer,state,2), 'Potion box purchase')
assert(pack:getItem(0):getId()==36827)
local box=pack:getItem(0)
assert(huntingRewardAction.onUse(buyer,box))
local potions=0
for id=36723,36728 do potions=potions+buyer:getItemCount(id) end
assert(potions==1 and buyer:getItemCount(36827)==0, 'One native potion and consumed box')
assert(buyer:getTaskHuntingPoints()==958500)
assert(api.rules.purchaseShopOffer(buyer,state,0), 'Crystal coin purchase')
assert(buyer:getTaskHuntingPoints()==958400)
local legacy=inbox:addItem(60002,1)
legacy:setOwner(buyer)
legacy:setAttribute(ITEM_ATTRIBUTE_STORE,1234)
assert(HuntingShopRewards.recoverLegacyItems(buyer)==1)
assert(inbox:getSize()==0)
assert(not legacy:getOwnerId())
assert(legacy:getAttribute(ITEM_ATTRIBUTE_STORE)==0)
assert(HuntingShopRewards.recoverLegacyItems(buyer)==0, 'Migration is idempotent')
for _,item in ipairs(pack:getItems()) do item:remove() end
buyer:setCapacity(10000000)
for i=1,28 do assert(pack:addItem(53207,1), 'Fill slot '..i) end
assert(pack:getSize()==28)
assert(not api.rules.purchaseShopOffer(buyer,state,1), 'Full backpack rejects orb')
assert(buyer:getTaskHuntingPoints()==958400, 'Failed delivery refunds every point')
assert(inbox:getSize()==0)

for _,item in ipairs(pack:getItems()) do item:remove() end
buyer:setCapacity(0)
assert(not api.rules.purchaseShopOffer(buyer,state,2), 'Insufficient carrying capacity rejects potion box')
assert(buyer:getTaskHuntingPoints()==958400 and pack:getSize()==0, 'No charge or item when overweight')
