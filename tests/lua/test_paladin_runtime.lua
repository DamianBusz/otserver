-- Deployment smoke check: temporarily copy into data/scripts before startup.
-- This inspects the real Lua bindings and registered spells without casting or
-- changing characters. Remove the copied script after observing the PASS log.
local check = GlobalEvent("PaladinDeploymentCheck")
function check.onStartup()
	assert(Player.setPaladinStance and Player.getPaladinStance and Player.castAimedSpell)
	assert(CONST_ME_DIVINE_BARRAGE == 319 and CONST_ME_ETHEREAL_BARRAGE == 320)
	assert(PaladinSpellFormula and PaladinSpellFormula.bounds)
	for _, row in ipairs({
		{ "Divine Barrage", "exori dir san", 302, 175, 4000 },
		{ "Ethereal Barrage", "exori dir moe", 303, 135, 4000 },
		{ "Divine Grenade", "exevo tempo mas san", 258, 160, 26000 },
	}) do
		local spell = assert(Spell(row[1]), "Missing spell: " .. row[1])
		assert(spell:words() == row[2] and spell:id() == row[3], row[1])
		assert(spell:mana() == row[4] and spell:cooldown() == row[5], row[1])
		assert(not spell:needTarget() and spell:range() == 7 and spell:isBlockingWalls(), row[1])
	end
	for _, row in ipairs({ { "Sharpshooter", 313 }, { "Divine Defiance", 314 } }) do
		assert(Spell(row[1]):id() == row[2], row[1])
	end
	logger.info("[Paladin deployment] PASS: both barrages, ground-targeted grenade, stances, effects and formula helper registered")
	return true
end
check:register()
