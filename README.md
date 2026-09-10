# Near-Future RTS

A large-scale near-future RTS prototype with a C++ fixed-tick simulation and Godot 4 presentation layer.

## Current Focus

Goal 08 — Playable Skirmish Vertical Slice is active by explicit user direction. The current slice establishes the validated Broken Strait scenario and Godot setup flow; `G08-SCENARIO` is in progress and no Goal 08 acceptance criterion is verified.

The repository contains substantial provisional combat, production, logistics, networking, replay, and AI work across post-baseline commits and a dirty working tree. Earlier review findings and required reviews remain prerequisites for Goal 08 verification.

## OpenCode Start Here

OpenCode automatically reads `AGENTS.md`. Its next session should then follow [docs/OPENCODE_HANDOFF.md](docs/OPENCODE_HANDOFF.md), which contains:

- the required reading order;
- baseline build and Godot commands;
- verified behavior;
- known correctness blockers;
- the active definition of done;
- a bounded first-task prompt.

Do not feed every milestone to OpenCode at once. The immediate task is Goal 03 behavior and benchmark validation; do not proceed to Goal 04.

## Build and Smoke Test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

./Godot_v4.7.2-stable_linux.x86_64 \
  --headless --path godot/project --script res://test.gd

./Godot_v4.7.2-stable_linux.x86_64 \
  --headless --path godot/project --quit-after 30
```

See [EXTENSION_STATUS.md](EXTENSION_STATUS.md) for the working GDExtension layout and [docs/NEXT_TASKS.md](docs/NEXT_TASKS.md) for the ordered remaining work.

## Demo

From a Linux desktop session, launch the playable Broken Strait presentation
with:

```bash
./scripts/run_demo.sh
```

It rebuilds the native extension, opens Godot, and starts the 20-versus-26
skirmish automatically. Blue units are Elite Precision; red units are Mass
Warfare. Drag-select blue units, right-click to issue a move, use the wheel to
zoom, WASD/arrow keys to pan, and `X` to stop.

## Canonical Milestone Order

1. `01_ENGINE_AND_ARCHITECTURE.md`
2. `02_SIMULATION_AND_SCALE.md`
3. `03_ECONOMY_COMBAT_FACTIONS.md`
4. `04_LOGISTICS_AIR_NAVAL_INTEL.md`
5. `05_MODDING_ASSET_PIPELINE_MAP_EDITOR.md`
6. `06_MULTIPLAYER_REPLAYS_STATS_AI.md`
7. `07_AI_FOUNDATION.md`
8. `08_PLAYABLE_SKIRMISH_VERTICAL_SLICE.md` (active)

Supporting documents such as `03_PATHFINDING.md` and `03_RENDERING_AND_CONTROLS.md` refine the active scale-prototype work; they do not create permission to skip the canonical order.
