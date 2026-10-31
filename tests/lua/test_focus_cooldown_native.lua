-- Run as otclientrc.lua in a separate OTClient working directory and --user-dir.
-- Real action-bar handlers and native UIProgressRect widgets; no server login.
scheduleEvent(function()
  local ok, err = pcall(function()
    local a = assert(modules.game_actionbar)
    local spells = a.Spells
    modules.client_options.setOption('graphicalCooldown', true)
    modules.client_options.setOption('cooldownSecond', true)
    local panel = g_ui.createWidget('UIWidget', rootWidget)
    panel:setSize({width=220,height=50})
    local buttons = {}
    for i, words in ipairs({'exevo gran mas vis','exevo gran mas flam','uteta res ven'}) do
      local b = g_ui.createWidget('UIWidget', panel)
      b:setId('1.'..i)
      b:setSize({width=40,height=40})
      local cooldown = g_ui.createWidget('UIProgressRect', b)
      cooldown:setId('cooldown')
      cooldown:setSize({width=40,height=40})
      local data = assert(spells.getSpellDataByParamWords(words))
      b.cache={isSpell=true,isRuneSpell=false,spellID=data.id,spellData=data}
      buttons[i]=b
    end
    a.activeActionBars={{tabBar=panel}}
    a.spellCooldownCache={}
    a.spellGroupCooldownCache={}
    local rage, hell, avatar=buttons[1], buttons[2], buttons[3]
    local function remaining(b) return b.cooldown:getDuration()-b.cooldown:getTimeElapsed() end
    local function expect(b, ms, label)
      assert(math.abs(remaining(b)-ms)<1000, label..': '..remaining(b))
      assert(#b.cooldown:getText()>0, label..': missing visible countdown')
    end
    -- Wheel-reduced Focus cooldown received from the server.
    a.onSpellCooldown(119,36000)
    a.onSpellGroupCooldown(7,36000)
    a.onSpellGroupCooldown(1,2000)
    expect(rage,36000,'Rage individual cooldown')
    expect(hell,36000,'Hell shared Focus cooldown')
    a.checkRemainSpellCooldown(hell,24)
    expect(hell,36000,'Focus survives button refresh')
    -- Both focus spells share the same authoritative group in both directions.
    a.onSpellCooldown(24,40000)
    a.onSpellGroupCooldown(7,40000)
    expect(rage,40000,'Rage shares Hell cooldown')
    -- A long restored Avatar timer cannot be hidden by a short Support group.
    a.onSpellCooldown(266,389000)
    a.onSpellGroupCooldown(3,2000)
    expect(avatar,389000,'restored Avatar cooldown')
    a.checkRemainSpellCooldown(avatar,266)
    expect(avatar,389000,'Avatar survives button refresh')
    a.onSpellCooldown(266,10000)
    expect(avatar,10000,'authoritative cooldown reduction')
    -- Expired individual and group timers clear, rather than leaving a stale timer.
    a.onSpellGroupCooldown(3,0)
    a.onSpellCooldown(266,0)
    assert(avatar.cooldown:getText()=='', 'expired Avatar timer must clear')
    -- Disabled visuals still record server timers for later restoration.
    modules.client_options.setOption('graphicalCooldown',false)
    modules.client_options.setOption('cooldownSecond',false)
    a.onSpellCooldown(266,15000)
    modules.client_options.setOption('cooldownSecond',true)
    a.checkRemainSpellCooldown(avatar,266)
    expect(avatar,15000,'timer cached with hidden visuals')
    for _, b in ipairs(buttons) do
      if b.cache.removeCooldownEvent then removeEvent(b.cache.removeCooldownEvent) end
    end
    a.activeActionBars={}
    panel:destroy()
    print('NATIVE FOCUS PASS: shared Focus group, refresh, Avatar restore/reduction/expiry, disabled visuals')
  end)
  if not ok then print('NATIVE FOCUS FAIL: '..tostring(err)) end
  g_app.exit()
end,1500)
