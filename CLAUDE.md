# CLAUDE.md

Read `AGENTS.md` first, then `docs/HANDOFF.md` and `docs/STATUS.md`.

## Worktrees

Do all work in a dedicated git worktree on its own task branch, never in the primary checkout, which holds the user's in-progress changes. Build and test inside the worktree, report its path and branch when finished, and remove only worktrees you created. See "Use Git Worktrees" in `AGENTS.md` for the full rules.

# Mandate of War: Agent Guide

Near-future RTS in C++ and Godot. The game rules run in a deterministic, engine-independent C++ simulation built as an ECS. Godot handles rendering, input, audio, and UI around it. Read this file fully before changing code.

## Commands
- Build simulation library and tests (no Godot needed): <fill in>
- Run unit tests: <fill in>
- Run determinism, golden-hash, and replay tests: <fill in>
- Run headless match: <fill in>
- Build the GDExtension: <fill in>
- Run the game in Godot: <fill in>
- Banned-token check: <fill in>

## Architecture in one paragraph
Everything that decides what happens in the game lives in mow_sim. Godot only (a) turns player input into commands and (b) draws what the simulation says is true. If a rule would come out differently depending on the machine, the frame rate, the compiler, or the graphics card, it is in the wrong place.

## Layers and dependency direction
- mow_sim: pure C++ (ECS world, components, systems, commands, tick, fixed-point math, RNG, hashing, serialization). Depends on the standard library only.
- mow_content: loads data files into definition structs for mow_sim.
- mow_bridge: the GDExtension layer, the only code that includes godot-cpp. Owns a sim instance, accepts commands, advances ticks, exposes snapshots.
- Godot project: scenes, UI, camera, effects, input-to-command translation. No game rules in GDScript.
- mow_net: transport for commands and state hashes only.
- tools/tests: headless runner, replay runner, tests. None require Godot.

Dependencies point inward only. mow_sim never includes Godot headers, and the build is set up so it cannot. Reason: this is what lets us test and replay without the engine and what makes peer-to-peer lockstep multiplayer possible.

## ECS conventions
- Entity = an ID. Component = plain data, no logic. System = logic over every entity having certain components.
- Prefer many small components over few large ones.
- Systems run in one explicit order defined in <path>. That order is a gameplay rule; do not reorder without a test and a note.
- Systems keep no hidden state. All state lives in components or world resources so it can be hashed, saved, and replayed.
- Do not add rich entity classes, repositories, or per-entity update methods inside the simulation.
- DDD gives us vocabulary and module boundaries (Economy, Combat, Pathfinding, FogOfWar, Production, AI). Use the names in docs/glossary.md and add new terms there.
- Any iteration whose order can change the outcome must use a stable order (entity ID ascending, or an explicitly specified sort with a total ordering).

## Commands (the only way to change the simulation)
- Every player action becomes a command: issuing player, what it does, and the tick it applies on. Commands are small, immutable, and serializable with a fixed byte order.
- Only the tick function applies commands. Godot code, GDScript, and networking never edit components directly.
- Invalid commands (wrong owner, insufficient resources, dead target) are rejected deterministically inside the simulation, never crash, and never differ between machines.
- To add an action: define the command, handle it in the proper system, add a test, then wire a Godot input to create it.

## Determinism rules (NON-NEGOTIABLE in mow_sim)
Why: in lockstep multiplayer, all machines run the same simulation and only exchange commands. One tiny difference snowballs into a desync.
- No float or double in game rules. Use the fixed-point type for positions, speeds, distances, damage, costs, and timers. No <cmath>.
- No rand() and no <random> (distributions differ between standard libraries). Use the project's own seeded RNG; its state is part of the saved state.
- No <chrono>, time(), or frame delta time. Time is measured in ticks.
- No iteration over unordered containers or pointer-keyed containers where order can affect results. Never use addresses for ordering, hashing, or identity.
- No plain std::sort when equal elements can occur; use a strict total order with an ID tie-breaker, or std::stable_sort.
- No signed integer overflow. Use unsigned or checked arithmetic where overflow is possible.
- Initialize all memory. Hash and serialize field by field, never raw struct bytes (padding is garbage).
- No threads or parallel loops over simulation state unless the result is provably order-independent.
- No file, network, or environment access inside the simulation.
- Godot code may use floats and engine time freely for display, but must never write back into the simulation.
- Do not use Godot physics, navigation, or RNG for gameplay rules. They are not deterministic across machines.

