# Druid rebalance implementation ledger

The r4 server is deployed for the user's playtest. It includes the cast-result,
wave-callback and combat-mode fixes. Damian Druid (level 2000, Elder Druid,
magic level 150) was created on the existing account in Ankrahmun. Live gameplay
acceptance is incomplete; the user has taken over in-game testing. The Wheel
position correction below is deployed; OTClient must be restarted to load its
matching labels and icons.
The complete scope remains [the implementation plan](druid-rebalance-plan.md).

## Evidence and decisions

The official [release announcement](https://www.tibia.com/news/?id=8833&rejectcookies=0&subtopic=newsarchive)
defines the final two stances. The [July corrections](https://www.tibia.com/news/?id=8872&rejectcookies=0&subtopic=newsarchive)
take precedence over release powers. Installed official spell metadata agrees:
Forked Glacier 90, Forked Thorns 97, Strong Ice Wave 140, Nature's Embrace 2000.
The latter requires level 275, resolving the stale server requirement of 300.

The previously extracted official resources remain available under
`/tmp/tibia-resource-audit`. Spell IDs 309/319 use icon indices 191/190 and
effects 287/288 respectively; forked spells 317/318 use icons 197/198 and
effects 324/325. These are separate namespaces. Strong Ice Wave's preview
contains ten tiles: one adjacent tile followed by three rows of width three.
Resource provenance and hashes are recorded in the sorcerer system document.
All five effect records now match the official client after normalizing absolute
sprite IDs: exact compressed sprite bytes, frame ordering, sheet-relative offsets
and animation metadata. This does not replace live casting/animation acceptance.

### Stance contract

Both stances require a premium, promoted druid of level 20, cost 400 mana,
and use 10-second individual/Stance and 2-second Support cooldowns. Recasting
toggles off. Player-owned selection persists under `druid-stance`; restoration
passes the same eligibility gate. Death retains selection, matching the existing
paladin selection contract. Vocation change or demotion invalidates selection.
Elemental Synthesis derives its bonus from current total general magic level,
excluding specialized magic, so gear/condition changes cannot compound it.
Integer flooring of 10% is an explicit local approximation: official metadata
does not establish the rounding rule. This needs empirical calibration.

### Evidence limits

Official metadata supplies means, not exact random bounds or rounding. The
attack and healing contracts below explicitly identify local approximations.
Mana Buffer boundary cases and secondary-healing ordering now have engine tests;
calibration against official gameplay and local live acceptance remain required.
Rod/wand generation and energy-ring conversion are implemented and engine-tested.
Combat-mode removal and equipment compensation are implemented locally;
replacement weapon-type/monster mitigation coefficients remain unverified.

### Attack formula reference

Use the same empirical power model as the paladin implementation, with the
author's [pinned numerical dataset](https://github.com/kik-tibia/tibiatools/blob/fe745abfe6fd53bbac32aacba31edc7e888a8465/src/data/spells.json).
It specifies power bounds 88–92 for Forked Glacier, 95–99 for Forked Thorns,
100–180 for Strong Ice Wave, 140–210 for Wrath of Nature, and fixed powers
115/195 for strong/ultimate strikes. These reproduce that empirical model,
not a claim about CipSoft's exact implementation. Bounds use general plus
specialized magic divided by 25, a constant 0.25, integer floor, and the
existing diminishing level contribution used for paladin spells.

### Forked target contract

Implementation decision where release text is incomplete: use the selected
target, or acquire the nearest legal creature within seven tiles when none is
selected. Explicit aim must resolve a legal creature and never falls back.
Reject without resources/cooldowns if none exists. Secondary targets lie within
four tiles of the original target, with sight from that origin, on its floor.
Order by distance then creature ID, excluding duplicates, the caster, its own
summons and all protected targets. These tie/geometry rules are local empirical
approximations pending official-game calibration; the official preview supports
forks from a fixed origin, not a sequential chain. Resolve and execute all forks
synchronously and revalidate targets before each hit, avoiding deferred owners.

## Current runtime

At task start Docker ran `canary-local:paladin-rebalance-20261029`, image
`sha256:9c88530096e413cc9d83b0b1e3ce7390338566b1378f7df5fc9d845634085ac6`.
This image was replaced by the druid deployment recorded below. Existing staged,
unstaged and untracked work was snapshotted under `/tmp/druid-rebalance-baseline`.

## Implementation checkpoint

Implemented in source: stance selection/persistence and dynamic Elemental
Synthesis; client spell metadata, official atlas mappings and stance HUD;
strong/ultimate strike ranges and power bounds; Strong Ice Wave base footprint,
cooldown and Wheel area selection; Wrath of Nature bounds; Nature's Embrace
level requirement; forked spell registration, fixed-origin targeting and effects;
spell Wheel augments and matching client node/summary text.

The enlarged Strong Ice Wave footprint currently uses `AREA_WAVE5` (15 tiles),
an explicit local approximation requiring live geometry calibration. Terra
Wave I now preserves its exact 6.5% bonus through declarations, augment data and
runtime combat. The same confirmed integer-rounding defect was corrected in
Groundshaker's 12.5% node and 6.5%-based gem; damage-percentage storage now retains
fractions, including upgraded gem grades.

Forked Spells use slots 6/21 (red 200 / blue 50), Heal Friend uses 11/26
(red/blue middle 100), and Strong Ice Wave uses 16/31 (red 50 / blue 200).
The installed-client comparison below corrects the initial legacy assignment;
numeric slot IDs remain unchanged. Both Forked nodes use official atlas cell 29;
both Heal Friend nodes use cell 33. Other druid cells retain their matching
identities. Full locked tooltips, summaries and both node copies are tested.
Nature's Embrace gem ID 60 remains the same perk; its client text now matches
the existing server's five-second reduction rather than promising ten seconds.

Shared Conservation, Blessing of the Grove critical healing, 25% Healing Link,
healing formula bounds and Mana Buffer are implemented. Derived heals carry
an explicit kind; sharing additionally carries a weak original-party identity
and rechecks membership, range, floor, sight, source state and recipient removal
after callbacks. Engine tests cover overhealing, lowest-HP selection, self-heal,
exact Grove thresholds, independent criticals, nonrecursive Link, both mage
vocations, surcharge timing, exhausted mana and Gift of Life ordering.

The old generic deferred chain still captures a raw Combat pointer. Forked
spells deliberately use a synchronous path; no new callback lifetime is added.
A future migration of the generic chain requires its own ownership/cancellation
work and must not reuse that raw-pointer scheduler.

## Shared prerequisite audit

| Area | Current evidence / status |
| --- | --- |
| Mana Buffer | Implemented for both mage vocations; engine and arithmetic boundary tests pass, including exhausted Buffer followed by Gift of Life. Live acceptance remains. |
| Rod/wand mana | Implemented generation using former configured cost, range 7 on all 66 wand/rod definitions, and Strike on existing slots. Zero-mana attack/generation and range tests pass. Per-item official numeric calibration remains. |
| Healing runes | Both existing IH/UH scripts accepted other players. Now reject non-self targets before combat; the existing ultimate-rune monk restriction remains unchanged. Lua regression passes. |
| Party experience | The old polynomial gave 30/60% and also reduced the bonus for larger parties. Two/three-vocation parties now receive 35/70% regardless of party size; other vocation-count behavior is retained. Lua regression passes at multiple party sizes. |
| Great/superior/distilled mana potions | Implemented: great mana for every vocation, new items/actions and 30 existing NPC vendors. Both great mana refill sources already use item 238 without a vocation gate. Level/vocation, mana bounds, flask/charge and refill tests pass. |
| Energy ring | Implemented 2 mana per absorbed HP for ring equipment shields; other sources remain 1:1. Engine tests cover odd mana and finite spell capacity. |
| Mitigation and Gift of Life mana | Existing sorcerer work already supplies shared Wheel mitigation and Gift of Life mana recovery; preserve and regression-test it. |
| AoE runes | Thunderstorm/Stone Shower now match the existing Avalanche/GFB 50-power formula; Explosion uses nine squares. Normal/high-level parity and footprint tests pass. Existing rune level scaling is preserved. |
| Charm targeting | Auto-attack extensions now suppress all charms away from the main target, including passive critical charms. Ordinary criticals and spell/rune eligibility remain intact. The engine test covers auto-attacks with no main target; selected-main-target live coverage remains. |
| Combat-mode compensation / mitigation | Locally implemented: mode-independent full attack, defense and mitigation; 30% shield/60% spellbook defense compensation; modern-client control removal with legacy-server UI compatibility. Not included in r1 or r2. Exact updated weapon-type and monster mitigation coefficients remain unverified. |
| Monk party buffs | Existing Mentor Other still supplies 5% druid healing; the newer Justice party rework is separate monk work, not implemented here. |

The combat-mode audit distinguishes protocol support from balance behavior.
The current protocol already has `TacticsWithoutFightMode` and forces offensive
mode in `ProtocolGame::parseFightModes`; legacy profiles still accept a mode.
The local follow-up removes mode factors from attack, defense and
`PlayerWheel::calculateMitigation`, and ignores legacy mode requests without
changing chase/safe-fight/PvP behavior. The modern client's inventory no longer
displays or restores mode controls; legacy-server behavior remains available.
The offensive attack display already included a 1.2 multiplier in `attackTotal`,
while weapon damage uses its own full-attack coefficients. The permanent bonus
therefore keeps those full-attack coefficients and does not multiply them again.
The official release specifies equipment compensation percentages but does not
publish replacement weapon-type or monster mitigation coefficients. Removing
the old mode factor is not a claim that the full mitigation rebalance is exact.

### Current validation

- Maintained `macos-release` server/test build passes. All **400 C++ unit tests**
  pass, including engine fork acquisition, exclusions, unique hit counts and Wheel target grades. The full
  suite required local-socket permission for its existing connection tests.
  Logs: `/tmp/druid-fork-build.log`, `/tmp/druid-fork-tests.log`, `/tmp/druid-all-unit-tests.log`.
- Druid stance, spell, shared-rule, Wheel and potion Lua checks pass, together
  with the sorcerer/paladin Wheel, HUD and text-rendering regressions. Potion
  XML and all 30 changed vendor scripts parse, with verified published prices.
- Isolated native OTClient passes 360 locked/unlocked node descriptions, all ten
  affected druid node copies, druid/sorcerer/paladin summary tooltips and twelve
  gem grade labels. Log: `/tmp/druid-native-wheel.log`. This is real native UI
  rendering, but is not a server-connected combat test.
- Official medium atlas and druid backdrop match byte-for-byte. SHA-256:
  medium `64a798e63ec8ee3fe2d23d46ebe57d9bb55eb441afd506e03801f810e925af5b`;
  backdrop `aa3b7ded2120eac314d71444f4215b786b2b6a1985ab9225e4c67da6408193b2`.
  Effects 42/287/288/324/325 match complete normalized animation records and
  underlying sprite bytes; audit result `/tmp/druid-effect-parity.json`.
- Server and client staged patches remain byte-identical to the initial
  snapshots; no commits were made. Three separate test accounts were added after backup; existing characters were not modified as fixtures.

Remaining: weapon-type/monster mitigation calibration, deployment of the local
follow-ups, interrupted normal/high-level live
healing/damage and native animation acceptance. Engine tests cover forked
selection, nearest acquisition, walls, floors, own summons, protected players,
removed targets, fixed-origin geometry, unique hits and both spell target limits
with Wheel grades. Actual rejected-cast resource behavior remains a live gate.

### Potion evidence

The installed official appearance records and the server/client appearance
files agree on item identities. Official flags 62/63 provide vocation and level
restrictions; NPC sale records provide vendor names and prices.

| ID | Potion | Level | Mana | Gold | Vocations |
| --- | --- | ---: | --- | ---: | --- |
| 53162 | Superior mana | 100 | 240–360 | 254 | Sorcerer, druid, paladin, monk |
| 53163 | Distilled superior mana | 130 | 240–360 | 381 | All |
| 53164 | Distilled ultimate mana | 200 | 425–575 | 732 | All |

Matching offers were added to the 30 existing NPC definitions found in this
map pack, including Hireling. The official vendors Minzy, Onfroi and Yerga have
no definitions in this checkout; no new NPCs or map spawns were invented.
Existing appearance assets already contain all three potions. All use the
existing large-flask behavior and 3.10 oz weight. Vendor evidence and file list
are recorded in `/tmp/druid-potion-vendors.json`.

## Healing and survival decisions (implementation contract)

The official announcements establish percentages and power means but do not
publish precise random bounds or these ordering details. The following are
explicit local approximations to be calibrated against official gameplay:

- Heal Friend uses power bounds 250–270 around the metadata mean 260;
  Nature's Embrace uses 1900–2100 around 2000. Both use the documented magic
  scaling/level bands and floor each bound. This narrows variance without
  inventing a claim of exact official coefficients.
- Shared healing uses 30% of nominal source healing after source buffs, spell
  augments and flat Wheel healing, before primary-target received-healing,
  Grove and critical bonuses. It therefore still works on an overhealed main
  target. It rolls its own Grove critical once; primary criticals never become
  input to a second critical multiplication.
- Choose the injured living party member with the lowest absolute current HP,
  including the caster and excluding the primary target. Ties use player GUID,
  then runtime ID. Use the caster's same-floor 7-by-5 viewport, clear sight and
  revalidation of party membership immediately before healing. Ordinary healing
  callbacks/black-skull restrictions apply; protection zones remain usable to
  the same extent as normal nonaggressive healing.
- Healing Link uses 25% of the primary nominal final heal, including its critical,
  and applies no new Wheel/stance bonuses or critical roll. It never propagates.
  Sharing and linking have distinct typed origins and cannot trigger each other.
- Shared Conservation's self bonus applies once to self-cast healing spells/runes
  and to its secondary heal when the caster is selected, not potions or Link.
- Grove bonuses are 5/7.5/10% below 60% health and double below 30%, using exact
  integer threshold comparisons. Self-healing spells participate. Only named
  instant healing spells roll critical healing (not potions, runes or Link).
- Mana Buffer runs after normal shields/damage modifiers and before Gift of Life.
  It preserves one HP by converting the remaining otherwise-lethal damage at
  10 mana per HP, including the one HP needed to stay alive. It charges ceil(25%
  maximum mana) at most once per 2000 ms. Mana equal to the total cost is exhausted
  and does not prevent death; Gift of Life may still intervene. The exact-equality
  hit, one-HP reservation and rounding are explicit local boundary decisions.

## Shared weapon and shield contract

The release notes confirm mana generation, increased range and removal of Strike
restrictions but do not publish the per-item numerical table. This implementation
uses the configured former rod/wand mana cost as mana generated per successful
attack, range 7 for all existing `weaponType=wand` items, and enables Strike in
existing imbuement slots without adding slots. These are explicit local balance
choices pending per-item official calibration. Generation does not train magic
as mana spent, and the legacy starter-weapon refund cannot double the gain.

Energy-ring equipment shields consume two mana per absorbed HP; other shield
sources retain one. An odd final mana point cannot absorb a full HP and remains
available. Mana-change callbacks run before calculating absorption and consuming
shield capacity; callback changes are bounded and rechecked. Finite spell-shield
capacity remains measured in absorbed HP. This preserves the documented ratio
without doubling the protection of spell shields.


## Local deployment and user playtest handoff — 2026-10-29

Running image: `canary-local:druid-rebalance-20261029-r1`.
Verified container image identity:
`sha256:1927a15d1c032fe2a96d2042da96583893765f44e0dc04e797ed0b5bcb32cf66`.
The previous paladin image remains the rollback reference. The ignored
`docker/.env` changed only `CANARY_IMAGE`; its store overlay was preserved.

The server was idle (zero online players), stopped gracefully, and backed up
before replacement. Backup: `/tmp/canary-before-druid-20261029.sql`, 410927 bytes,
SHA-256 `f38971a9bfaf7607660d8e965da8417ba78c1bc76051b46efceb816e75e3c5f6`.
The original environment is saved privately beside it as
`/tmp/canary-before-druid-20261029.env`. Both have owner-only permissions.

Startup completed and the temporary runtime probe passed stance bindings,
forked spell metadata, effect IDs, July cooldowns, Nature's Embrace level and
potion registration. The native executable was missing and was restored by
relinking its existing maintained `macos-release` cache. An isolated native
client then connected successfully as `Druid Probe Normal` using protocol 1525.

Separate normal-permission test characters were created: Druid Probe Normal
(level 150), Druid Probe High (2000), and Druid Probe Partner (500), each on its
own test account. Credentials remain in owner-only temporary files; they are
not included here. Existing characters were not used as fixtures.

Live acceptance did **not** pass: the first controlled fork cast was rejected.
A subsequent read-only diagnosis found the temporary accounts had `lastday=0`;
this server derives premium eligibility from that expiry, not `premdays` alone.
The premium-gated fork therefore failed before targeting. This is a test-fixture
error, not evidence of broken fork targeting. The fresh native profile also encountered an unrelated forge-module unload
error during first-locale module reload. These results require diagnosis; neither
registration nor the successful engine tests establish in-game acceptance.
The user then elected to test in game personally, so automated casts stopped.
The isolated client exited. Both temporary probe scripts were removed from the
container without restarting or interrupting the server. Their already-loaded
normal-player action (restricted to the named test characters) and isolated arena
at 60000,60000,7 remain in memory until the next server restart.

Logs: `/tmp/canary-druid-docker-build.log`, `/tmp/canary-druid-startup.log`,
`/tmp/druid-live-results.log`, `/tmp/druid-native-live.log`.


### Follow-up source correction (not in deployed r1)

The numeric-target branch of `Combat:execute` ignored the engine's boolean result
and returned success even when a fork target was removed before application.
Both numeric-target dispatch branches now propagate their result, as position
and string variants already do. An engine/Lua regression tests a valid target
followed by the same registered target marked removed. The focused healing,
Mana Buffer and Lua dispatch checks pass (18 tests). The full suite passes all
401 tests with local loopback permission and exits cleanly; log:
`/tmp/druid-all-unit-tests-followup.log`. The running server will not be restarted
during the user's playtest.

The startup callback warning was traced to Strong Ice Wave's enlarged combat:
named callback registration consumes the global function, so its second
registration had no formula. Both combats now register the shared local formula
by re-exposing it immediately before registration. The Lua harness models this
consumption, validates both combats at ordinary/high levels, and exercises the
Wheel-expanded execution path and failure propagation. Those checks pass. An
audit of repeated named registrations across spell scripts found no siblings.
This correction is also **not deployed in r1**.

A further engine regression calls `InstantSpell::playerCastInstant` for both
fork spells at levels 150 and 2000 with no legal target and verifies unchanged
mana, individual cooldown and attack-group cooldown. It also aims at an empty
tile with a legal nearby creature and verifies that the cast does not fall back
to that creature. A completed-cast accounting control verifies that the fixture
does normally charge 180 mana and apply both cooldowns. This test passes; live
client/server resource acceptance remains with the user's playtest.

With that regression included, the maintained host build and all **402 unit
tests** pass and exit cleanly (`/tmp/druid-final-offline-tests-rerun.log`). An
earlier full run had a timing failure in the unrelated
`ConnectionWriteDiagnosticsTest.CompletionRetainsNextMessageDuringPreparation`:
the immediate socket availability assertion saw 3 bytes instead of 6. No
networking code or test was changed; the subsequent full run passed. The earlier
result is retained in `/tmp/druid-final-offline-tests.log`. All thirteen druid,
sorcerer/paladin spell, stance, Wheel, HUD, potion and aiming Lua checks also pass.
Both repositories' staged patches still match the initial snapshots byte-for-byte.

### Prepared spell-fix image r2 — not deployed

`canary-local:druid-rebalance-20261029-r2` built successfully using the maintained
local Dockerfile. Image ID:
`sha256:abebcc60a3716d25547b1379bb9bd96dea055edcae7d097f18cda4946d7c5cd5`.
Log: `/tmp/canary-druid-r2-docker-build.log`.

A temporary container with networking disabled verified the packaged Strong Ice
Wave script SHA-256 matches the checked source:
`cfe9264cd3f7a7eef6f5be91f775513a6dad8f45d4fcf292b1bd1e0098e1e7d2`.
Packaged server binary SHA-256:
`03f51b157f8a1468a4ee5c69b8d39c60752c521db5d7801f2887f61c87ec739a`.
Compose still reports the running server as the original r1 image identity
recorded above. No configuration change, database mutation or server restart
was performed while preparing r2. Deployment is awaiting the end of the user's
current playtest session; fresh backup and startup/registration checks remain
mandatory when replacing the server.

The Lua-userdata review checks found only pre-existing legacy registrations, the
unused weak-metatable helper declaration/definition, and the typed borrowed helper
with its no-op deleter. This correction adds only a typed `Combat` userdata in
the regression and uses numeric creature identities; no legacy registration or
shared ownership behavior was changed. All five Lua combat dispatch calls now
propagate their engine result.

### Combat-mode follow-up — packaged in r3, not deployed

Legacy fight-mode requests now normalize to the permanent offensive state.
Weapon formulas keep their full-offensive coefficient; the attack display
always includes its existing 20% equipment bonus. Both attack display paths
bound extreme totals before narrowing to the protocol's 16-bit field. Defense
no longer drops while attacking, and mitigation no longer changes with mode.
This applies to all vocations without altering their stance mechanics.

The item definitions now include the announced compensation for 80 shields
and 21 spellbooks. Values are rounded once to the nearest whole defense point;
the official release does not specify rounding, so this is a local decision.
Examples: wooden shield 14→18, mastermind shield 37→48, spellbook 14→22, and
spellbook of ancient arcana 19→30. Non-equippable decorative book definitions
and custom per-instance item overrides remain unchanged. Values are changed in
the item catalog rather than multiplied again during combat, so item inspection,
defense and mitigation consume the same catalog value. The complete pre/post
audit is `/tmp/druid-defense-compensation.json`.

OTClient uses `GameTacticsWithoutFightMode` to hide the three obsolete controls
in both inventory layouts, ignore saved modes and stop persisting them. Chase,
safe-fight and PvP settings remain intact. Legacy-server controls and saved modes
are preserved. `tests/lua/test_combat_modes.lua` runs the actual inventory Lua
module with widget doubles across modern/legacy and compact/expanded cases.
It is a logic test, not native visual acceptance. No extra client was opened
while the user was playing.

The maintained host build and full 403-test run pass
(`/tmp/druid-combat-modes-tests.log`). After adding the matching bound to the
raw attack display and including monk in the regression, the final host build
and focused mode/boundary test also pass. The regression covers all three legacy
mode requests for promoted druid, sorcerer, paladin, knight and monk. Final build
log: `/tmp/druid-combat-modes-final-build.log`. All fourteen Lua regressions pass; log:
`/tmp/druid-combat-modes-lua-tests.log`. Native rendering, deployment, and live
combat acceptance of this follow-up remain pending. The running r1 server and
database have not been changed.

### Prepared replacement image r3 — not deployed

`canary-local:druid-rebalance-20261029-r3` includes both r2 spell fixes and the
combat-mode/equipment follow-up. Its local Docker build completed successfully;
log: `/tmp/canary-druid-r3-docker-build.log`. Image ID:
`sha256:f8fbc8a2e83d596741b6eb98105880d78af6b6dcf077cd807bd081a6ead470ab`.

A network-disabled temporary container verified these packaged hashes:

- `data/items/items.xml`:
  `b9e327177ad72b8b8ee184fc026c1030741873625b491b9046fbdb83c1ceaa2f`.
- Strong Ice Wave script:
  `cfe9264cd3f7a7eef6f5be91f775513a6dad8f45d4fcf292b1bd1e0098e1e7d2`.
- Server binary:
  `d9ebebe191ef22489090bc6cd50de998b719f1ed0aff2a988422ccf737dc7020`.

Both data hashes match the checked source files. Compose still reports r1 at
`sha256:1927a15d1c032fe2a96d2042da96583893765f44e0dc04e797ed0b5bcb32cf66`.
r3 supersedes r2 as the prepared candidate, but has not been started against
the game database. Fresh database backup, startup/registration checks and native
live acceptance remain required once the user's current test session ends.
The read-only startup probe for that replacement is prepared at
`/tmp/zz_druid_r3_runtime_probe.lua` and passes Lua syntax validation. It checks
spell/binding registration and representative compensated item defenses; it has
not been installed or executed in the running server.

## Installed-client Wheel position comparison (2026-10-29)

The Applications Tibia launcher uses the installed game package at
`~/Library/Application Support/CipSoft GmbH/Tibia/packages/Tibia.app/Contents/MacOS/`.
The original installation was inspected read-only. The executable SHA-256 remains
`303d6565e09186bb53bdda88e85e79f2b176e4d553b65587390835130c9feb79`;
its graphics archive remains
`91aa4a771b82be6114144d3f67ff8d5d529bae73fb8975352e358455bfb30c68`.

The executable's 36-row Wheel table is at file offset `0x237cd94`, with
44-byte rows: maximum points, five dedication IDs, then five conviction IDs
(Knight, Paladin, Sorcerer, Druid, Monk). The druid conviction is at row offset
36. Accessors at `0x101145190`, `0x1011451f0` and `0x101145230` establish
row width and column indexing. The perk-name jump table at `0xfb0878` identifies
29 as Forked Spells, 31 as Strong Ice Wave and 33 as Heal Friend. Grid-name data
at `0x237c95c` independently identifies slot 31 as QBL8 (outer bottom-left)
and slot 16 as QTR0 (inner top-right). Slots below are 1-based protocol IDs.

| Perk | Previous incorrect slots | Official/corrected slots |
| --- | --- | --- |
| Forked Spells | 11, 26 | 6 (outer top-right), 21 (inner bottom-left) |
| Heal Friend | 16, 31 | 11 (middle top-right), 26 (middle bottom-left) |
| Strong Ice Wave | 6, 21 | 16 (inner top-right), 31 (outer bottom-left) |

The original implementation incorrectly preserved legacy spell assignments while
updating their effects. Server grants, client selected/hover text, summaries and
both copies of each icon now follow the official positions. Numeric slot IDs,
point allocations, gem IDs and spell-grade effects remain unchanged. Both
Strong Ice Wave nodes are required for grade II's enlarged area. Existing saved
points receive the perks of the corrected positions.

The Lua regression pins all 36 druid icon IDs to the independent installed
reference and checks the two Ice Wave positions explicitly. Server tests cover
full/partial node grants and both Forked Spells copies. The native UI fixture's
expected positions were updated; it has not been rerun in this session.

### r3 playtest deployment completed

The previous "not deployed" entries above describe the state before the user's
explicit startup request. r3 is now running with image ID
`sha256:f8fbc8a2e83d596741b6eb98105880d78af6b6dcf077cd807bd081a6ead470ab`.
The startup registration probe passed and Damian Druid logged in with protocol
1525. Character ID 192 belongs to existing account 117; existing characters were
preserved. Ankrahmun town/spawn is 10 / 33194,32853,8.

The database backup before replacement is
`/tmp/canary-before-druid-r3-playtest-20261029T213146Z.sql`, SHA-256
`c01ec14f068a27375be1e28b9ec351f41bb3690b18e7d9138f5e60bdf7b82d11`.

### Wheel correction deployed in r4

The maintained macOS release build passed. Eight focused native tests and four
Lua Wheel suites (druid, sorcerer, paladin, shared text rendering) passed. All 36
druid node icons, dedication types and maximum-point values match the installed
reference. The isolated native UI fixture was updated but not run; live UI
acceptance remains with the user.

The Docker image `canary-local:druid-wheel-layout-20261029-r4` is running:
`sha256:a98d7fe64a07812139d25b65dfaf8e8ea05edcaf8c9ab74d7fef35fa34bcfbfa`. Startup logged the r4 registration probe PASS
and server online at 22:28:30 UTC. No characters were online before the graceful
replacement. Damian Druid remains level 2000, vocation 6, magic level 150.

Backup: `/tmp/canary-before-druid-wheel-r4-20261029T222825Z.sql`.
SHA-256: `b061f3bbe0b9186781b8932cd0269567817d9f1b101fce40518c348f19fb5cfa`.
The original Tibia executable/archive hashes were rechecked and remain unchanged.
