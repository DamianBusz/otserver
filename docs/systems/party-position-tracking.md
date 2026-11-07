# Remote party positions: extended opcode 89, version 1

This server extension supplies remote positions for OTClient's normal minimap.
It requires a new client receiver; it does not modify nearby creature packets,
party shields, map imagery, or docking. No feature is inferred from the numeric
client version, OS, or the presence of another OTClient extension.

## Transport and reservation

Both directions use the existing extended-opcode transport:

| Field | Encoding | Value |
| --- | --- | --- |
| Game opcode | u8 | `0x32` (50) |
| Extended opcode | u8 | `0x59` (89) |
| Payload length | u16 little endian | Number of UTF-8 JSON bytes |
| Payload | raw bytes | One JSON object, without a terminator |

Do not send OTClient's `S`/`P`/`E` JSON fragmentation prefixes. Each message is
independently framed JSON. Use `registerExtendedOpcode(89, callback)` plus
`json.decode`, and `sendExtendedOpcode(89, json.encode(request))`.

The allocation audit covered Canary's C++ dispatch, Lua creature events and
modules, plus the adjacent `otclient/modules` and `otclient/src/client` checkout.
Existing extensions include 0 (transport enable), 1 (locale), 2 (ping), the
client's reserved 3–7, 86 (stance state), 87 (aimed spells), 88 (proficiency
shaping), and 201 (shop). No handler used extended opcode 89. This document
reserves **89** for party positions in this distribution. Extended opcode **90**
is reserved for the [Misc Analyser](misc-analyser.md), with its own explicit
per-connection subscription. Extended opcode **91** is reserved for the
[market tier overview](market-tier-overview.md), a solicited read-only snapshot.
Ordinary game opcodes
have a separate namespace. Third-party deployments must audit their extensions
before adopting this reservation.

Existing server party packets (`0x8B`, shields, health, mana, vocation) do not
provide the required position stream, session identity, or resets. Their wire
format and behavior remain unchanged.

## Capability negotiation

After game login and extended-opcode transport activation, the client registers
its receiver and sends:

```json
{"version":1,"action":"enable","session":"0123456789abcdef0123456789abcdef"}
```

`session` is a fresh client-generated random 128-bit token encoded as exactly
32 lowercase hexadecimal characters. Generate a new token for every connection
and every new enable cycle. Discard old tracking locally before enabling.
The token correlates replies; it is not an authentication credential.

The server's first `snapshot` or `clear` acknowledges support for version 1.
Until then, use nearby markers only. Older servers may ignore the request.
Unsupported versions, malformed JSON/types, unknown actions, invalid tokens,
and requests larger than 256 bytes are silently ignored. The capability is
connection-local and is never persisted or inherited by a replacement login.

Additional requests use the current token:

```json
{"version":1,"action":"resync","session":"0123456789abcdef0123456789abcdef"}
{"version":1,"action":"disable","session":"0123456789abcdef0123456789abcdef"}
```

`resync` requests a complete replacement snapshot. `disable` produces a `clear`
with `enabled:false`, then stops all tracking output. Clear locally immediately
on disable rather than waiting for the response. A stale token cannot disable
or resync a newer subscription. Multiple requests before a sweep coalesce.

## Common response fields and identity

Every response includes:

| Field | Type | Meaning |
| --- | --- | --- |
| `version` | integer | Always `1` |
| `type` | string | `snapshot`, `update`, `remove`, or `clear` |
| `session` | string | Echo of the client's active token |
| `epoch` | decimal string | Increasing reset generation within this connection |
| `seq` | decimal string | Contiguous message sequence within the epoch, starting at `"1"` |

Counters are canonical unsigned decimal strings, without leading zeroes. Keep
them as strings or use exact integer handling; do not lose precision through a
Lua/JavaScript floating-point conversion. To compare decimal strings, compare
length first, then lexicographically when lengths match.

The epoch advances on a new subscription/resync, party membership change, or
clear/reset. Leaving and rejoining the same party between sweeps still changes
the recipient's membership revision and forces a new epoch. Snapshot and clear
are the only messages that can establish an epoch; deltas cannot.

A member record is:

```json
{"id":"42","creatureId":268435498,"name":"Alice","x":33805,"y":32767,"z":2,"leader":true}
```

- `id`: opaque decimal string identifying this member's **connection session**.
  It stays constant during that connection, changes on reconnect even when the
  server reuses the Player object/creature ID, and is unique within the running
  server process. It is not a persistent character/database ID.
- `creatureId`: the existing u32 creature ID, supplied to merge nearby updates.
  It is not sufficient to distinguish reconnects by itself.
- `name`: character name, JSON escaped.
- `x`, `y`: u16 world coordinates represented as JSON integers.
- `z`: world floor represented as a JSON integer.
- `leader`: boolean, true only for the current party leader.

Snapshots include the recipient itself and all connected, alive party members,
regardless of their client capabilities or distance. The UI should omit its own
marker. Invitations, summons, dead/disconnected players, and other parties are
excluded. The current leader may be temporarily absent if disconnected/dead;
do not elect a replacement in the client.

## Snapshot

```json
{"version":1,"type":"snapshot","session":"0123456789abcdef0123456789abcdef","epoch":"1","seq":"1","part":0,"parts":1,"members":[{"id":"42","creatureId":268435498,"name":"Alice","x":33805,"y":32767,"z":2,"leader":true},{"id":"43","creatureId":268435499,"name":"Bob","x":33000,"y":32000,"z":9,"leader":false}]}
```

