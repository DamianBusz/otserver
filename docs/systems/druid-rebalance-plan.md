# Druid vocation rebalance — implementation and handoff plan

Status: core druid implementation deployed as r1; follow-up fixes and shared
combat-mode changes are awaiting deployment and validation. The user is performing in-game acceptance. See
[the implementation ledger](druid-rebalance.md) for current validation and
deployment evidence. The completion criteria below remain unchanged.

Prepared: 2026-10-29.

## Objective

Implement the druid vocation rebalance in Canary and its sibling OTClient, then
deploy the matching local server image and validate actual gameplay. Deliver
stances, spells, healing behavior, Wheel changes, official effects and client
presentation as a coherent update.

Do not equate a successful host build or updated client metadata with a working
deployed feature. The previous paladin handoff exposed exactly that mismatch.

## Workspace and existing work

- Server: `/Users/damianbusz/Dev/pixel/canary`.
- Client: `/Users/damianbusz/Dev/pixel/otclient`.
- Read all applicable `AGENTS.md` files in both repositories before edits.
- Both repositories contain existing work. Inspect staged and unstaged changes;
  preserve the sorcerer/paladin implementation and unrelated user changes. Do not
  reset, clean, overwrite, or commit unrelated files.
- Server policy requires reading `docs/building/local-validation.md` before
  building. Reuse the maintained preset/cache and update all maintained build
  lists when adding C++ files.
- Read `docs/systems/lua-shared-userdata.md` before changing shared Lua userdata.
- Client edits may need filesystem approval because it is outside the server
  workspace. Prepare concrete changes before requesting that approval.
- No delegation or subagents are required by this plan.

Existing implementation references:

- [Sorcerer rebalance](sorcerer-rebalance.md).
- [Paladin rebalance and deployment](paladin-rebalance.md).
- `src/creatures/players/components/elemental_stance.hpp`.
- `src/creatures/players/components/paladin_stance.hpp`.
- `data/scripts/creaturescripts/player/aimed_spells.lua`.
- OTClient `modules/game_actionbar/logics/SpellControls.lua`.

As last verified, local Docker runs `canary-local:paladin-rebalance-20261029`.
Re-check this instead of assuming it is still current. The local account
`damian` has `Damian Paladin` (level 2000, Royal Paladin) and existing sorcerer
characters. Preserve these characters, their items, balances and progression.
Do not modify them as test fixtures without authorization.

## 1. Establish the specification

Use these primary sources and check for subsequent official corrections:

