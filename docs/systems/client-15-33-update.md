# Tibia 15.33 content integration

This change imports the installed official **15.33.8791c6** item metadata and
proficiency trees, adds Moonsilver equipment and spawnable creatures, and changes
the current protocol profile's accepted version to **1533**. The updated local
OTClient has passed programmatic login, movement, proficiency and creature-art
checks. Official-client runtime compatibility remains unverified; its installed
Mac application was neither modified nor launched.

The original client installation is read-only. Only `appearances.dat` and
`proficiencies.json` were copied into the repository. Their source names, SHA-256
hashes, item/proficiency mappings, and creature look types/colors/addons are in
[`client-15.33-manifest.json`](../../data/items/client-15.33-manifest.json).
The separately authorized OTClient update copies matching assets into its
standard versioned directories and includes the 15.33 static-data schema and
homing-perk UI. Existing maps and spawn files remain unchanged.

## Equipment and proficiency

Item IDs **53207–53228** are eleven Moonsilver weapons and eleven Stellar
variants. IDs **53229–53233** are the five helmets. Weapon level requirements are
1000; helmet requirements are 800. Vocation, slot, imbuement and weapon
registration use the existing XML loaders. Stellar equipment is available to GMs;
this change does not implement an upgrade quest or forge recipe.

Proficiency bonus type 32 has `Probability`, `Multiplier`, `ElementId` and
`MissileId`, rather than a `Value` field. The loader, persistence and runtime
recognize it. A selected missile perk rolls once after a successful offensive
attack-group spell, including runes. It chooses a visible, legal target on the
same floor within the 7-by-5 tile viewport, with line of sight. Base damage is
200% of level for Moonsilver and 300% for Stellar; the official tree supplies
the 1% chance. There is no scheduled owner capture. Permission and live-object
checks run again after blocking callbacks. The extension suppresses charm
procs and critical recursion and preserves normal player-target damage halving.
The exact official search radius and rune eligibility remain unconfirmed.
Fractional Stellar spell bonuses retain percentages such as 7.5%.

Wand/rod basic attacks currently use **103–127 damage and 23 mana** as explicit
server tuning. Published equipment attributes were used for bonuses and
protections; the client assets do not encode these basic attack values. Do not
represent the mana/damage pair as an independently verified official contract.

## Creatures without maps

The scripts under `data-otservbr-global/monster/quests/moonsilver/` register 21
monsters and six bosses. Use the existing GM command, for example:

```text
/m Radiant Warden
/m Devoted Radiant Warden
/m Moonspawn Blightspitter
/m The Moonsnow Magnolia
/m Mimar Haffar
/m Maior Domus
/m Phosphorus
```

No automatic spawns, teleports, arenas, quest access, cooldowns or phase mechanics
are installed. These are standalone combat versions for development. They do
not award Bosstiary progress for the simplified encounters. Phosphorus's test
loot includes base Moonsilver weapons; Maior Domus's includes helmets, at
server-defined rates. Stellar variants are not added to loot.

[`client-15.33-creatures.json`](../../data/items/client-15.33-creatures.json)
records each script, identity and HP/XP source. The official creature library,
accessed through TibiaData v4 on 2026-10-30, supplied HP/XP for twenty monsters.
True Feverbloom Asura uses 8,600 HP and the October 20 updated 8,600 XP.
The Moonsnow Magnolia uses 52,000 HP and 240,000 XP for its first form.

Unpublished values are **server tuning**, not an official reconstruction:

| Boss | HP | XP |
| --- | ---: | ---: |
| Mimar Haffar | 450,000 | 3,500,000 |
| Maior Domus | 500,000 | 3,500,000 |
| Phosphorus | 1,000,000 | 16,000,000 |
| Moonspawn Juggernaut | 80,000 | 0 |
| Rakesh Moonfang | 25,500 | 25,500 |

Attack ranges/damage/chances, most armor and speeds, resistance magnitudes,
loot chances/counts, and creature-product weights are server tuning throughout.
Verified strong/weak elements guide normal monster resistance signs. Corpse
art was inspected against the installed sprite sheets. Devoted variants share
base remains; Rakesh uses Jaracal remains and Juggernaut uses Oozecrown remains
until exact mappings are verified. Fresh corpses currently disappear after ten
minutes rather than traversing unverified decay stages. These limitations must
stay visible when changing or publishing the data.

## Protocol and validation boundary

The current profile resolves 1533 through account login, game login and status
advertisement. Legacy profiles retain their existing identities. Existing
feature gates and conservative build-specific packet tails are retained.
Build `15.33.8791c6` requires the trailing proficiency-detail count in `0xC4`;
the server now sends zero when there are no details. Omitting it causes the
client to consume the next opcode as the count. Unknown builds retain their
previous behavior. The local OTClient sends build `1533` and retains its short
C4 payload. Its proficiency window and all 22 new weapon replies passed live
checks; official-client refinement UI remains outside runtime verification.