`part` is zero-based, `parts` is the total number of messages in this batch.
Each contains at most 32 records; all snapshot parts have the same epoch and
contiguous sequences. The server writes the entire batch in one world-commit
sweep, without interleaved deltas. On the first part of a new epoch, discard
previous remote tracking. Accumulate all parts, then atomically install the
complete snapshot. Never treat one part as the whole membership.

A supported joining client gets a full snapshot by the next sweep. Existing
members get an update adding it. A reconnect must negotiate again and gets a
full snapshot if the character still has a party. If normal logout/death logic
has removed the character from the party, it gets a clear instead; this feature
does not restore party membership.

## Member updates, including leader changes

```json
{"version":1,"type":"update","session":"0123456789abcdef0123456789abcdef","epoch":"1","seq":"2","part":0,"parts":1,"members":[{"id":"43","creatureId":268435499,"name":"Bob","x":34000,"y":32000,"z":12,"leader":false}]}
```

Every entry is a full upsert, not a partial position patch. Unchanged entries
are omitted. Joins, movement, teleports, floor changes, and leadership changes
use this message. Leadership transfer updates the old and new leader flags
in the same batch; accumulate all `parts` before applying a batch so the UI
does not temporarily display two leaders. Ordering of members is not meaningful.

## Explicit member removal

```json
{"version":1,"type":"remove","session":"0123456789abcdef0123456789abcdef","epoch":"1","seq":"3","part":0,"parts":1,"ids":["43"]}
```

Remove these session IDs from remote tracking. This covers leaving, disconnect,
death, removal from the world, and replacement connections. Removals precede
upserts in the same sweep, so a reconnect removes the old session before adding
the new one. Like updates, removals can be split into batches of 32.

## Clear/reset

```json
{"version":1,"type":"clear","session":"0123456789abcdef0123456789abcdef","epoch":"2","seq":"1","enabled":true}
```

Discard all remote members and incomplete batches immediately. `enabled:true`
means the subscription remains active, but the recipient has no eligible party
view (not in a party, dead, or no longer a reciprocal member). A later join or
return to life yields a new snapshot. `enabled:false` acknowledges disable.
Disbanding clears all subscribed recipients. A non-party client can receive
this position-free acknowledgement; it never receives another party's records.

## Client merge and stale-message rules

1. On logout, disconnect, protocol replacement, or disable, drop all remote
   state, pending parts, and the active token. Re-negotiate after reconnect.
2. Reject responses whose version or session token does not match. Reject older
   epochs, duplicate/older sequences, and deltas without an installed snapshot.
3. Accept a newer epoch only from snapshot part 0 or clear, with `seq:"1"`.
   For missing sequences or inconsistent parts, discard pending tracking and
   request `resync`. Do not apply a partial membership snapshot.
4. Maintain a remote membership cache keyed by session `id`. Use `creatureId`
   and name to associate live nearby creatures. Nearby movement can update the
   display between remote sweeps. Remote updates remain authoritative for
   membership, leader status, and positions outside the visible area.
5. A normal creature disappearance is not a remote membership removal. Remove
   its nearby association, but retain its remote marker until a remote remove,
   snapshot replacement, clear, or connection reset. This differs from the old
   nearby-only behavior. After explicit removal, stale nearby references must
   not recreate the old remote session.
6. Filter markers to the currently displayed minimap floor; use blue for
   members and gold for `leader:true`, with the supplied name as tooltip.

No periodic heartbeat is sent for stationary members. Do not expire them merely
because no movement update arrived; connection teardown and explicit resets
are the invalidation mechanisms.

## Scheduling, privacy, and validation

One world-commit dispatcher cycle runs every 200 ms. It samples current party,
connection, health, and position state, compares per-recipient cached values,
and sends only changed records. Fast movement coalesces to the latest position;
membership transitions and removals have the same maximum normal sweep delay.
There are no per-player timers or retained Party/Player objects in the cache.
The shutdown gate stops the sweep from sending during shutdown.

Both recipient and subject must have their current authenticated player
connection, and subject membership is revalidated against the recipient's
party. A client cannot request a party, player, or position by ID. Output goes
directly to the recipient's protocol buffer, bypassing cast packet mirroring.
Unsubscribed official/legacy/OTClient connections receive no extension output.

Tests live in `tests/unit/server/network/protocol/party_tracking_test.cpp` and
exercise production negotiation, serialization, party lifecycle, remote
position changes, identity replacement, death, filtering, reset semantics,
batching, and a following opcode sentinel. Run with the maintained build:

```sh
cmake --build --preset macos-release --target canary canary_ut -j 4
build/macos-release/tests/unit/canary_ut --gtest_filter='PartyTracking*'
```

Local validation passed the `macos-release` server/test build and all 18 tests
selected by `PartyTracking*:PartyTest.*:ProtocolProfileRegistryTest.*:ConnectionTransportTest.*`,
including clean process teardown. The nine tracking tests exercise the real
party methods, shared Game sweep, protocol negotiation and writer. Tile
relocation and death-screen state are set up in the fixture; this is not a live
OTClient/Docker end-to-end test.

Use the corresponding maintained preset on other platforms. A host build does
not update the Docker server. Deploy and validate the matching server image
before client playtesting, following the repository's Docker handoff policy.
