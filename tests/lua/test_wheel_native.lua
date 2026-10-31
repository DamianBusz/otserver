-- Run as otclientrc.lua in an isolated OTClient working directory/user profile.
-- Requires the real client modules and fonts; never load this in a playing session.
scheduleEvent(function()
  local ok, err = pcall(function()
    local m = assert(modules.game_wheel)
    local wheel = m.WheelOfDestiny
    local panel = m.wheelOfDestinyWindow.selection.tabContent
    m.wheelWindow:show()
    m.wheelOfDestinyWindow:show()
    panel:show()
    for vocation = 1, 5 do
      wheel.vocationId = vocation
      for _, filled in ipairs({false, true}) do
        for node = 1, 36 do
          wheel.pointInvested[node] = filled and m.WheelBonus[node - 1].maxPoints or 0
        end
        for node = 1, 36 do
          wheel.configureDedication(node)
          wheel.configureConviction(node)
          assert(not panel.conviction:getText():find('�', 1, true), 'broken grade glyph')
          assert(#panel.conviction:getText() > 0,
            'blank native conviction: vocation ' .. vocation .. ', node ' .. node)
        end
      end
    end
    wheel.vocationId = m.SORCERER
    for _, node in ipairs({6, 21, 11, 26}) do
      wheel.configureDedication(node)
      wheel.configureConviction(node)
      local text = panel.conviction:getText()
      assert(text:find('I:', 1, true) and text:find('II:', 1, true), 'missing readable grades')
      local focus = node == 6 or node == 21
      assert(text:find(focus and 'Augmented Focus Spells' or 'Augmented Death Echo', 1, true))
      assert(text:find(focus and '+5% Base Damage' or '+12% Base Damage', 1, true))
      local size = panel.conviction:getSize()
      assert(size.width > 0 and size.height > 0 and panel.conviction:isVisible())
      print('NATIVE WHEEL node ' .. node .. ': ' .. text:gsub('\n', ' / '))
    end
    -- Render actual summary tooltip markup through the real BBCode and native parsers.
    wheel.configureConvictionPerk()
    local renderer = g_ui.createWidget('UILabel')
    local foundDeath = false
    for _, row in ipairs(m.wheelOfDestinyWindow.convictionPerks.tabContent:getChildren()) do
      local info = row:getChildById('info')
      local perk = row:getChildById('perk')
      if info and info.parseColoreDisplay then
        renderer:parseColoredText(info.parseColoreDisplay)
        assert(#renderer:getText() > 0, 'empty native summary tooltip')
        if perk:getText() == 'Aug. Death Echo' then
          assert(renderer:getText():find('+12% Base Damage', 1, true))
          foundDeath = true
        end
      end
    end
    renderer:destroy()
    assert(foundDeath)
    wheel.vocationId = m.PALADIN
    for _, row in ipairs({
      {6, 21, 'Ethereal Barrage', '10% life leech', '+10% Critical Hit Chance'},
      {13, 29, 'Divine Barrage', '+8% Base Damage', '+12% Base Damage'},
      {11, 26, 'Divine Dazzle', '+2 additional', '-8s'},
      {16, 31, 'Divine Caldera', '-20 Mana Cost', '+10% Base Damage'}
    }) do
      for _, node in ipairs({row[1], row[2]}) do
        wheel.configureDedication(node)
        wheel.configureConviction(node)
        local text = panel.conviction:getText():gsub('\n', ' ')
        for index = 3, 5 do
          assert(text:find(row[index], 1, true), 'paladin native node ' .. node .. ': ' .. text)
        end
        assert(panel.conviction:isVisible() and panel.conviction:getSize().height > 0)
        print('NATIVE PALADIN node ' .. node .. ': ' .. text)
      end
    end
    wheel.configureConvictionPerk()
    local paladinRenderer = g_ui.createWidget('UILabel')
    local paladinSummaries = {}
    for _, row in ipairs(m.wheelOfDestinyWindow.convictionPerks.tabContent:getChildren()) do
      local info, perk = row:getChildById('info'), row:getChildById('perk')
      if info and info.parseColoreDisplay then
        paladinRenderer:parseColoredText(info.parseColoreDisplay)
        paladinSummaries[perk:getText()] = paladinRenderer:getText()
      end
    end
    assert(paladinSummaries['Aug. Ethereal Barrage']:find('+10% Critical Hit Chance', 1, true))
    assert(paladinSummaries['Aug. Divine Barrage']:find('+12% Base Damage', 1, true))
    assert(not paladinSummaries['Aug. Sharpshooter'] and not paladinSummaries['Aug. Swift Foot'])
    paladinRenderer:destroy()
    wheel.vocationId = m.DRUID
    for _, row in ipairs({
      {16,31,'Strong Ice Wave', '+6% Base Damage', 'Affected area enlarged'},
      {8, 24, 'Mass Healing', '+4% Base Healing', 'Affected area enlarged'},
      {6,21,'Forked Spells', '-2s Cooldown', '+1 Target'},
      {13, 29, 'Terra Wave', '+6.5% Base Damage', '10% life leech'},
      {11,26,'Heal Friend', '+4% Base Healing', '+6% Base Healing'}
    }) do
      for _, node in ipairs({row[1], row[2]}) do
        wheel.configureDedication(node)
        wheel.configureConviction(node)
        local text = panel.conviction:getText():gsub('\n', ' ')
        for index = 3, 5 do
          assert(text:find(row[index], 1, true), 'druid native node ' .. node .. ': ' .. text)
        end
        assert(panel.conviction:isVisible() and panel.conviction:getSize().height > 0)
        print('NATIVE DRUID node ' .. node .. ': ' .. text)
      end
    end
    wheel.configureConvictionPerk()
    local druidRenderer = g_ui.createWidget('UILabel')
    local druidSummaries = {}
    for _, row in ipairs(m.wheelOfDestinyWindow.convictionPerks.tabContent:getChildren()) do
      local info, perk = row:getChildById('info'), row:getChildById('perk')
      if info and info.parseColoreDisplay then
        druidRenderer:parseColoredText(info.parseColoreDisplay)
        druidSummaries[perk:getText()] = druidRenderer:getText()
      end
    end
    assert(druidSummaries['Aug. Forked Spells']:find('+1 Target', 1, true))
    assert(druidSummaries['Aug. Heal Friend']:find('+6% Base Healing', 1, true))
    assert(druidSummaries['Aug. Terra Wave']:find('+6.5% Base Damage', 1, true))
    assert(not druidSummaries["Aug. Nature's Embrace"])
    druidRenderer:destroy()
    wheel.vocationId = m.KNIGHT
    for domain=0,3 do wheel.vesselEnabled[domain]={} end -- normally initialized by the server Wheel response
    for _, row in ipairs({
      {6,21,'Front Sweep','+40% Base Damage','Affected area enlarged'},
      {8,24,'Shield Slam','15% life leech','75% total'},
      {11,26,'Groundshaker','-2s Cooldown','+12.5% Base Damage'},
      {13,29,'Intense Wound Cleansing','+125% Base Healing','-60s Cooldown'},
      {16,31,'Fierce Berserk','-30 Mana Cost','+10% Base Damage'},
    }) do
      for _, node in ipairs({row[1],row[2]}) do
        wheel.configureDedication(node)
        wheel.configureConviction(node)
        local text=panel.conviction:getText():gsub('\n',' ')
        for i=3,5 do assert(text:find(row[i],1,true),text..' missing '..row[i]) end
        wheel.insertPoint(node, m.WheelBonus[node-1].maxPoints)
        local icon=m.wheelPanel:recursiveGetChildById('icon'..node)
        assert(icon:getImageSource():find('rebalance%-mediumperks'), 'Knight node uses stale atlas')
        local measured,available=panel.conviction:getTextSize(),panel.conviction:getSize()
        assert(measured.width<=available.width and measured.height<=available.height,
          'Knight node text clips: '..node..' '..measured.width..'x'..measured.height..' in '..available.width..'x'..available.height)
        print('NATIVE KNIGHT node '..node..': '..text)
      end
    end
    wheel.configureConvictionPerk()
    local knightRenderer=g_ui.createWidget('UILabel')
    local knightSummaries={}
    for _, row in ipairs(m.wheelOfDestinyWindow.convictionPerks.tabContent:getChildren()) do
      local info,perk=row:getChildById('info'),row:getChildById('perk')
      if info and info.parseColoreDisplay then
        knightRenderer:parseColoredText(info.parseColoreDisplay)
        knightSummaries[perk:getText()]=knightRenderer:getText()
      end
    end
    assert(knightSummaries['Aug. Shield Slam']:find('75% total',1,true))
    assert(knightSummaries['Aug. Groundshaker']:find('12.5%',1,true))
    assert(knightSummaries['Aug. Front Sweep']:find('40%',1,true))
    assert(not knightSummaries['Aug. Chivalrous Cha...'])
    knightRenderer:destroy()
    -- Feed gem fixtures through the real selection handler and native label.
    local gem, level
    m.GemAtelier.getFilledVesselCount = function() return 3 end
    m.GemAtelier.getEquipedGem = function() return gem end
    m.Workshop.getGemInformationByBonus = function() return 'Gem bonus', level end
    local grades = {'(I)', '(II)', '(III)', '(IV)'}
    for quality = 0, 2 do
      gem = {gemType = quality, gemID = 1, lesserBonus = 1, regularBonus = 2, supremeBonus = 3}
      for rank = 0, 3 do
        level = rank
        wheel.onGemVesselClick(0)
        local name = m.wheelOfDestinyWindow.selection.gemContent.gemName:getText()
        assert(not name:find('�', 1, true), 'broken native gem grade glyph')
        assert(name:find(grades[rank + 1], 1, true), 'missing native gem grade')
      end
    end
    print('NATIVE WHEEL PASS: 360 locked/unlocked descriptions, knight, sorcerer, paladin and druid summary tooltips and 12 gem grade labels')
  end)
  if not ok then print('NATIVE WHEEL FAIL: ' .. tostring(err)) end
  g_app.exit()
end, 1500)
