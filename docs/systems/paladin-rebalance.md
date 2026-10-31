# Paladin rebalance implementation

The source and sibling OTClient checkout implement the paladin spell/stance
changes below. The local Docker server now runs
`canary-local:paladin-rebalance-20261029`. Its real startup registry check passed
for both barrages, ground-targeted Divine Grenade, stances, effect IDs and the
formula helper. Live combat/visual playtesting remains distinct from these checks.

## Reference and scope

Use the official [release-state announcement](https://www.tibia.com/news/?id=8833&subtopic=newsarchive)
and [July adjustments](https://www.tibia.com/news/?id=8872&subtopic=newsarchive).
The July values take precedence: Sharpshooter 32%, Divine Defiance 6% specialized
magic and 12% dodge, Divine Barrage power 130, Divine Caldera power 150.
Installed 15.25 client spell metadata and animation previews, extracted for the
[sorcerer audit](sorcerer-rebalance.md#official-installed-assets-audit-2026-10-28),
provide the IDs, icons, cast effects and 21-tile barrage footprint.

This work covers spells, stances, targeting, related Wheel augments and their
client presentation. The announcement's ammunition and shared vocation changes
are separate from this spell/stance implementation.

## Stances

| Stance | Words | Spell ID | Effect | Official icon index |
| --- | --- | ---: | ---: | ---: |
| Sharpshooter | `utori con` | 313 | 5 | 193 |
| Divine Defiance | `utori hur` | 314 | 286 | 194 |

Both require a premium promoted paladin of level 20, cost 250 mana and have
10-second individual/shared Stance cooldowns plus 2-second Support cooldown.
Recasting the selected stance toggles it off. The `paladin-stance` KV key stores
the selected name or `none`; login restores it only when eligibility passes.
Death retains the selection. Changing vocation/demotion clears runtime state.
The old timed `utito tempo san` spell (135) is removed. Old text hotkeys using
those words must be reassigned to `utori con`.

Selection is player-owned, separate from sorcerer conversion charge. Sharpshooter
multiplies total distance skill after gear, conditions, Wheel and proficiency,
floors the result and saturates at the skill protocol limit. Divine Defiance
contributes floor(distance * 0.06) only to holy/healing specialized magic. It
updates as the underlying distance skill changes and cannot stack with
Sharpshooter. It rolls a separate 12% dodge for a non-adjacent attacker on the
same floor; field and condition ticks and attacker-less damage are excluded.

The existing opcode 86 state/badge flow now recognizes paladin stances. The
server refreshes skills/stats on selection and sends authoritative badge state;
client login resynchronization handles both paladin and sorcerer selection.

## Spells and effects

| Spell | Words | ID | Level / mana | Cooldown | Impact effect |
| --- | --- | ---: | --- | --- | ---: |
| Divine Barrage | `exori dir san` | 302 | 70 / 175 | 4 seconds | 319 |
| Ethereal Barrage | `exori dir moe` | 303 | 60 / 135 | 4 seconds | 320 |

Both are premium paladin spells with 2-second Attack cooldown, range 7 and the
21-tile diamond-arrow area. Divine scales with magic level, Ethereal with total
distance skill, independently of weapon attack or weapon element. They can hit
an explicit cursor/crosshair position, the current target's position, or self
when no living target is selected. Out-of-range and blocked targets are rejected.
Explicit aim does not change facing or fall back to the attack target.

Divine Grenade uses the same ground-target selection. Its three-second blast
snapshots the area and combat values, and validates weak caster/spell identities,
original player session, death generation, spell replacement and shutdown before
combat or client output. It no longer resolves a bare reused player ID in Lua.
Wheel stages give 26/20/14-second cooldowns and 190/220/250 base power. It retains
its level 300 and Wheel-unlock gates. Death Echo's ordinary cast now also uses
its current target, as described by the same official targeting contract.

Swift Foot permits attacks/spells while applying a separately identified 30%
damage penalty for its ten-second duration. Cooldowns are 4 seconds individual,
2 seconds Support and 2 seconds Focus. Failed combat does not apply the penalty
or haste to familiars. Divine Dazzle's jump range is 7. Salvation power is 500.

Clients below 15.25 receive existing holy/physical effects for the two barrages.
Current-client effect and spell icon IDs come from installed official metadata.
The shared official spell atlases are reused without rearranging existing icons.

## Damage model

Official metadata states power and scaling type, but does not publish the full
server formulas. Numerical bounds use the game-checked
[TibiaTools dataset](https://github.com/kik-tibia/tibiatools/blob/fe745abfe6fd53bbac32aacba31edc7e888a8465/src/data/spells.json)
and its documented source model, pinned at that revision. No third-party source
code is copied into the server. These are empirical numerical references, not
proof of exact CipSoft implementation or random-distribution parity.

For each bound, the implementation adds the level contribution to
floor(power-bound * (skill / skill-factor + 0.25)). Level contribution increases
by 100 per band, with successive bands requiring 500, 600, 700, ... levels.

| Spell | Minimum / maximum power | Skill factor |
| --- | --- | ---: |
| Divine Barrage | 120 / 140 | 25 (magic) |
| Ethereal Barrage | 37 / 43 | 17.8571 (distance, empirical) |
| Divine Caldera | 130 / 170 | 25 (magic) |
| Divine Grenade | stage power minus/plus 50 | 25 (magic) |
| Salvation | 375 / 625 | 25 (magic) |

Salvation retains its prior +/-25% spread while scaling its base healing from
400 to 500. The common helper is `data/scripts/lib/register_paladin_spells.lua`.
Canary's existing combat randomization, damage multiplier and augment machinery
remain in use; live numerical calibration remains required.

## Wheel compatibility

Persisted slot and gem numeric IDs remain unchanged. Both former Sharpshooter
nodes now grant Ethereal Barrage (+10% life leech at I, +10 percentage points
critical chance at II). Both former Swift Foot nodes grant Divine Barrage
(+8% and +12% base damage, cumulative +20%). Divine Dazzle grants two additional
targets and an eight-second cooldown reduction; Divine Caldera II grants 10%.

The official medium atlas uses gold-arrow cell 19 for Divine Barrage and
blue-arrow cell 22 for Ethereal Barrage, verified against their official spell
icons. Other paladin cells match the previous atlas byte-for-byte. OTClient now
uses that atlas and the official paladin background. Node descriptions, both
node copies, locked tooltips and summary tooltips use the replacement perks.
Existing gem identities/descriptions remain associated with their existing
spells; the replaced nodes did not have corresponding spell-specific gems.

A shared units bug was also corrected: Wheel critical chance was passed as
percentage points into a combat field measured in hundredths. Both registered
augments and runtime bonus paths now convert exactly once. The analogous
sorcerer and monk paths are covered by the same regression.

## Validation and deployment checks

The maintained `macos-release` server/test build passed. All 379 C++ unit tests
passed (the existing network tests require local socket access). Nine Lua suites
passed, covering the paladin and sorcerer spell/stance/Wheel contracts, spell HUD,
spell assignment and Wheel text rendering. The isolated native OTClient test
also passed: 360 locked/unlocked node descriptions, both copies of each changed
paladin node, paladin/sorcerer summary tooltips and 12 gem grade labels. Official
spell atlases, medium Wheel atlas and paladin backdrop were compared byte-for-byte
with the installed official resources.

Run the maintained `macos-release` build with tests enabled. Lua checks:

```sh
luajit tests/lua/test_paladin_stances.lua ../otclient
luajit tests/lua/test_paladin_spells.lua ../otclient
luajit tests/lua/test_paladin_wheel.lua ../otclient
luajit tests/lua/test_spell_hud.lua ../otclient
luajit tests/lua/test_wheel_text_rendering.lua ../otclient
```

`PaladinStance.*` and `PaladinStancePlayer.*` cover arithmetic, real player skill
and specialized-magic behavior, eligibility, vocation changes, saved Wheel
mappings, cumulative bonuses and critical-chance units. Lua contracts cover
persistence/toggling, failure propagation, metadata, formulas, area sizes,
Swift Foot conditions and client tooltips. They do not establish live combat
or delayed callback lifetime coverage.

For live acceptance, validate real casting in all aim modes, range and
wall rejection, PvP protection, multi-target damage, dodge adjacency, stance
login/death behavior, official animation rendering and grenade cancellation
on logout/death/reload. The native client regression includes paladin-specific
checks but does not connect to a server. Rebuild the runtime image before expecting
future C++ changes to affect gameplay; editing Lua alone does not add bindings or
delayed combat.

## Local deployment: 2026-10-29

The previous running image (`focus-cooldowns-20261028`) did not contain the
paladin implementation even though the client listed the new spells. The local
server was stopped gracefully, its database backed up to
`/tmp/canary-before-paladin-20261029.sql`, and its image replaced with
`canary-local:paladin-rebalance-20261029`. The ignored `docker/.env` image setting
was updated while preserving the store overlay. Startup completed without spell
load errors. `Damian Paladin` remains level 2000 and Royal Paladin.

`tests/lua/test_paladin_runtime.lua` was temporarily copied into the container's
`data/scripts/zz_paladin_runtime_probe.lua` before startup. Its PASS log verifies
the actual runtime bindings, spell IDs/words/cooldowns, effects and that all three
paladin ground spells have `needTarget=false`, range 7 and wall checks. The copied
probe was removed after startup. This is a registration check, not a combat test.

`luajit tests/lua/test_ground_spell_aim.lua ../otclient` verifies that both
barrages, Divine Grenade and Death Echo send empty-ground coordinates in cursor
and default free-aim modes without querying a creature target. For an existing
action-bar assignment explicitly set to **On current target**, choose
**At cursor** or **Free aim (click map)** to use empty ground.
