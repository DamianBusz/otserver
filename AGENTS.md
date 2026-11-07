# Canary-specific guidance

The global Git, commit, PR, C++ header, exception, and documentation policies apply. This file only records Canary-specific gates.

## Recurring Defect Prevention

- For a reusable defect, inspect analogous paths by behavior and ownership, fix confirmed siblings atomically, and keep the audit proportional; do not turn a one-off into a speculative refactor.
- Decide whether tooling makes recurrence impossible. If not, add a narrow rule to the nearest `AGENTS.md` that states the unsafe pattern, required alternative, and validation rather than incident history.
- Prefer enforceable safeguards—types, helpers, static checks, architecture docs, or focused tests—especially for lifetime, arithmetic, identity, ownership, bounds, and cancellation escapes.

## Deferred Callback Lifetime Safety

- Assume scheduled, deferred, timer, and worker callbacks can outlive their source object or state.
- Never capture raw `this`, references, iterators, or mutable-container pointers across that boundary. Use immutable values plus `std::weak_ptr` or re-resolvable identity validated with the original identity, generation, epoch, or session token.
- Removal, replacement, reload, or reinterpretation must cancel pending work or advance a checked generation. Callback-owning types are non-movable unless moving cancels or safely rebinds every event.
- Use bounded arithmetic for intervals; stale work must become a no-op before gameplay, Lua, combat, movement, persistence, or client output. Cover destroyed/replaced owners, reused IDs, shutdown, and ownership transfer where practical.

## Static Ownership Lifetime Safety

- Never rely on cross-translation-unit static destruction order for caches, registries, or other global owners of gameplay objects. Prefer runtime-owned state; when global ownership is unavoidable, provide an explicit idempotent drain during controlled shutdown.
- Stop and join every producer and consumer before draining global ownership, and release retained objects while all services their destructors may access are still alive. Validate shutdown with retained entries and lifetime instrumentation where practical.

## PostgreSQL Binary Data

- Never pass serialized or otherwise opaque bytes to `escapeString`; PostgreSQL text literals require valid client-encoding text. Use `escapeBlob` for `bytea` values and validate round-trip preservation with invalid UTF-8 bytes.

## Canary build discipline

- Before an authorized local build, read `docs/building/local-validation.md`; its maintained entry-point, environment, preset, cache, and MSVC Ninja workflow is mandatory.
- C++ source/header additions, removals, and renames must update every maintained entry: the relevant CMake list, server `vcproj/canary.vcxproj`, and test CMake list when applicable.

- When handing off local gameplay changes for playtesting, verify the running Docker image contains the matching server bindings and scripts; a successful host build or updated client does not update the server. After replacement, check startup registration and preserve a database backup.

### MSVC Ninja dependency tracking

- Before configuring, repairing, or auditing an MSVC Ninja build, read `docs/building/local-validation.md#msvc-ninja-dependency-tracking`. Its code-page, launcher, dependency-log, and concurrency rules remain mandatory.

## Aimed Spell Direction

- Explicitly aimed directional spells must select a cardinal axis with `getPrimaryDirection` and pass a cast position without turning the creature or falling back to its attack target. Validate all cursor octants and check that facing remains unchanged.

- Directional Lua area matrices anchor at the cast position one tile ahead, not at the caster. Marker `3` is both an anchor and an affected tile (`2` is anchor-only). Validate affected world coordinates, including the anchor, rather than counting only `1` cells.

## Precompiled Header Policy

- `src/pch.hpp` owns broad shared standard includes; do not duplicate an unguarded PCH include.
- Headers must declare their public dependencies. When a source needs a PCH-provided include without PCH, guard it with `#ifndef USE_PRECOMPILED_HEADERS`; add broad includes to the PCH with the same local fallback.

## Lua Shared Userdata Gate

- Before changing `std::shared_ptr` Lua userdata, read `docs/systems/lua-shared-userdata.md` and use its typed trait, registration, and push helpers.
- Never combine shared `pushUserdata` with a manual metatable, use a weak metatable for shared userdata, or wrap a borrowed object without a no-op deleter. Run the document's two `rg` checks and investigate every match.

## Docker Quickstart Policy

