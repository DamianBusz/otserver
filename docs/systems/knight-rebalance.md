# Knight rebalance implementation ledger

## Scope and evidence

Implement the released Knight vocation adjustments in Canary and the sibling
OTClient, including spell mechanics, stances, visuals, healing, proficiency
bonuses and the Wheel. Completion requires matching runtime deployment and
server-connected validation; source changes alone are insufficient.

References: [release state](https://www.tibia.com/news/?id=8833&rejectcookies=0&subtopic=newsarchive)
and [July corrections](https://www.tibia.com/news/?id=8872&subtopic=newsarchive).
July values supersede the release values. The installed official client and RCC
were rehashed on 2026-10-30 and match the source hashes recorded in
`sorcerer-rebalance.md`. Read-only extracted metadata remains in
`/tmp/tibia-resource-audit/embedded/24d4378.json` (spells), `24e1a8e.json`
(previews), and `/tmp/official-wheel-layout.json` (independent layout table).
The baseline of both dirty worktrees is preserved in `/tmp/knight-rebalance-baseline`.
Existing sorcerer, paladin and druid work must remain intact.

## Required final behavior

- Blood Rage (133, `utito tempo`): persistent, toggleable stance; +25% total
  sword/axe/club skill, +15% damage received. Protector (132, `utamo tempo`):
  +30% total shielding, -15% damage dealt and received. Mutually exclusive;
  level 20, promoted/premium Knight, 20 mana, 2-second individual, Support and
  Stance cooldowns. Preserve selection across sessions, invalidate on demotion
  or vocation change. Remove old timed conditions and their double bonuses.
- Shield Bash (315, `exori ico scu`): level 18, 30 mana, 4-second cooldown,
  base power 55. Shield Slam (316, `exori scu`): level 30, 110 mana,
  6-second cooldown, base power 52, eight adjacent targets. Both have 2-second
  Attack cooldown, require a shield, scale from its defense, and reduce each
  victim's next auto-attack within ten seconds by 50% (75% with Slam II).
  Verify target rejection, split-element hits, consumption/expiry and identity.
- Berserk/Fierce Berserk/Groundshaker mana: 125/360/200. Chivalrous Challenge:
  four base targets, jump range seven (the installed preview explicitly places four targets). Front Sweep: power 80, normal three-tile
  footprint and five tiles with augment II; explicit aim selects a cardinal
  direction without changing facing or falling back to the attack target.
- Knight direct heals scale with magic and shielding, powers 15/70/225/500 for
  Bruise Bane/Wound Cleansing/Fair Wound Cleansing/Intense Wound Cleansing.
  Individual/group cooldowns are two seconds except Intense's 120-second
  individual cooldown. Mana 10/60/135/300. Double healing proficiency perks;
  reduce Intense cooldown-reduction augments by 80%, including gems.
- Wheel: Front Sweep I +40% base damage, II area; Shield Slam I +15% life
  leech, II +25 percentage points attack suppression; Groundshaker I cooldown
  -2 seconds, II +12.5% damage. Battle Healing: +10% healing spell output,
  +20% with shield, no challenge-triggered heals. Combat Mastery: 1% incoming
  reduction/outgoing increase per 14/12/10% missing own/target HP by stage,
  respectively; defense doubled with shield, offense with two-handed weapon.
  Remove old shield defense and critical-damage grants.
- Preserve all serialized slot, stage and gem IDs. Independently extracted
  official Knight column assigns Shield Slam to slots 8/24, Groundshaker to
  11/26, Front Sweep to 6/21, Fierce Berserk to 16/31 and Intense Wound
  Cleansing to 13/29. Verify every pair, cumulative grades and units.
- OTClient: matching metadata, icons/effects, stance HUD, aimed casting,
  Wheel node artwork/text, summaries and gem descriptions. Compare actual
  effect animations and official icon cells, not merely effect ID presence.
- Run focused Lua/engine tests, native client regression, maintained build,
  and runtime registration/combat checks. Back up the database and replace
  Docker with matching binary/scripts before handing off for playtesting.

## Current result — 2026-10-30

The functional implementation is deployed and has passed the final native and
server-connected acceptance checks. Full goal completion remains pending either
acceptance of the documented tunable healing model or an authoritative measured
replacement. The outstanding user question has not yet been answered. This is
not a claim of exact official healing coefficients.
Existing Sorcerer, Paladin and Druid work is preserved. No commit was created.

| Requirement | Current evidence |
| --- | --- |
| Persistent, exclusive Knight stances, total-skill bonuses and damage factors | Native stance/health-drain tests; actual casts, mana charges, toggle-off and logout/reconnect restoration in `/tmp/knight-live-results-r5.log` |
| Bash/Slam shield gating, effects, costs, eight-target area and 50%/75% suppression | Native shield/Lua tests; real casts and each affected monster's next attack in the same live log |
| Entire player weapon swing shares suppression | Elemental eldritch claymore hits all three recipients and diamond arrows hit all 21 recipients at both reduction strengths; `/tmp/knight-weapons-results-r2.log` |
| Consumption, expiry, failed attempts and invalidation | Native state/identity tests; actual immune hit, missed arrow, out-of-range attempt and eleven-second expiry in `/tmp/knight-weapons-results-r3.log` |
| Existing spell costs, challenge targets and aimed Front Sweep | Lua metadata/callback tests; base/I/II exact world footprints in all eight cursor octants, unchanged facing, in live acceptance |
| Four heals, effective shielding/magic, mana and cooldown composition | Lua formulas and real native combat; all four live casts in neutral/Protector, no-shield casting, eight Intense cooldown combinations |
| Proficiency and equipment cooldown adjustments | `tests/knight_healing_data_test.py`: 38 independent official perks per server/client table and the 24000 ms equipment augment |
| Wheel grants, stable slots, fractional values and updated majors | Native registration/runtime tests plus `test_knight_wheel.lua`: all five node pairs, cumulative grades, Battle Healing and Combat Mastery |
| Client metadata, visuals, HUD and Wheel text | Exact effect/icon parity, live protocol-1525 reconnect, native rendering of all ten Knight spell nodes and 360 total descriptions; `/tmp/knight-native-wheel-final.log` |
| Matching deployed binary/scripts and cleanup | `/tmp/knight-runtime-audit.json`: image identity, unmodified binary and all 93 changed/new gameplay data files; `/tmp/knight-cleanup.json`: backup, fixture removal and unchanged user character |

The final native build passes with 52 focused tests across Knight and adjacent
vocations (`/tmp/knight-healing-model-tests-final.log`), in addition to the earlier
86 adjacent combat/PvP/spell regressions. Focused Lua suites cover Knight healing,
shield spells, stances, existing spells, Wheel, spell assignment, HUD, aimed
geometry and the confirmed adjacent Strong Ice Wave fix. Live test clients exited
successfully. The isolated profile emits a missing optional `config.ini` message;
its actual connections and all gameplay assertions succeed.

## Runtime details

Image: `canary-local:knight-rebalance-20261030-r2`, ID
`sha256:e89dee47b3a49e67e6d29abd5ce805957b1944e9bf1d1ca8897f9a56198e6f29`.
The maintained Docker build includes the matching server bindings and scripts.
Startup assertions verified spell registrations, stance/shield bindings, healing
helper and effect 318 before live tests. After cleanup the same server restarted
successfully and reported online; neither temporary probe remains loaded.

Deployment backup:
`/tmp/canary-before-knight-r2-20261030T123319Z.sql`, SHA-256
`d4671021c81d15f93ad30f72bebdf6a5fb4ee1c2328c8d07986925f9d690bc02`.
Cleanup backup:
`/tmp/canary-before-knight-cleanup-20261030T132658Z.sql`, SHA-256
`5bfc6023d1048a1c8e47c5af002b605c7251eb5de04c357ba990921ca14e50c6`.
Backups are owner-readable only. The generated account 121 / Knight Probe High,
its temporary credentials, arena and two test hooks were removed. Account/player
counts decreased by exactly one; Damian Knight's full player row remained
unchanged. Production client configuration was not used for these tests.

The weapon acceptance fixes effective attack skill at zero so level 2000 gives a
known base. Elemental cleave hit losses were `11/399/11` normally,
`5/199/5` at 50% and `2/99/2` at 75%, with independent component truncation.
Diamond arrows hit all 21 targets for 400 normally, 200 at 50%, and 100 at 75%.
Ordinary criticals remain possible and are bounded separately. Follow-up attacks
returned to full damage. A forced ordinary-arrow miss consumed exactly one
arrow and the reduction; an immune swing also consumed it. An out-of-range
attempt preserved 75% for the subsequent valid swing. Waiting eleven seconds
removed the reduction before a valid attack.

## Implementation decisions and calibration limits

`AttackReductionState` is creature-owned and expires without scheduled work.
A synchronous swing snapshot covers native cleave and scripted area ammunition;
spell/rune damage does not consume it. Failed attempts preserve pending state;
committed misses consume it. New suppression received during a resolved swing
belongs to the next attack. Removal, death and master changes clear state and
invalidate active scopes. Reapplication keeps the stronger strength and refreshes
ten seconds. That refresh policy and exact official rounding/order remain local
calibration decisions.

Shield damage uses the pinned empirical
[tibiatools dataset](https://github.com/kik-tibia/tibiatools/blob/fe745abfe6fd53bbac32aacba31edc7e888a8465/src/data/spells.json):
`levelBonus + floor((power ± 2) × (effectiveShielding × defense / 1000 + 0.25))`.
Powers are 55/52; effective defense requires a real shield and includes only the
weapon's extra-defense modifier. Shield damage stays physical with elemental
weapon imbuements. Front Sweep uses empirical powers 60–100 around mean 80 and
round-to-nearest bounds. Level bonuses use diminishing bands. These empirical
bounds do not establish an exact official random distribution.

The official client supplies healing powers 15/70/225/500 and magic/shielding
inputs, but not their coefficients or random bounds. The implemented local model
is explicitly tunable in `data/scripts/lib/register_knight_spells.lua`:

`mean = power × (0.25 + healingMagicLevel / 5 + effectiveShielding / 25)`.

Bounds add the diminishing level bonus to 80%/120% of that mean, rounded to
nearest. Integer percentage arithmetic avoids half-point floating-point drift.
At level 2000, magic 15 and shielding 130, pre-bonus bounds are 426–477,
798–1035, 1846–2607 and 3705–5395. Protector increases effective shielding;
Blood Rage does not improve healing. No shield is required. The engine supplies
specialized healing magic and applies Battle Healing, equipment and proficiency
once. Exact official numerical healing parity is **not established**.

Battle Healing adds 10% to primary healing spells, 20% with a shield; it does not
boost potions, runes or secondary healing. Combat Mastery uses continuous
proportional missing HP at the stated 14/12/10 thresholds, with component
truncation at final conversion. Exact official quantization/order is not exposed
by the source metadata. Old defense/critical grants and display contributions
are removed. Fractional spell boosts remain doubles through Wheel registration
and runtime aggregation, including Groundshaker 12.5% and Terra Wave 6.5%.

## Healing review for manual playtesting

Baseline: level 2000, effective healing magic 15, effective shielding 130 before
Protector (169 with Protector), no healing equipment/proficiency/Wheel bonuses,
and sufficient missing HP to avoid overheal. These are **local model bounds**,
not official-client measurements. Actual equipment and Wheel bonuses can move
your character's healing outside this baseline.

| Spell | Words | Mana | Neutral | Protector |
| --- | --- | ---: | ---: | ---: |
| Bruise Bane | `exura infir ico` | 10 | 426–477 | 445–505 |
| Wound Cleansing | `exura ico` | 60 | 798–1035 | 886–1166 |
| Fair Wound Cleansing | `exura med ico` | 135 | 1846–2607 | 2127–3028 |
| Intense Wound Cleansing | `exura gran ico` | 300 | 3705–5395 | 4329–6331 |

A bounded source recheck on 2026-10-30 did not produce a measured replacement:
[upstream Canary](https://raw.githubusercontent.com/opentibiabr/canary/main/data/scripts/spells/healing/fair_wound_cleansing.lua)
still uses its earlier level/magic formula without shielding;
[TibiaTools' current spell table](https://raw.githubusercontent.com/kik-tibia/tibiatools/main/src/data/spells.json)
has no healing/Cleansing entries; and
[Crystalserver's implementation issue](https://github.com/zimbadev/crystalserver/issues/773)
explicitly marks the Knight healing coefficients tunable. Those implementations
cannot establish official numerical parity. To calibrate from official play,
record effective level/magic/shielding, stance, equipment/proficiency/Wheel
bonuses, missing HP and the full sequence of healing results. A single observed
minimum/maximum is insufficient to establish exact random bounds.

## Proficiency, cooldown and client parity notes

Do not multiply proficiency data again. The 15.25 import already doubled the
relevant Sanguine, Cobra/Lion, Falcon, Avenger and Phantasmal healing perks and
reduced Intense proficiency cooldown bonuses from 150 to 30 seconds. The fixture
`tests/fixtures/knight-healing-proficiencies.json` pins all 38 relevant entries to
official table SHA-256
`7fea90ec1cfd472f4b5978f4456b430d3271598b4e545b7919d692641411e015`.
No Intense cooldown gem exists in the maintained enum/dispatch. Fair Wound
Cleansing's healing gem is unchanged. Stoic iks chestplate (44620) now reduces
Intense by 24000 ms. The engine's existing 50%-of-base cooldown floor gives
120/90/96/66 seconds for no/proficiency/equipment/both augments, and 60 seconds
with Wheel II in each combination. Healing-group cooldown stays two seconds.

Bruise Bane's stale ID 170 is corrected to 175, resolving official icon cell 131
without changing unrelated atlas indices. Shield icons 195/196 and all four
healing icons match official pixels at 20 and 32 pixels. Effects 5/286/318 match
complete animation records, sprite order, dimensions and compressed sheet bytes.
The official executable/RCC hashes remain unchanged. Parity reports:
`/tmp/knight-effect-parity.json`, `/tmp/knight-shield-icon-parity.json`, and
`/tmp/knight-healing-icon-parity.json`.

All 36 Knight Wheel cells map independently to the official layout. The actual
medium atlas SHA-256 is
`64a798e63ec8ee3fe2d23d46ebe57d9bb55eb441afd506e03801f810e925af5b`.
Native rendering verifies icon sources, locked/full descriptions, summaries and
measured text fit; both copies of all five Knight spell nodes are checked.

Live geometry uncovered an extra anchor row in Front Sweep and the analogous
Strong Ice Wave matrix. Both were corrected atomically: the engine supplies a
cast position one tile ahead, and marker 3 is an affected anchor cell. Tests now
assert actual world geometry and count both 1 and 3 as affected. The nearest
AGENTS rule records this invariant. The final deployed image includes both fixes.

The earlier investigation chronology is preserved locally in
`/tmp/knight-rebalance-history-20261030.md`; its historical pending items are
superseded by this acceptance record.
