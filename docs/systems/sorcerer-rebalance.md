# Sorcerer rebalance implementation

Target: the TibiaWiki vocation rebalance, reviewed 2026-10-28. This is an
incremental implementation; the current source is **not a complete rebalance**.

## Sources and conflicts

Use the [Sorcerer inventory](https://tibia.fandom.com/wiki/Sorcerer) and
[rebalance-specific update](https://tibia.fandom.com/wiki/Updates/15.25.3a4a52).
The older Lightning and Strong Energy Strike pages still contain pre-update
values; the explicit rebalance entries take precedence for those changes.
[Great Death Beam](https://tibia.fandom.com/wiki/Great_Death_Beam) supplies its
current requirements and cooldown. Follow [Death Echo](https://tibia.fandom.com/wiki/Death_Echo)
for its 150-mana cost, rather than the announcement's 155.

The live wikitext for [Master of Flames](https://tibia.fandom.com/wiki/Master_of_Flames)
specifies **4% base damage**, correcting the cached Sorcerer table's critical-chance
description. [Thunder](https://tibia.fandom.com/wiki/Master_of_Thunder) grants four
percentage points of critical chance; [Decay](https://tibia.fandom.com/wiki/Master_of_Decay)
grants 30 percentage points of critical damage. Spell IDs are 304–306.

## Implemented in source

- Lightning and the four strong/ultimate fire/energy strikes: range 7.
- Great Death Beam: level 66, premium Master Sorcerer, six-second base cooldown,
  eight-tile beam, no Wheel-unlock requirement. Existing damage formula retained.
- Matching OTClient spell metadata, including normal spell-list availability.
- Three elemental stance spells, their shared cooldown and saved selection.
- Player-owned conversion state: a natural-element cast charges conversion;
  the next eligible other-element cast consumes it. Each complete cast uses one
  decision. Failed casts do not commit state. Switching, death and login clear
  pending conversion. Healing, runes and weapon attacks do not consume it.
- Direct spell damage, projectiles and tile effects use the converted element.
  Damage conditions are cloned with the new element without changing shared
  templates. Natural-element bonuses apply to direct spell damage; existing
  damage-over-time ticks retain their scheduled values and do not gain criticals.
- Converted fire waves use effects 331/332, verified present in the installed
  15.25 appearances. Other converted spells use existing elemental effects;
  legacy clients receive compatible wave effects.
- Client spell-list/hotkey entries, Stance group cooldown, and authoritative
  active-stance badge. Ping/FPS moves below the badge while it is visible.
- Normal, cursor, target and crosshair cast modes, saved per action. Explicit
  directional aim uses the dominant cardinal axis (vertical on ties) without
  turning the character or using its locked target.
- Death Echo: fixed 5x5 location, delayed half-strength repeat after one second,
  and no charm proc on the repeat. Weak owner/spell identity plus death generation
  invalidate stale work after removal, death, replacement or spell reload.
- Official stance and Death Echo icons on separate atlases; existing icon order
  is preserved. Death Echo uses effects 321/322/323 and stance casts 5/287/285.
- Spell assignment uses the styled HTML select control, restores the selected
  spell preview, and discards stale radio selections when rebuilding Show All.

These changes affect the shared datapack, including legacy runtime profiles.
Ordinary spells are available by default with `toggleLearnSpells = false`.
Explicit Wheel/quest unlocks and level, vocation, mana and premium requirements
still apply. The Sorcerer Wheel replacements are implemented with unchanged persisted slot and gem IDs.
The sorcerer changes were deployed as `canary-local:focus-cooldowns-20261028`, including the Wheel changes, shared cooldown resynchronization and stance-specific Focus effects. That image has since been superseded by the [paladin deployment](paladin-rebalance.md#local-deployment-2026-10-29), which retains these changes. A database backup was saved before each deployment.

## Remaining work

| Work | Acceptance criteria |
| --- | --- |
| Damage formulas | Verify base-power scaling; targets: Lightning 110, strong strikes 125, ultimate strikes 210, death beam 155 (July adjustment). Do not silently approximate coefficients. |
| Lightning chaining | Three targets total; verify jump distance, selection, walls and PvP rules. Fix deferred combat ownership before reusing the existing chain scheduler. |
| Elemental stances | In-game verification of formulas, condition behavior, persistence and effects. Active indicator and official icons are implemented. |
| Crippling auras | One aura alongside an elemental stance; cover spells, runes and auto-attacks, refresh/stacking and PvP rules. |
| Death Echo | Engine replay is implemented; live combat, damage scaling, cancellation and caster/session lifetime still need end-to-end playtesting. |
| Mana Buffer | Verify lethal-hit handling and interactions with shields, death prevention and PvP. |
| Proficiency integration | Validate equipment-specific bonuses alongside the updated Wheel. Sorcerer Wheel perks and augments are implemented. |
| Client presentation | Exact projectile/sound parity and live interaction checks. Stance synchronization, official icons, hotkeys, assignment controls and cooldown display are implemented. |

## Validation

Run `luajit tests/lua/test_sorcerer_spell_contract.lua ../otclient` from the
Canary root. It checks spell loading, beam dispatch without Wheel APIs, combat
failure propagation, and server/client metadata consistency. It does not replace
engine integration tests.

Also run `luajit tests/lua/test_elemental_stances.lua ../otclient` and the C++
`ElementalStance.*` tests. The latter cover all conversion pairs, natural bonuses,
failed casts, multi-target decisions, separate players, nested casts, state
invalidation and cloned damage conditions. The native `macos-release` preset was
configured with `-DCANARY_BUILD_TESTS=ON -DVCPKG_MANIFEST_FEATURES=tests` for this work.
The native server build, Lua contract checks and all 372 C++ unit tests passed.
The Wheel Lua regression covers all five Sorcerer spell augments, including
Focus Spells at +5% base damage and both grade-II cooldown reductions of 4 seconds.
Its node and summary tooltips previously displayed +8%; both now match the server.
`tests/lua/test_wheel_text_rendering.lua` exercises the actual selected/hover
handlers for all five vocations, summary tooltips through the UI BBCode parser,
and gem descriptions. Wheel text/color tables are converted at the UI boundary
to the strings required by the native renderer; passing tables directly caused
blank Conviction Perk text and invalid tooltip content. The assembly helper is
local to each Wheel source file because gamelib replaces the global
`setStringColor` with a different, non-appending function. The Lua tests load
the real gamelib utilities instead of substituting that helper.
`tests/lua/test_wheel_native.lua`, run as `otclientrc.lua` in an isolated client
working directory and `--user-dir` profile, passed 360 locked/unlocked node
checks through the actual native renderer plus summary tooltip checks. It
explicitly verifies both Focus Spells and both Death Echo nodes.
The full unit run requires local socket access for the existing network tests.
Live startup verification passed for the stance bindings, registered spells,
shared cooldown groups, beam settings and ranges. HTTP login, game TCP and XML
status checks passed on the deployed Linux ARM runtime. Client visual and combat
playtesting remain outstanding.

Manual sequence: select `uteta mort`, cast `exori mort`, then `exevo vis lux`.
The beam should deal death damage and show death effects. The following energy
beam should be energy again until another death spell charges conversion.
Repeat for `uteta flam` and `uteta vis`, and with multiple creatures in a beam.

Before deployment, test a premium level-66 Master Sorcerer with no Wheel points;
check level/promotion/premium rejection on ordinary accounts. Check targets at
distances 7 and 8, wall blocking, and shared cooldowns. Repeat with Wheel bonuses.
Future C++ changes require the maintained build workflow and a custom runtime
image: editing this checkout does not rebuild the published Docker executable.

## Local Docker deployment

Build the development runtime from the repository root:

```sh
docker build -f docker/Dockerfile.local -t canary-local:focus-cooldowns-20261028 .
```

This uses the pinned vcpkg manifest and `linux-release` preset without CI cache
credentials. It builds for the Docker host architecture and limits compilation
to two jobs. Keep the default quickstart image unchanged; set `CANARY_IMAGE` in
the ignored `docker/.env` to the local tag for this development instance.
Preserve the existing `COMPOSE_FILE` store overlay.

Before replacing the server, stop it gracefully and save a database dump. Then
run `docker compose up -d --no-deps server` from `docker/`. Check startup logs,
the status service on port 7173 and login at `http://localhost:8088/login`.
Restart OTClient to load its updated spell metadata and cooldown widgets.

## Official installed assets audit (2026-10-28)

The official installation under `~/Library/Application Support/CipSoft GmbH/Tibia/packages/Tibia.app/Contents/`
was read only. Embedded spell metadata, preview animations, translations and
`MacOS/graphics_resources.rcc` were extracted to temporary files. The client and
resource archive hashes were unchanged after inspection. Their SHA-256 values:

- client: `303d6565e09186bb53bdda88e85e79f2b176e4d553b65587390835130c9feb79`
- graphics_resources.rcc: `91aa4a771b82be6114144d3f67ff8d5d529bae73fb8975352e358455bfb30c68`

| Spell | Official icon index | Impact effect |
| --- | ---: | ---: |
| Master of Flames (304) | 187 | 5 |
| Master of Thunder (305) | 188 | 287 |
| Master of Decay (306) | 189 | 285 |
| Death Echo (310) | 192 | 321; elemental variants 322/323 |

Both the installed official appearances and OTClient's 1525 appearances contain
these effects and every referenced sprite sheet. The Echo sheets were already
installed: the missing visual was caused by using a generic effect. The official
201-icon atlas rearranges existing slots from index 109 onward, so replacing the
old 187-icon atlas would corrupt unrelated icons. Only the four new spell IDs
use the copied official atlases. Great Death Beam retains its old atlas mapping.

The preview animation contains a two-second Echo delay, whereas the released
mechanic specifies one second; preview timing is not treated as authoritative.
Embedded metadata confirms base power 75 for Echo and 155 for the death beam,
but does not establish the server's precise damage-formula coefficients.
It also marks stances as promoted-only and Great Death Beam as not promoted-only;
those conflicts with existing server requirements remain for separate review.

## Sorcerer Wheel investigation

Installed translations confirm Lord of Destruction, side beams for Beam Mastery,
Focus Mastery's group-cooldown reduction, and the new spell augment families.
The numeric changes below come from the official
[rebalance announcement](https://www.tibia.com/news/?id=8833&rejectcookies=0&subtopic=newsarchive)
and subsequent [July adjustments](https://www.tibia.com/news/?id=8872&subtopic=newsarchive).
The later values take precedence.

| Change | Benefit |
| --- | --- |
| Lord of Destruction replaces Drain Body | Stages I/II/III add 2/3/4 percentage points to Flames damage and Thunder critical chance, and 15/22.5/30 points to Decay critical damage. Totals become 6/7/8%, 6/7/8%, and 45/52.5/60%. |
| Beam Mastery | Adjacent side beams deal 25/40/70% at stages I/II/III after the July reduction. Existing central-hit bonuses remain described in the client. |
| Death Echo augments replace Sap Strength | I reduces cooldown by two seconds; II increases damage by 12% after July. |
| Special Spells augments replace Magic Shield | Lightning and both strong elemental strikes gain four-second cooldown reduction at I and 50% damage at II. |
| Focus Mastery | Focus spells reduce their spell-group cooldown by two seconds. |
| Energy Wave augments | I enlarges the area; II adds 10% damage. |

These changes are now implemented in the server and OTClient. Slot numbers,
stage values and gem modifier IDs retain their serialized identities, so existing
allocations map to the replacement perks. The old Drain Body spell upgrade is
no longer granted, and Great Death Beam no longer receives obsolete stage-based
cooldown/damage bonuses from Beam Mastery.

Additional client regressions are covered by
`tests/lua/test_spell_hud.lua` (state, ping spacing, icons, logout) and
`tests/lua/test_spell_assignment.lua` (selection, preview, modes, Apply/OK and
Show All). These load the real Lua handlers with UI stubs; they do not replace
an in-game visual check of dropdown rendering or combat.

## Stance feedback message fix

Stance activation and login restoration must use `MESSAGE_STATUS`, which this
server exports to Lua. `MESSAGE_STATUS_SMALL` is not exported and became
`MESSAGE_NONE`, triggering the administrator error. The stance test now imports
message names from `lua_enums.cpp` and validates the message type for both paths,
instead of defining a nonexistent constant in its fixture.

## Sorcerer Wheel migration

- `IOWheel` grants Special Spells to Lightning, Strong Energy Strike and Strong
  Flame Strike from both former Magic Shield slots, and Death Echo from both
  former Sap Strength slots. Grades accumulate through the existing spell engine.
- Lord of Destruction uses the former Drain Body stage/gem numeric identities.
  Natural-element bonuses are sampled once per cast; converted spells do not
  acquire those bonuses. Removing the old pseudo-spell grant also removes its
  leech upgrade from Sap Strength/Expose Weakness.
- Beam Mastery retains the five-tile Energy Beam and eight-tile great beams,
  and expands perpendicular to the cardinal axis. Side tiles
  pass sight, floor-change, tile and PvP checks. Their damage is 25/40/70% of the
  initial damage; only central targets count toward the capped three-target
  damage increase and cooldown reduction. Central-target damage is determined
  for the entire cast, independent of tile iteration order.
- Focus Mastery reduces the primary group cooldown of Sorcerer focus spells by
  two seconds, separately from focus-spell augment reductions.
- Shared Wheel mitigation is 0.075% per point; mitigation gems grant
  20/22/24/30%. Gift of Life also restores 20/25/30% of maximum mana. These shared
  definitions apply consistently to all vocations.
- OTClient uses copies of the official Sorcerer background and perk atlases,
  with updated slice descriptions, summary, stage tooltips and gem labels.
  Special Spells uses atlas cell 26 (nodes 8/24); Death Echo uses cell 36
  (nodes 11/26). The replacement atlas cannot reuse the old pixel mappings.
  All five Sorcerer spell-augment nodes expose their full tooltip before unlock.
  Other vocation artwork retains the existing atlases. The official installation
  remains read-only.
- Cyclopedia augment reporting matches grouped spell names and sends cooldown
  reductions in seconds, correcting the former accidental extra division by 1000.

Validation: `SorcererWheel*` native tests exercise natural/converted bonuses at
all stages, cardinal side geometry and coordinate boundaries, saved slot mappings,
cumulative reported augments and mitigation. `test_sorcerer_wheel.lua` exercises
actual client handlers for slices, summaries, stages and other-vocation isolation.
The client tests use UI stubs; live visual/combat playtesting remains distinct.

## Focus spell cooldowns and stance animations

- Rage of the Skies and Hell's Core now declare Focus group 7 (40 seconds) in
  OTClient; the analogous Druid focus spells use the same shared group.
- Action buttons restore the longest remaining individual/shared timer after a
  refresh. Server updates can shorten timers; shorter Attack/Support timers do
  not erase a longer Focus/Avatar timer. Hidden cooldown visuals still retain
  received timers. Multi-action scheduling uses received durations, not the
  unmodified spell catalogue duration.
- Rejected casts resynchronize active cooldowns, so a long Avatar cooldown is
  visible even after a missed initial update. Damian Sorcerer's saved condition
  was spell cooldown 266 with 389000 ms remaining when inspected; this was a
  legitimate Avatar timer, not a failure to unlock the spell. Its cooldown was
  not removed or shortened by this fix.
- Rage's converted fire/death cloud effects are 339/340; Hell's Core uses the
  energy/death explosion variants 337/338. Natural effects remain 41/7.
  Official and OTClient assets have different global sprite numbering, but
  normalized sheet slots, sprite bytes, dimensions and animation metadata for
  all six effects match. Legacy clients receive compatible fallback effects.
- `tests/lua/test_focus_cooldown_native.lua` runs the real action-bar handlers and
  native countdown widgets in an isolated profile. It covers both Focus spell
  directions, refresh, restored Avatar timers, shorter support cooldowns,
  authoritative reductions, expiry and disabled visuals. The native test passed.

## Readable Wheel grades

Perk prefixes use ASCII `I:` and `II:`; gem grades use `(I)` through `(IV)`.
The isolated native widget regression covers 360 locked/unlocked node descriptions,
summary tooltips and all 12 gem quality/grade combinations without replacement glyphs.