- For quickstart changes, read `docs/docker/quickstart-for-beginners.md` and `docker/DOCKER.md`; keep CI/build, development, and user quickstart responsibilities separate.
- The default client path is `login-server` at `http://localhost:8088/login`, never MyAAC `login.php`. MyAAC remains website/admin-only, uses `slawkens/myaac` `2.x`, and keeps `http://localhost:8080`; public config stays `CANARY_*`, and the quickstart uses the published Canary runtime image.

## Protocol compatibility

- Do not infer optional packet tails from the numeric client version alone. Tie build-specific tails to a verified reader or capture, and validate the production writer through the next opcode boundary while preserving legacy profiles.

- Party-position extensions require per-connection opt-in and live, reciprocal party membership checks. Never send private tracking through a writer that mirrors to cast viewers. Validate nonmembers, invitations, unsupported clients, reconnect identity reuse, and the next opcode boundary when changing this path; reserve extension IDs in `docs/systems/party-position-tracking.md`.

## Wheel compatibility

- Wheel slot, stage and gem IDs are persisted and sent over the protocol: preserve their numeric identities when replacing perks. Update both server bonuses and client slices, summaries, icons and gem descriptions together. Validate saved-slot mappings, cumulative grades and units (IOWheel cooldown declarations are seconds; runtime spell cooldowns are milliseconds).

- When replacing Wheel sprite atlases, map cells by perk identity rather than retaining old pixel offsets. Check changed cell artwork against its source and cover both node copies and full tooltips in the Wheel regression.

- Do not infer updated perk positions from legacy spell slots. Pin vocation-specific node assignments to an independent official layout reference, and validate every changed node pair against both server grants and client artwork/text while preserving serialized slot IDs.

- Wheel critical-chance declarations use percentage points. Convert to `CombatDamage` hundredths exactly once; validate both registered spell augments and runtime spell bonuses.

- Keep fractional Wheel damage percentages through declaration, gem/grade aggregation and runtime combat; never round a percentage to an integer before applying it. Validate both registered augments and actual damage for a fractional grade.

- Preserve fractional weapon-proficiency damage percentages through spell augmentation; do not round ratios to whole percentage points. Validate a fractional Stellar perk against the copied proficiency data and the runtime multiplier.

## Derived healing and survival arithmetic

- Spell-damage and spell-healing proficiency passes must be mutually exclusive by combat type. When a caller evaluates both passes, reject healing in the damage pass as well as damage in the healing pass; validate both perk kinds together on healing, physical and elemental spells.

- Secondary healing must carry an explicit origin and revalidate its original party plus live recipient eligibility after combat callbacks. Do not reapply source bonuses, recursively share/link, or inherit a critical result as a new base. Cover overheal and stale recipients.
- Widen signed damage totals and unsigned mana before arithmetic, then bound the result before narrowing. Do not use signed `abs` on an unbounded damage component; validate lethal-hit and signed/unsigned boundary cases.

- For auto-attack splash, determine charm eligibility per recipient before critical extensions and retain it through damage, fatal, life-leech and mana-leech processing. Only the main attack target may trigger charms; ordinary criticals and spell/rune charm eligibility must remain intact. Validate both primary and splash recipients.

- Lua combat dispatch must return the engine result for every variant branch; never report success after discarding a failed cast. Validate the Lua-facing result for both a valid target and a removed or rejected target so failed spells cannot charge resources or cooldowns.

- Named Lua callback registration consumes its global function. Re-expose a shared local function before each registration or use distinct globals; callback test doubles must consume globals and validate every combat variant.

- Legacy fight-mode bytes are compatibility input only; they must not alter attack, defense or mitigation. Keep the permanent offensive bonus in its existing damage/display units and validate every legacy mode across affected vocations to prevent double compensation.

- Scripted weapon Combat objects must explicitly use their auto-attack origin (for example `ORIGIN_RANGED` for ammunition), never the default spell origin. Validate whole-swing suppression and primary-versus-splash charm eligibility for scripted and native weapons.

## Proficiency shaping persistence

- Shaping option IDs are persisted identities, not row positions. Preserve existing IDs when changing the catalog. Validate saved options and pending offers against the weapon's vocation and unlocked tree before applying bonuses; test costs, replayed revisions, rank limits, and reconnect persistence together.

## Bounty rerolls

- Do not gate token-funded bounty rerolls on unselected choices only; allow replacing selected, unfinished assignments while protecting completed rewards. Validate the daily-claim then reroll action sequence, token persistence, cleared progress and creature icons, and rejection with no tokens or unclaimed rewards.