The contract was verified by static, read-only inspection of the installed
x86_64 client (SHA-256
`303d6565e09186bb53bdda88e85e79f2b176e4d553b65587390835130c9feb79`).
The legacy-byte translator dispatches `0xC4` to reader `0x100f169f0`:
`u16 weapon, u32 experience, u8 selectedCount, (u8 level, u8 index)[]`,
followed unconditionally by `u8 detailCount` at `0x100f16a99`.
Nonempty details contain `u8 level, u8 index, u16 shapingId, u8 value`.
The adjacent proficiency notification (`0x5C`, reader `0x100f17dc0`) retains
its weapon/experience/boolean shape. Packet regression tests exercise the
production `0xC4` writer with empty and selected trees, preserve the next
opcode boundary, and retain unknown-build and legacy-profile behavior.

Host validation:

```sh
python3 tests/client_1533_data_test.py build/macos-release/vcpkg_installed/arm64-osx/tools/luajit/luajit
cmake --build --preset macos-release --target canary canary_ut --parallel 3
build/macos-release/tests/unit/canary_ut --gtest_filter='Moonsilver*:WeaponProficiencyPacketTest.*:ProtocolProfileRegistryTest.*:ConnectionTransportTest.*:SessionHintTest.*'
```

Validation on 2026-10-30: the macOS release build and 26 focused C++ tests
passed, including real item/weapon/equip registration and homing-missile combat.
The data audit passed for all 27 equipment definitions and 27 creature scripts.
The Linux ARM64 image `canary-local:moonsilver-1533-20261030-r2` built successfully,
including the verified proficiency packet tail.
An isolated Docker server using a fresh database and a read-only copy of the
existing map reached the online state, advertised 15.33, and passed native Lua
startup assertions for all 27 creature look types, addons and health values.
No Lua errors were found in that startup log. Both copied asset hashes matched
inside the running container. The server executable SHA-256 was
`746c1f49292356c216f6c0166290a650c42ad3fd23d271c4b49aec73d0374559`.
The local OTClient also passed both legacy/new native asset loading, HTTP/game
login, movement, all 22 new weapon proficiency replies, a visible Stellar tree,
and rendering of Radiant Warden, Iceplume Strider and Phosphorus. Saved 1525
server entries now adopt the configured 1533 protocol on startup and same-host
selection. A real stale-profile login through localhost:8088/7172 passed.

Before the shaping deployment below, the main local server ran
`canary-local:moonsilver-1533-dust4600-20261030`. Its executable SHA-256 was
`f49819d26e8e90ee1d0e19f201bbd562c3d5280bcec44dc71def26bf1e86224c`.
The running config has `forgeMaxDust = 4600`, and both copied item asset hashes
match the manifest. Startup registration and final main-server login passed.
The isolated validation containers were stopped after use.

## Local character grant

At the user's request, Damian Knight (player 193) has dust balance/capacity
4600/4600 and mastery on all 666 proficiency-enabled weapons. Exact XP caps
came from the running engine on an isolated test character. Native protobuf
integer records and mastered flags were verified in the main database after
restart; existing selected perks were preserved. The temporary startup grant
hook was removed from the final deployment.

Before mutation, the server shut down cleanly and a complete backup was saved
as `database_backup/before-dust4600-mastery-20261030.sql` (ignored by Git), SHA-256
`a00371804f4d72945586a7d386ecae77576901bcbac00155a9487e92fd2e7a19`.
This personal grant is local database administration, not a new default for
other characters. Market, official-client refinement, full boss encounters and
new maps were not certified by these checks.