- [Release-state specification](https://www.tibia.com/news/?id=8833&subtopic=newsarchive).
- [July balance changes](https://www.tibia.com/news/?id=8872&subtopic=newsarchive).

Use the final two-stance design, not superseded test-server proposals. July
values override earlier release numbers: Forked Glacier power 90, Forked Thorns
97, Strong Ice Wave 140, and Mana Buffer multiplier 10.

Installed official client metadata was previously extracted to
`/tmp/tibia-resource-audit/embedded/24d4378.json`. Animation metadata is at
`/tmp/tibia-resource-audit/embedded/24e1a8e.json`, and extracted images are under
`/tmp/tibia-resource-audit/animations/`. These are temporary audit artifacts,
not permanent dependencies. Verify their existence and provenance; consult the
official-assets audit in the sorcerer document if they need to be recreated.

The installed metadata reported:

| Spell | Words | Spell ID | Icon index | Level | Mana | Individual cooldown |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| Shared Conservation | `utura sio` | 309 | 191 | 20, promoted | 400 | 10 s |
| Elemental Synthesis | `utito dru` | 319 | 190 | 20, promoted | 400 | 10 s |
| Forked Glacier | `exevo fur frigo` | 317 | 197 | 90 | 180 | 6 s |
| Forked Thorns | `exevo fur tera` | 318 | 198 | 80 | 180 | 6 s |

Spell IDs, icon indices and animation/effect IDs are separate namespaces. Do not
reuse one as another or infer artwork from old atlas offsets.

Before implementation, document unresolved details: exact formula bounds,
rounding, secondary-heal target selection, effective versus nominal healing,
critical-healing order, forked target acquisition and jump geometry. Resolve
them from current primary evidence or label an empirical approximation clearly.
Do not silently present inferred formulas as exact official behavior.

## 2. Implement druid stances

Official behavior to implement:

- Shared Conservation: Heal Friend and Nature's Embrace share 30% healing with
  another party member; self-healing gains 10%.
- Elemental Synthesis: grants 10% of magic level as additional ice/earth magic.

Use player-owned state, mutual exclusion and explicit deactivation. Reuse the
existing persistence, shared Stance cooldown and authoritative HUD synchronization
patterns without coupling druid state to sorcerer conversion charge.

Specify promotion/premium/level eligibility, login restoration, death behavior
and vocation-change invalidation. Recompute specialized magic correctly when
gear or underlying magic level changes, without compounding the bonus.

For shared healing, define eligible party recipients, viewport/floor/range
restrictions, target priority and tie-breaking. Exclude the primary recipient.
Apply normal healing restrictions, including protection-zone and special-area
rules. Prevent shared healing, Healing Link and critical healing from recursively
triggering or multiplying each other.

## 3. Implement attacks and existing spell updates

Add Forked Glacier and Forked Thorns with official effects and target limits.
Investigate the existing combat chain implementation before choosing reuse:
verify whether it matches the required forked geometry and selection behavior.
Do not assume a generic sequential chain is equivalent.

Cover selected targets, no selected target, no valid target, walls, floors,
duplicate hits, summons, PvP restrictions and disappearing targets. Resolve
whether no-target casting selects a nearby valid creature from release evidence.
Failure must not consume mana or cooldowns.

Update Strong Ice Wave's shape and cooldown; the strong/ultimate ice and terra
strikes; Wrath of Nature; Heal Friend; and Nature's Embrace. Reconcile Nature's
Embrace's source level requirement of 300 with installed metadata showing 275.

For aimed directional waves, use the existing cardinal-axis contract: explicit
aim does not turn the creature or fall back to its attack target. Test all cursor
octants. Preserve existing cooldown groups unless the verified specification
changes them.

Relevant source files include `data/scripts/spells/attack/strong_ice_wave.lua`,
the corresponding strike scripts, `wrath_of_nature.lua`, and the healing scripts
`heal_friend.lua` and `nature's_embrace.lua`.

## 4. Update healing and Wheel behavior atomically

Required Wheel work:

- Forked Spells augments: cooldown reduction and extra target; verify replacement
  mappings against the official Wheel.
- Heal Friend and Strong Ice Wave augment updates.
- Terra Wave life-leech update.
- Healing Link at 25%.
- Blessing of the Grove: critical healing and stage-dependent health thresholds.

Preserve persisted slot, stage and gem identities. Update both copies of affected
nodes, full tooltips, summaries, artwork and applicable gem descriptions together.
Check cumulative grades and cooldown units.

Primary server integration points are `src/io/io_wheel.cpp`,
`src/creatures/players/components/wheel/player_wheel.cpp` and the healing path in
`src/game/game.cpp`. Client integration lives in `modules/game_wheel/`.

The audit found older behavior: Strong Ice Wave cooldown 8 seconds, stronger
strikes at range 3, Healing Link 10%, and old Nature's Embrace/Strong Ice Wave
augments. New druid stance and forked-spell implementations were absent from the
paths searched. Re-check these findings against the working tree before editing.

## 5. Audit shared vocation changes

Mana Buffer must be handled consistently for druid and sorcerer. The initial
search did not locate an implementation under that name; inspect the actual
damage/death pipeline before concluding it is absent. Verify its surcharge,
timing, insufficient-mana behavior and interaction with other death prevention.
Use bounded arithmetic and test threshold cases.

Inventory rod mana generation, range and imbuement changes, plus shared rune,
potion, mitigation, party-bonus and other applicable vocation adjustments.
Classify each as already implemented, missing, or requiring verification. Apply
confirmed shared prerequisites consistently across affected vocations and avoid
double-applying existing bonuses. Record unrelated knight/monk work separately;
do not silently expand the druid task into a speculative all-vocation rewrite.

## 6. Complete client support and tests

Update spellbook/action-bar metadata, aiming behavior, stance badges, official
icons, effects, cooldown displays and Wheel presentation. Verify effect assets
in the installed client and protocol compatibility rather than checking only
numeric constants.

Required coverage:

- Stance selection, deactivation, persistence, eligibility and dynamic stats.
- Solo and party healing; full-health targets; overhealing; critical thresholds;
  Healing Link interactions; recipients leaving range/party or disappearing.
- Attack target limits, target acquisition, walls/floors, PvP protections and
  rejected-cast resource behavior.
- Every affected Wheel grade, both node copies, saved allocations and units.
- Damage/healing checks at ordinary levels and level 2000, with stance and Wheel
  combinations, using controlled gear and skills.
- Sorcerer/paladin regressions after shared combat changes.
- Native client rendering and live server/client casts, not only widget mocks.

Existing tests under `tests/lua/` provide reusable patterns, including
`test_paladin_spells.lua`, `test_paladin_stances.lua`, `test_paladin_wheel.lua`,
`test_ground_spell_aim.lua`, `test_wheel_native.lua` and
`test_paladin_runtime.lua`. The runtime test demonstrates registration checks;
it does not prove damage, healing or animation correctness.

If delayed work is necessary, use immutable snapshots and validated weak
identities/generations. Test logout, death, reload, replacement and shutdown;
never carry raw owners or mutable references across callbacks.

## 7. Build, deploy and validate

1. Run meaningful tests and the maintained server build. Validate all changed
   CMake/MSVC entries and the client text/rendering contracts.
2. Build a uniquely tagged local image using `docker/Dockerfile.local`.
3. Stop the local server gracefully and save a database backup before replacing
   it. Preserve the existing ignored `docker/.env` settings and store overlay.
4. Deploy the image and check its identity, startup errors, actual Lua bindings
   and spell registrations. Verify status/login services.
5. Load the matching client files. Use authorized test characters to validate
   spells, healing, stances, effects and Wheel behavior in game.
6. Document implemented behavior, test results, deployed image, rollback
   reference and any remaining empirical uncertainty in a druid system document.

Use the paladin/sorcerer deployment documents for the established local workflow.
Do not expose credentials from configuration, database dumps or client profiles.
Do not bypass denied UI permissions with another automation mechanism.

## Completion criteria

- Required druid behavior is implemented in both server and client.
- Existing saved Wheel allocations and other vocation behavior are preserved.
- Automated checks and actual in-game acceptance pass.
- The running Docker server contains the matching implementation.
- The handoff states exactly what was verified and identifies any unresolved
  balance or formula limitations. A registration-only test is not a combat test.
