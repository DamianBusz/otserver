# Misc Analyser: extended opcode 90, version 1

The server supplies OTClient's Misc Analyser with authoritative charm
activations, critical hits, restored life/mana and forge tier-effect activations.
This reserves extended opcode **90** in both directions. Existing impact,
imbuement tracker, forge crafting and official protocol packets stay unchanged.

## Negotiation and ownership

The client registers its receiver and, after login plus extended-opcode transport
activation, sends an independently framed JSON object on opcode 90:

```json
{"version":1,"action":"enable","session":"0123456789abcdef0123456789abcdef"}
```

The token is 32 lowercase hexadecimal characters generated afresh per login
and local analyser reset. The server clears pending deltas when the token
changes. Duplicate enables with the same token preserve pending events. Disable
uses the same envelope with `action:"disable"` and must match the active token.
Malformed requests, unsupported versions, unknown actions and requests over
256 bytes have no effect. No extension output is inferred from client version,
OS or another extension. Legacy connections and livestream viewers cannot opt in.

Counters belong to the authenticated player connection. Replaced protocols,
disconnected players and removed players cannot produce output. The writer
bypasses livestream mirroring; counters go only to their owner. No database
state or per-player timer is added.

## Batched output

The existing shared 200 ms connection sweep flushes changed counters:

```json
{"version":1,"session":"0123456789abcdef0123456789abcdef","events":[{"category":"charm","id":5,"amount":1},{"category":"imbuement","id":"mana","amount":153},{"category":"forge","id":"momentum","amount":1}]}
```

Framing is game opcode `0x32`, extended opcode `90`, u16 JSON length and UTF-8
JSON bytes. There are no `S`/`P`/`E` fragmentation prefixes or optional native
packet tails. An empty events array acknowledges a new subscription. Otherwise
there is no idle traffic. Each message contains at most 32 coalesced counters;
amounts are deltas, never cumulative totals. Pending additions saturate at the
largest integer exactly representable by the Lua client (`2^53-1`). The client
rejects messages from a stale connection or reset token.

| Group | IDs | Meaning |
| --- | --- | --- |
| `charm` | 0–24, preserving `charmRune_t` | Activations at authoritative charm effect hooks |
| `imbuement` | `critical` | Critical damage hits, once per damaged target, excluding secondary/condition damage |
| `imbuement` | `mana`, `life` | Net restored points after leech combat callbacks and bar caps |
| `forge` | `onslaught` | Fatal damage hits, once per damaged target |
| `forge` | `ruse` | Tiered-armor dodge activations; stance/hazard-only dodges excluded |
| `forge` | `momentum` | Procs that actually reduce at least one cooldown |
| `forge` | `transcendence` | Avatar-forge activation |

Critical passive charm attribution follows the per-recipient `CombatDamage`
copy: Low Blow counts when its extra critical roll succeeds; Savage Blow when
its bonus applies to a critical. Auto-attack splash keeps its existing charm
suppression. Charm extensions do not recursively count as critical/fatal hits.
Mana/life leech charm activations require a positive actual gain. Bless counts
when death-loss reduction applies; Gut when its bonus produces extra loot;
Scavenge on a successful eligible skinning action. The Lua effects use the
validated `Player:recordCharmProc(charmId)` binding.

The combat/loot chances and random draws are unchanged. Gut's existing
`gutTriggered` comparison used the same threshold on both sides; the marker now
compares against the adjusted threshold before adding the charm bonus, without
changing generated loot.

## Validation and deployment

Build using `docs/building/local-validation.md`, then run `MiscAnalyser*` tests.
These exercise opt-in, all counters, batching, reset/disable, unsupported and
legacy clients, replacement sessions, owner-only output, packet boundaries,
real critical/charm hooks and capped/full-bar leech through production combat.
Client tests additionally exercise negotiation/reset tokens and real widgets.

A host build does not replace the running Docker server. Before live playtesting,
back up the database and deploy an image containing the matching binary and
scripts, verify startup registration of `recordCharmProc`, and use the updated
client. The existing preview remains simulated until that server is deployed.