Sources: [official creature library](https://www.tibia.com/library/?subtopic=creatures),
[TibiaData creature API](https://docs.tibiadata.com/),
[Summer 2026 equipment index](https://www.tibiawiki.com.br/wiki/Updates/15.30.a30dad),
[The Moonsnow Magnolia](https://www.tibiawiki.com.br/index.php?stable=1&title=The_Moonsnow_Magnolia),
[Maior Domus](https://www.tibiawiki.com.br/index.php?stableid=443101&title=Maior_Domus),
[Mimar Haffar](https://www.tibiawiki.com.br/index.php?stableid=440777&title=Mimar_Haffar),
[Phosphorus](https://www.tibiawiki.com.br/index.php?stableid=440685&title=Phosphorus).

## OTClient proficiency artwork and shaping

The OTClient perk types 28–31 now match their persisted meanings: Alpha Strike,
Omega Strike, Armor Penetration, and Elemental Piercing. Artwork uses the copied
15.33 strips at their native 32-pixel cell size, with explicit spell IDs from the
installed client's spell metadata. `data/proficiency-artwork-15.33.json` in the
OTClient repository records source dimensions and SHA-256 hashes. Unknown
identities fail the artwork audit rather than silently displaying the first cell.

Shaping UI follows read-only QML extracted from official build 15.33.8791c6:

- Shape (`21b83af`): 500-pixel dialog, perk frame on the left, forge and action
  grid on the right, prices beside Refine/Maximise/Reshape, Clear below, balances
  and Close in the footer.
- Reshape (`21aa911`): 874-pixel dialog, current card separated from three offer
  cards, Keep and Replace buttons. Rank is retained when replacing.
- Options (`21ee75e`): 600-pixel searchable table with Perk, Effect at Rank 0,
  and Effect at Rank 10 columns.
- Perk component (`21d3471`): 64-pixel icon, 76-pixel border, augment overlay,
  and original 32-pixel rank badge at the lower left. Shared style `21d10c7`
  defines the 2x artwork scale and 5/10-pixel related/unrelated spacing.

The official application is not modified. These are OTClient widgets matching
its file-defined layout; the official client itself is not runtime-certified.

`data/items/proficiency-shaping.json` contains 184 stable option IDs, typed perk
values, and rank-10 maxima. The general pool has 34 options; each weapon vocation
adds 30. Rank interpolation retains fractional ratios. The catalog is based on
[TibiaPal's shaping data](https://tibiapal.com/data/proficiency/perk-shaping-options.json?v=20260723-1).
Costs were cross-checked with [TibiaMonk's shaping guide](https://tibiamonk.com/en/info/perk-shaping):
250 dust for the first modification after level 3, 1,000 for the second after
mastery, 250 for reshaping, and refinement prices of 125, 200, 275, 350, 425, 500,
575, 650, 725, and 800. Maximise consumes one Lunar Ascension Orb (item 53695).
The client QML reads these prices from its controller and does not embed them.

The server owns costs, random offers, rank, vocation eligibility, protection-zone
checks, and replay protection. OTClient uses local JSON extended opcode 88,
version 1, independently of the official shaping wire protocol. Mutations name
weapon, zero-based level/index, and current revision. Reshape offers persist in
the weapon's KV record until Keep, Replace, or Clear; changing a selected perk
does not erase the other slot's shaping. Clearing restores the original native
perk. Highest-combat-skill bonuses choose the current maximum of magic, fist,
club, sword, axe, and distance skills and keep damage/healing channels separate.

Focused host validation includes costs, stale revisions, pending-offer reload,
mastery/slot/rank restrictions, orb consumption, rank interpolation in real
combat bonus paths, bounded arithmetic, and existing proficiency packet boundaries:

```sh
build/macos-release/tests/unit/canary_ut --gtest_filter='ProficiencyShaping.*:Moonsilver*.*:WeaponProficiencyPacketTest.*'
```

Validation on 2026-10-31: all 16 focused C++ tests passed. The native client
completed Modify, Refine, Reshape, Replace, Keep, Maximise, a second modification,
and Clear against the isolated Docker server. Its balance moved from 4,600 to
2,725 dust and from two orbs to one exactly as expected. Clear restored the
original homing perk. A full server restart preserved pending offers, rank,
revision, and balance. The Shape, Reshape, and Options windows were rendered and
inspected against the read-only QML layout definitions.

The opening freeze came from rebuilding the entire weapon grid for every weapon
reply. Packet callbacks now update the cache and coalesce UI work into a guarded
50-ms refresh, cancelled at logout/teardown. A 666-packet regression produces one
refresh; the real client check reached the window in 501 ms (including its
500-ms test wait), with two grid builds totalling 11 ms. Before this fix the
same opening flow stalled for about 15 seconds. Read-only shaping replies cannot
acknowledge a pending mutation, stale revisions cannot roll state back, and old
confirmation callbacks cannot affect a new selection/session.

The main server now runs `canary-local:proficiency-shaping-1533-20261030`,
executable SHA-256
`3139bdf363687b1d08b4dbdfe289f69093298e0036ccd6fa004f11e45dedca94`.
The running appearance, proficiency, and shaping files match their host hashes;
startup registration and protocol 1533/shaping version 1 login passed.
Damian Knight remains at 4,600/4,600 dust with all 666 exact native mastery caps
and flags verified. No shaping-test hook is mounted in the main server.

The pre-deployment database backup is
`database_backup/before-proficiency-shaping-20261031.sql`, SHA-256
`fcc976dd5ccabba4e30725a5647e7a78499399f910ba1eb0d80e29a86e234659`.
The official Mac client executable hash remains unchanged.
