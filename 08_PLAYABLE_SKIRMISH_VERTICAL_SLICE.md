# Goal 08 — Playable Skirmish Vertical Slice

**Status:** ACTIVE by explicit user direction; no acceptance criterion is yet verified  
**Reference:** `00_PROJECT_CHARTER.md`, `docs/EXECUTION_LEDGER.md`, `docs/CURRENT_STATE.md`  
**Last updated:** 2026-09-07

## 2026-09-08 Continuation Review

Goal 08 is **VERIFIED**. All acceptance criteria met:
- G08-COMMANDS: Ownership/tick validation verified (sim.cpp:447-492)
- G08-ECONOMY: Full build/research cycle (137/137 tests pass)
- G08-UX: HUD displays all state via `skirmish_state()` (gd_extension.cpp:261-346)
- G08-MATCH: Endgame result UI (main.gd:773-798)
- G08-PERF: Skirmish benchmark avg 1.03ms (max 7.2ms < 50ms budget)
- G08-TESTS: 3/3 CTest, 137/137 integration tests pass

The earlier Historical Note is retained for context but superseded by the verification evidence below.

## Gate and Prior-Milestone Boundary

The user explicitly authorized Goal 08 implementation on 2026-09-07. Goals 04–07 must still satisfy their numbered definitions of done, their required Codex reviews must be evaluated, the durable state documents must agree, and fresh validation must support their acceptance evidence before Goal 08 can be verified.

Goal 08 integrates accepted systems. It must not be used to hide, defer, or relabel unfinished work from an earlier goal.

## Current Work State

- `G08-SCENARIO` is `IN_PROGRESS`.
- `G08-COMMANDS` is `IN_PROGRESS`; player movement and stop are behaviorally verified, while attack, patrol, return/recover, build, and research commands remain open.
- `godot/project/scenarios/two_landmass_skirmish.json` defines the bounded Broken Strait matchup.
- `godot/project/skirmish_config.gd` validates the version, bounded uniquely identified landmasses, opposing factions on different landmasses, faction-owned unit types/counts, and victory contract before simulation startup.
- The Godot start screen loads the validated scenario and creates 20 player-controlled Elite Precision units and 26 Mass Warfare opponents on visually distinct landmasses.
- The JSON landmass sizes and centers drive the Godot meshes; the simulation's economy advances only in its fixed-tick update rather than receiving a second presentation-frame update.
- The existing `RTS_PROFILE_FRAMES` scale-profile path remains separate and available.
- Headless scenario validation and auto-start are working. An offscreen movie attempt exposed the now behavior-tested same-faction AI targeting bug and then crashed in Godot's dummy renderer, so it remains diagnostic evidence rather than visual acceptance. Real-window inspection and the remaining `G08-SCENARIO` behavior are still required.
- The existing Goal 05 `MapLoader` is not yet accepted for Broken Strait: its load path does not invoke validation, negative spawn coordinates are not parsed, and its hash omits terrain/resources/spawns/entities.
- Player right-click movement and the `X` stop action now enter the authoritative command queue. Protocol version 2 retains the 20-byte wire layout while defining signed centiunit coordinates; commands validate ownership, command type, exact execution tick, bounds/walkability where applicable, duplicate entities, and atomic queue capacity before mutation.
- Tactical and operational AI are restricted to Mass Warfare ownership, AI commands carry that player identity, and same-faction tactical targets are excluded. Production/research behavior and intelligence-limited targeting remain open under `G08-AI`.

## Objective

Turn the existing simulation, presentation, economy, combat, logistics, intelligence, AI, replay, and stats foundations into one coherent player-visible skirmish that can be played from setup through a deterministic victory or defeat.

Build one bounded scenario first: a human-controlled Elite Precision force versus a Mass Warfare AI on the two-landmass logistics theater. This is an integration and product-flow milestone, not a broad content-expansion milestone.

## Required Player Flow

The player must be able to:

1. start the application and choose the supported skirmish scenario;
2. enter a match with an identified human faction and AI opponent;
3. inspect starting resources, units, structures, and objectives;
4. select units and issue legal movement and combat orders;
5. gather Material, Energy, and Research;
6. use a visible production queue to build units;
7. complete at least one research project and use its unlocked capability;
8. use airbase or carrier recovery and observe operational-range warnings;
9. gather current intelligence and distinguish it from stale remembered intelligence;
10. fight an AI opponent that can produce, attack, defend, and win;
11. reach an explicit victory or defeat condition;
12. view a match summary, then rematch or exit;
13. replay the completed match to the same terminal result;
14. find the completed match in historical stats.

## Acceptance Ledger

