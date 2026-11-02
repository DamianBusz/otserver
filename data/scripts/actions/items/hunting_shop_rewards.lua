-- Hunting Task Shop consumables. Keep reward delivery and consumption atomic.
local POTION_BOX, OUTFIT_BAG, MOUNT_BAG = 36827, 60001, 60004
local POTIONS = {36723, 36724, 36725, 36726, 36727, 36728}
local rewards = {}
HuntingShopRewards = rewards

local function paidStoreOffer(category, offer, pool)
    -- Explicit catalog opt-in excludes quest, event, tournament and custom categories.
    local coins = require("gamestore.constants").CoinType
    return category.huntingRewardPool == pool and type(offer.price) == 'number' and offer.price > 0 and
        (offer.coinType == nil or offer.coinType == coins.Coin or offer.coinType == coins.Transferable)
end

function rewards.outfitPool(player)
    local GameStore = require("gamestore.constants")
    local pool, seen = {}, {}
    for _, category in ipairs(GameStore and GameStore.Categories or {}) do
        for _, offer in ipairs(category.offers or {}) do
            local ids = offer.sexId
            if paidStoreOffer(category, offer, 'outfits') and offer.type == GameStore.OfferTypes.OFFER_TYPE_OUTFIT and offer.addon == 3 and ids and
                type(ids.male) == 'number' and type(ids.female) == 'number' then
                local key = ids.male .. ':' .. ids.female
                if not seen[key] then
                    seen[key] = true
                    if player:getOwnedOutfitAddons(ids.male) ~= 3 or player:getOwnedOutfitAddons(ids.female) ~= 3 then
                        pool[#pool + 1] = {name = offer.name, male = ids.male, female = ids.female}
                    end
                end
            end
        end
    end
    return pool
end

function rewards.mountPool(player)
    local GameStore = require("gamestore.constants")
    local pool, seen = {}, {}
    for _, category in ipairs(GameStore and GameStore.Categories or {}) do
        for _, offer in ipairs(category.offers or {}) do
            if paidStoreOffer(category, offer, 'mounts') and offer.type == GameStore.OfferTypes.OFFER_TYPE_MOUNT and
                type(offer.id) == 'number' and offer.id >= 1 and offer.id <= 255 and offer.id % 1 == 0 and not seen[offer.id] then
                seen[offer.id] = true
                -- Raw ownership is independent of premium status and GM access.
                if not player:ownsMount(offer.id) then
                    pool[#pool + 1] = {name = offer.name, id = offer.id}
                end
            end
        end
    end
    return pool
end

local function openMountBag(player, bag)
    local pool = rewards.mountPool(player)
    if #pool == 0 then
        player:sendCancelMessage('You already own every premium Store mount in this bag. Your bag has been kept.')
        return false
    end
    local reward = pool[math.random(1, #pool)]
    if player:addMount(reward.id) ~= true then
        player:sendCancelMessage('Unable to unlock the mount. Your bag has been kept.')
        return false
    end
    if not bag:remove(1) then
        player:removeMount(reward.id)
        return false
    end
    player:sendTextMessage(MESSAGE_EVENT_ADVANCE, 'You unlocked the premium Store mount ' .. reward.name .. '!')
    return true
end

local function openPotionBox(player, box)
    local backpack = player:getSlotItem(CONST_SLOT_BACKPACK)
    if not backpack or not backpack:isContainer() then
        player:sendCancelMessage('Equip a backpack to receive your potion. Your box has been kept.')
        return true
    end
    local item = Game.createItem(POTIONS[math.random(1, #POTIONS)], 1)
    if not item then return true end
    if backpack:addItemEx(item, INDEX_WHEREEVER, 0) ~= RETURNVALUE_NOERROR then
        item:remove()
        player:sendCancelMessage('Make room in your backpack and check your capacity. Your box has been kept.')
        return true
    end
    if not box:remove(1) then item:remove() return true end
    player:sendTextMessage(MESSAGE_EVENT_ADVANCE, 'Your Drome Potion Box delivered one random Drome potion to your backpack.')
    return true
end

local function openOutfitBag(player, bag)
    local pool = rewards.outfitPool(player)
    if #pool == 0 then
        player:sendCancelMessage('You already own every full outfit in this bag. Your bag has been kept.')
        return false
    end
    local reward = pool[math.random(1, #pool)]
    local previous, types = {}, {reward.male}
    if reward.female ~= reward.male then types[#types + 1] = reward.female end
    for _, id in ipairs(types) do
        local mask = player:getOwnedOutfitAddons(id)
        previous[id] = {base = mask ~= nil, first = mask and mask % 2 == 1, second = mask and mask >= 2}
    end
    local function rollback()
        for id, owned in pairs(previous) do
            if not owned.base then player:removeOutfit(id)
            else
                if not owned.first then player:removeOutfitAddon(id, 1) end
                if not owned.second then player:removeOutfitAddon(id, 2) end
            end
        end
    end
    for _, id in ipairs(types) do
        if player:addOutfit(id, 3) ~= true then
            rollback()
            player:sendCancelMessage('Unable to unlock the outfit. Your bag has been kept.')
            return false
        end
    end
    if not bag:remove(1) then rollback() return false end
    player:sendTextMessage(MESSAGE_EVENT_ADVANCE, 'You unlocked ' .. reward.name .. ' with both addons for both appearances!')
    return true
end

-- Purchases use the same atomic unlock path as manually opened bags.
function rewards.isBag(id)
    return id == OUTFIT_BAG or id == MOUNT_BAG
end

function rewards.purchaseBag(player, id)
    if not rewards.isBag(id) then return false end
    local pool = id == OUTFIT_BAG and rewards.outfitPool(player) or rewards.mountPool(player)
    if #pool == 0 then
        player:sendCancelMessage('You already own every eligible premium Store reward. No points were spent.')
        return false
    end
    local backpack = player:getSlotItem(CONST_SLOT_BACKPACK)
    if not backpack or not backpack:isContainer() then
        player:sendCancelMessage('Equip a backpack to open this reward automatically.')
        return false
    end
    local bag = Game.createItem(id, 1)
    if not bag then return false end
    if backpack:addItemEx(bag, INDEX_WHEREEVER, 0) ~= RETURNVALUE_NOERROR then
        bag:remove()
        player:sendCancelMessage('Make room in your backpack for the reward bag. No points were spent.')
        return false
    end
    local unlocked
    if id == OUTFIT_BAG then unlocked = openOutfitBag(player, bag)
    else unlocked = openMountBag(player, bag) end
    if not unlocked then bag:remove() end
    return unlocked
end

-- Repair rewards issued by the original hunting shop's Store Inbox delivery.
-- These ordinary reward IDs are not Store offers; owner + Store stamp identifies
-- that old delivery path. Clear the invalid restrictions before moving anything.
function rewards.recoverLegacyItems(player)
    local inbox = player:getStoreInbox()
    if not inbox then return 0 end
    local ids = {[3043]=true,[53695]=true,[36827]=true,[60002]=true,[60003]=true}
    local backpack = player:getSlotItem(CONST_SLOT_BACKPACK)
    if backpack and not backpack:isContainer() then backpack = nil end
    local recovered = 0
    for _, item in ipairs(inbox:getItems(true)) do
        if ids[item:getId()] and item:getOwnerId() == player:getGuid() and
            (tonumber(item:getAttribute(ITEM_ATTRIBUTE_STORE)) or 0) > 0 then
            item:removeAttribute(ITEM_ATTRIBUTE_STORE)
            item:removeAttribute('owner')
            recovered = recovered + 1
            -- Keep the same item (and any backpack contents). If space or capacity
            -- prevents moving it, it remains accessible in the Store Inbox.
            if backpack then item:moveTo(backpack, 0) end
        end
    end
    if recovered > 0 then
        player:sendTextMessage(MESSAGE_EVENT_ADVANCE, 'Your earlier hunting shop items have been recovered. Check your backpack; any that did not fit remain in your Store Inbox and can now be moved normally.')
    end
    return recovered
end

local action = Action()
function action.onUse(player, item)
    -- A bag on the ground or belonging to someone else cannot unlock your reward.
    if item:getTopParent() ~= player then
        player:sendCancelMessage('Move this reward into your inventory or inbox first.')
        return true
    end
    if item:getId() == POTION_BOX then return openPotionBox(player, item) end
    if item:getId() == OUTFIT_BAG then openOutfitBag(player, item) return true end
    if item:getId() == MOUNT_BAG then openMountBag(player, item) return true end
    return false
end
action:id(POTION_BOX, OUTFIT_BAG, MOUNT_BAG)
action:register()