## Data-driven content
- Unit stats, costs, build times, tech trees, and balance numbers live in data files loaded by mow_content, never hardcoded in systems.
- Adding a unit should usually mean a data entry, plus a new component or system only for a genuinely new behavior.
- Peers compare a content hash on connect; changing content changes the hash.

## Godot integration rules
- The bridge runs ticks from a time accumulator: add real elapsed time each frame, run as many fixed ticks as have accumulated (with a cap), render with interpolation between the previous and current snapshot.
- Do not make one node per unit carry rules. Draw large unit counts with MultiMesh or the RenderingServer fed from a snapshot buffer.
- Keep calls across the GDExtension boundary few and batched. Pass packed arrays of per-frame data, not many per-unit calls with Variant conversions.
- UI is a view of snapshot data plus a source of commands. Use signals for UI events, but a signal never changes game state directly.
- GDScript is for scene glue, UI, and effects only. If it decides an outcome, move it into mow_sim.
- Do not call get_node or other tree searches in per-frame code; cache references.

## Testing requirements
- Every system has unit tests that build a tiny world, run a few ticks, and assert on component data.
- Determinism test: same seed plus same commands twice gives identical state hashes every tick. Must always pass.
- Golden hash test: the checked-in hash file must match on every supported compiler and OS. If it changes intentionally (a rule change), update it in the same commit and explain why in the message.
- Replay test and save/load round-trip test must always pass.
- Sanitizer builds (address and undefined behavior) must be clean.
- A bug fix includes a regression test that fails before the fix.
- Never weaken or delete a determinism or architecture check to make a build pass. Fix the code.
- Run the full test suite before saying a task is done.

## Game dev guidelines
Time and loops
- The simulation runs on a fixed timestep (default <20> ticks per second). The renderer runs as fast as it can and interpolates.
- Never tie rules to frame rate. If the game falls behind, run more ticks per frame up to a cap rather than lengthening the tick.

Performance
- Profile before optimizing and record what you measured in the commit message.
- Avoid heap allocation inside per-tick and per-frame code. Reserve vector capacity up front, reuse buffers, use arenas or pooled allocators, and prefer arrays of plain structs over pointer-linked structures.
- Store component data contiguously and iterate it in bulk.
- Pathfinding and spatial queries are the usual RTS hotspots. Use spatial partitioning (grid or similar) rather than comparing every unit with every other unit, and spread expensive work across ticks when possible.

Structure and scope
- Input produces commands and nothing else.
- Build vertical slices (select a unit, order a move, see it move, replay it) before broadening features.
- Prefer small systems that do one thing and can be tested alone.
- Add debug tooling early: tick counter, state hash overlay, command log viewer, headless match runner.

Saves, replays, and networking
- The full simulation state must be serializable and hashable at any tick. Any new state is added to serialization and hashing in the same change.
- Never put non-deterministic data (timestamps, machine IDs, display settings) in the simulation or in commands.
- Version the save and replay formats. Changing them requires a version bump and a migration or explicit rejection of old files.
- Netcode exchanges commands (with input delay to hide latency) and periodic state hashes; a hash mismatch is reported as a desync with the tick number.

C++ quality
- Use fixed-width integer types in the simulation. Treat warnings as errors on mow_sim.
- Prefer value types, std::array/std::span, and plain structs; avoid virtual calls and RTTI in hot paths.
- No raw owning pointers. Use RAII.
- Keep mow_sim's public headers small and documented; the bridge, content, net, and tools depend on them.
- Logging goes through the project's logger, never in hot loops. Use assertions in development builds; in release builds degrade deterministically.

Assets and repo hygiene
- Keep large binaries out of code directories and follow the existing asset pipeline (<fill in>). Use Git LFS or the repo's existing approach for large files.
- Placeholder art is fine; do not block gameplay work on final assets.

## Workflow rules for agents
- Before editing, state which layer the change belongs to. If it changes what happens in the game, it belongs in mow_sim.
- Make small commits, one concern each.
- If a request would break a rule in this file, stop and explain the conflict instead of working around it.
- Update this file and docs/glossary.md when architecture or vocabulary changes.

## Definition of done

- The build and all tests pass, including determinism, golden hash, replay, round-trip, and sanitizer builds.
- mow_sim compiles with no Godot include path and passes the banned-token check.
- The game still runs in Godot and behaves as before (or documented, approved differences).
- CLAUDE.md, `docs/architecture/README.md`, `docs/architecture/audit.md`, `docs/architecture/migration-plan.md`, and `docs/glossary.md` all exist and match the code.
- Finish with a short summary: what moved, what changed in behavior, and what remains.

## Sourced models and assets

All sourced models must be royalty free for commercial use, and each one must be given credit in the game.