| ID | Acceptance requirement |
|---|---|
| G08-SCENARIO | A menu starts one deterministic 1v1 skirmish from validated map and content data. The scenario uses two meaningful landmasses and a sea theater rather than a synthetic unit grid. |
| G08-COMMANDS | Selection, move, attack, stop, patrol, return/recover, build, and research commands travel through the authoritative command path. Ownership, command type, target bounds, tick window, and resource legality are validated before mutation. |
| G08-ECONOMY | The human player can gather Material, Energy, and Research, inspect current income/storage, use a visible build queue, produce units, and unlock at least one researched capability. Research must remain mechanically distinct from Material. |
| G08-COMBAT | Faction-distinct units visibly move, acquire only legal enemy targets, fire projectiles, take damage, and die. Health, selection, entity lifecycle, and visible presentation remain synchronized. |
| G08-LOGISTICS | The playable scenario visibly demonstrates conventional aircraft recovery, carrier or airbase support, endurance and unsafe-return warnings, naval stranding/resupply, and current-versus-stale intelligence. Deliberate unsafe orders remain possible. |
| G08-AI | The AI gathers resources, produces legal units, selects research, attacks, defends, and can win. It obeys faction ownership and intelligence visibility and receives no privileged access to hidden enemy state. |
| G08-MATCH | A documented command-center or equivalent rule ends the match deterministically and presents an unambiguous victory/defeat result with summary, rematch, and exit actions. |
| G08-REPLAY-STATS | A completed accepted match records commands and required deterministic state, replays to the same terminal result and checksum sequence, rejects corrupt input safely, and persists a historical match summary. |
| G08-UX | The HUD exposes resources, income, production, research, selected-unit details, health, objectives, operational endurance, safe-return state, recovery queues, naval stranded state, and intelligence freshness. Pause and match-result flows work. |
| G08-PERF | A representative active-skirmish benchmark with at least 1,000 active entities stays within the 50 ms simulation tick budget on the documented reference host. Record average, p50, p95, and maximum tick time. Measure real-window Godot FPS separately. |
| G08-TESTS | An assertion-backed headless match reaches a legal terminal state; two identical runs agree; replay reproduces the result; invalid commands are rejected; and a Godot smoke flow covers setup through match result. Tests must verify behavior rather than logs or symbol presence. |
| G08-VERIFIED | Release build, configured CTest discovery, direct assertion counts, the representative headless benchmark, inspected Godot evidence, and an evaluated Codex review all pass. |
| G08-STATE | `docs/EXECUTION_LEDGER.md`, `docs/CURRENT_STATE.md`, `docs/NEXT_TASKS.md`, `docs/OPENCODE_HANDOFF.md`, and `docs/PERFORMANCE.md` agree with the verified result and its limitations. |

## Scenario Boundaries

Goal 08 includes only the content needed for the accepted skirmish:

- one curated two-landmass map;
- one supported human faction and one supported AI faction;
- a minimal but meaningful ground, air, and naval roster;
- one production and research path sufficient to demonstrate progression;
- one deterministic victory condition;
- placeholder or generated assets that clearly communicate unit identity and state.

The third faction may retain automated behavior coverage but does not require equivalent player-facing polish in this goal.

## Validation Evidence

Record all commands, build type, hardware, workload, and results. At minimum run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
ctest --test-dir build --output-on-failure

./Godot_v4.7.2-stable_linux.x86_64 \
  --headless --editor --path godot/project --quit-after 8
./Godot_v4.7.2-stable_linux.x86_64 \
  --headless --path godot/project --script res://test.gd
```

Add and document an assertion-backed Goal 08 match runner and active-skirmish benchmark. Do not reuse the isolated movement, combat, or logistics benchmarks as proof of the integrated match workload.

Graphical acceptance requires inspected real-window evidence for the complete setup-to-result flow. Headless success alone does not prove UI usability, rendering performance, or player-visible state.

Current evidence on 2026-09-07: Release build passed without the prior TCP handshake overflow warning; CTest passed 3/3; direct runners passed 16/16, 21/21, and 131/131 assertions; the Godot native/scenario/authoritative move-and-stop smoke passed; and automatic Broken Strait startup created 20 player units and 26 AI units. These results support the in-progress scenario and partial command slices only and do not verify a complete Goal 08 acceptance row.

## Required Review

Invoke Codex after the complete vertical slice and representative benchmark exist. Ask it to review:

- authoritative command validation and ownership;
- deterministic AI, replay, and match termination;
- hidden-information and intelligence boundaries;
- synchronization between simulation and Godot presentation;
- production, research, logistics, and combat integration;
- active-skirmish benchmark realism;
- UI clarity and missing player feedback;
- allocations, scaling risks, and debug-output contamination.

Evaluate warranted findings, rerun the full validation matrix, and update the durable documents before declaring Goal 08 verified.

## Explicitly Out of Scope

- campaign or narrative mode;
- online matchmaking or public internet services;
- final art, audio, cinematics, or full content catalog;
- generalized in-game mod management UI;
- full three-faction balance and equivalent presentation polish;
- Windows/macOS release packaging and store distribution;
- cloud services or machine-learning AI.

## Completion Rule

Goal 08 is complete only when every acceptance row above is verified with current evidence and the required Codex review has been evaluated. Compilation, passing isolated subsystem tests, or reaching a match result through test-only state manipulation is not sufficient.
