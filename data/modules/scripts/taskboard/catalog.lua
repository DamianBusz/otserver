return function(api)
	local catalog = {
		ready = false,
		entries = {},
		byRaceId = {},
	}

	api.catalog = catalog
	local creatureProducts = dofile((CORE_DIRECTORY or "data") .. "/modules/scripts/taskboard/creature_products.lua")
	api.creatureProducts = creatureProducts

	local function safeCall(object, methodName, ...)
		if not object or type(object[methodName]) ~= "function" then
			return nil
		end
		local ok, value = pcall(object[methodName], object, ...)
		return ok and value or nil
	end


	function catalog.rebuild()
		catalog.entries = {}
		catalog.productCache = {}
		catalog.byRaceId = {}

		local monsterTypes = Game.getMonsterTypes() or {}
		local discovered = 0
		local rewardBosses = 0
		local invalidRaceIds = 0
		local duplicateRaceIds = 0
		for fallbackName, monsterType in pairs(monsterTypes) do
			discovered = discovered + 1
			local raceId = tonumber(safeCall(monsterType, "raceId")) or 0
			local name = safeCall(monsterType, "name") or tostring(fallbackName)
			local rewardBoss = safeCall(monsterType, "isRewardBoss") == true
			if raceId > 0 and raceId <= 0xFFFF and name ~= "" and not rewardBoss and not catalog.byRaceId[raceId] then
				local entry = {
					raceId = raceId,
					name = name,
					stars = api.clampByte(safeCall(monsterType, "BestiaryStars") or 0),
					monsterType = monsterType,
				}
				catalog.byRaceId[raceId] = entry
				table.insert(catalog.entries, entry)
			elseif rewardBoss then
				rewardBosses = rewardBosses + 1
			elseif raceId <= 0 or raceId > 0xFFFF or name == "" then
				invalidRaceIds = invalidRaceIds + 1
			else
				duplicateRaceIds = duplicateRaceIds + 1
			end
		end

		table.sort(catalog.entries, function(left, right)
			return left.raceId < right.raceId
		end)
		catalog.ready = #catalog.entries > 0
		api.diagnostics.trace("catalog", "rebuild discovered={} usable={} rewardBosses={} invalid={} duplicates={} ready={}", discovered, #catalog.entries, rewardBosses, invalidRaceIds, duplicateRaceIds, catalog.ready)
	end

	local function ensureCatalog()
		if not catalog.ready then
			catalog.rebuild()
		end
		return catalog.entries
	end

	function catalog.get(raceId)
		ensureCatalog()
		return catalog.byRaceId[tonumber(raceId) or 0]
	end

	function catalog.isValidRace(raceId)
		return catalog.get(raceId) ~= nil
	end

	function catalog.getPool(difficulty)
		local entries = ensureCatalog()
		local settings = api.config.bounty.difficulties[api.normalizeDifficulty(difficulty, api.difficulty.beginner)]

		local pool = {}
		local function matchesDifficulty(entry)
			return entry.stars >= settings.minimumStars and entry.stars <= settings.maximumStars
		end

		for _, entry in ipairs(entries) do
			if matchesDifficulty(entry) then
				table.insert(pool, entry)
			end
		end

		return pool
	end

	function catalog.randomUnique(amount, predicate, difficulty)
		local candidates = {}
		for _, entry in ipairs(catalog.getPool(difficulty)) do
			if not predicate or predicate(entry) then
				table.insert(candidates, entry)
			end
		end

		local result = {}
		amount = math.max(0, tonumber(amount) or 0)
		while #result < amount and #candidates > 0 do
			local index = math.random(1, #candidates)
			local entry = candidates[index]
			table.insert(result, entry)
			for candidateIndex = #candidates, 1, -1 do
				if candidates[candidateIndex].raceId == entry.raceId then
					table.remove(candidates, candidateIndex)
				end
			end
		end
		return result
	end

	function catalog.getSoulpitRaceIds()
		local raceIds = {}
		local added = {}
		for _, raceId in ipairs(api.config.soulpit.raceIds or {}) do
			local normalizedRaceId = tonumber(raceId)
			if normalizedRaceId and not added[normalizedRaceId] and catalog.isValidRace(normalizedRaceId) then
				added[normalizedRaceId] = true
				table.insert(raceIds, normalizedRaceId)
			end
		end
		table.sort(raceIds)
		return raceIds
	end

	function catalog.getCreatureProducts(difficulty, raceId)
        ensureCatalog()
        if catalog.productPoolSource ~= catalog.getPool then
            catalog.productCache = {}
            catalog.productPoolSource = catalog.getPool
        end
        local key = tostring(api.normalizeDifficulty(difficulty)) .. ":" .. tostring(raceId or "all")
        if catalog.productCache[key] then return catalog.productCache[key] end
		local result, seen = {}, {}
		local settings = api.config.bounty.difficulties[api.normalizeDifficulty(difficulty)]
		local function collect(loot)
			for _, item in ipairs(loot or {}) do
				local id = tonumber(item.itemId)
                local obtainable = (tonumber(item.chance) or 0) > 0 and not item.unique and (tonumber(item.actionId) or 0) == 0
				if obtainable and id and creatureProducts[id] and not seen[id] then
					local itemType = ItemType(id)
					if itemType and itemType:getId() == id and itemType:getName() ~= "" then
						seen[id] = true
						table.insert(result, {id = id, min = settings.weeklyItemMinimum, max = settings.weeklyItemMaximum,
                            chance = math.min(100000, tonumber(item.chance) or 0),
                            minCount = math.max(1, tonumber(item.minCount) or 1),
                            maxCount = math.max(1, tonumber(item.maxCount) or 1)})
					end
				end
				if obtainable then collect(item.childLoot) end
			end
		end
		local entries = raceId and {catalog.get(raceId)} or catalog.getPool(difficulty)
		for _, entry in ipairs(entries) do
			collect(safeCall(entry.monsterType, "getLoot"))
		end
		table.sort(result, function(a, b) return a.id < b.id end)
        catalog.productCache[key] = result
		return result
	end

	function catalog.getWeeklyItems(difficulty)
		return catalog.getCreatureProducts(difficulty)
	end
end
