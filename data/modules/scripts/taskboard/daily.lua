return function(api)
	local daily = {}
	api.daily = daily

	function daily.getProducts(difficulty, raceId)
		local result = {}
		for _, product in ipairs(api.catalog.getCreatureProducts(difficulty, raceId)) do
			if product.chance >= api.config.daily.minimumProductChance then
				table.insert(result, product)
			end
		end
		return result
	end

	local function assignRewards(task, difficulty, preserveRoll)
		local products = daily.getProducts(difficulty, task.raceId)
		if #products == 0 then return false end
		local product
        if preserveRoll then
            for _, candidate in ipairs(products) do if candidate.id == task.rewardId then product = candidate break end end
        end
        product = product or products[math.random(1, #products)]
		local expectedDrops = task.required * product.chance / 100000 * (product.minCount + product.maxCount) / 2
		task.rewardId = product.id
		task.rewardCount = math.max(api.config.daily.minimumProductsPerTier * (difficulty + 1), math.min(api.config.daily.maximumProductCount, math.floor(expectedDrops * api.config.daily.productDropPercent / 100)))
		local entry = api.catalog.get(task.raceId)
		local monsterXp = entry and entry.monsterType:experience() or 0
		task.experience = api.clampU32(math.floor(monsterXp * task.required * api.config.daily.experiencePercent / 100))
        task.bountyPoints = 0
        if preserveRoll then return true end
		task.bonusPotionId = 0
		local potions = api.config.daily.dromePotions
		if #potions > 0 and math.random(1, 10000) <= api.config.daily.potionChancePercent * 100 then
			task.bonusPotionId = potions[math.random(1, #potions)]
		end
		return true
	end

	function daily.ensure(player, state)
		local board = state.daily
		local difficulty = api.getDifficultyForLevel(player:getLevel())
		local oldPolicy = board.policyVersion or 0
		local changed = oldPolicy < 5 or board.difficulty ~= difficulty or board.selectionPending
		if oldPolicy < 2 and not board.selectionPending then
			api.rules.finalizeWeekly(player, api.dailyView(state))
		end
		local allowed, used, tasks = {}, {}, {}
		for _, entry in ipairs(api.catalog.getPool(difficulty)) do allowed[entry.raceId] = true end
		for _, task in ipairs(board.kills or {}) do
			local required = api.clampU16(task.required)
			local valid = required > 0 and api.catalog.isValidRace(task.raceId) and not used[task.raceId]
			local completed = required > 0 and (task.completed or (task.current or 0) >= required)
			if valid and #tasks < api.config.daily.slots and (completed or (board.difficulty == difficulty and allowed[task.raceId])) then
				local record = {
					raceId = task.raceId, required = required,
					current = completed and required or math.min(required, api.clampU16(task.current)),
					completed = completed, rewardId = task.rewardId, rewardCount = task.rewardCount,
					experience = task.experience or 0, bountyPoints = 0, bonusPotionId = task.bonusPotionId or 0,
					claimed = oldPolicy >= 2 and task.claimed == true,
				}
				local rewardValid = true
				if not record.claimed and (oldPolicy < 5 or not record.rewardId or record.rewardId == 0 or not record.rewardCount or record.rewardCount == 0) then
					local previousCount = record.rewardCount
					rewardValid = assignRewards(record, difficulty, oldPolicy >= 3)
					if oldPolicy == 4 and previousCount and previousCount > 0 then record.rewardCount = previousCount end
					changed = true
				end
				if rewardValid then
					used[record.raceId] = true
					table.insert(tasks, record)
				end
			else
				changed = true
			end
		end
		local settings = api.config.bounty.difficulties[difficulty]
		for _, entry in ipairs(api.catalog.randomUnique(api.config.daily.slots - #tasks, function(candidate)
			return not used[candidate.raceId] and #daily.getProducts(difficulty, candidate.raceId) > 0
		end, difficulty)) do
			local task = {raceId = entry.raceId, required = math.random(settings.weeklyKillMinimum, settings.weeklyKillMaximum),
				current = 0, completed = false, claimed = false}
			if assignRewards(task, difficulty, false) then
				table.insert(tasks, task)
				changed = true
			end
		end
		board.kills = tasks
		board.items = {}
		board.anyCreature = {required = 0, current = 0, completed = false}
		board.difficulty, board.policyVersion, board.selectionPending = difficulty, 5, false
		board.killsCompleted, board.itemsCompleted, board.points, board.soulseals = 0, 0, 0, 0
		board.killExperience, board.itemExperience = 0, 0
		for _, task in ipairs(tasks) do
			if task.completed then board.killsCompleted = board.killsCompleted + 1 end
		end
		if changed then board.revision = ((board.revision or 0) % 0xFFFFFFFF) + 1 end
		return changed
	end

	function daily.onKill(player, state, raceId)
		for _, task in ipairs(state.daily.kills) do
			if task.raceId == raceId and not task.completed then
				task.current = math.min(task.required, task.current + 1)
				task.completed = task.current >= task.required
				if task.completed then state.daily.killsCompleted = state.daily.killsCompleted + 1 end
				return true, task.completed
			end
		end
		return false, false
	end

	function daily.claim(player, state, slot, reset, revision)
		if reset ~= state.meta.nextDailyReset or reset <= os.time() or revision ~= state.daily.revision then return false end
		local task = state.daily.kills[(tonumber(slot) or -1) + 1]
		if not task or not task.completed or task.claimed then return false end
		local inbox = player:getInbox()
		if not inbox then return false end
		local rewards = {{id = task.rewardId, count = task.rewardCount}}
		if (task.bonusPotionId or 0) > 0 then table.insert(rewards, {id = task.bonusPotionId, count = 1}) end
		local created = {}
		for _, reward in ipairs(rewards) do
			local item = Game.createItem(reward.id, reward.count)
			if item then table.insert(created, item) end
			if not item or inbox:addItemEx(item, INDEX_WHEREEVER, FLAG_NOLIMIT) ~= RETURNVALUE_NOERROR then
				for _, rollback in ipairs(created) do rollback:remove() end
				return false
			end
		end
		task.claimed = true
        player:addExperience(api.rules.getTaskExperience(player, task.experience), true)
		return true
	end
end
