# Mandate of War

> **Current authority:** [docs/STATUS.md](docs/STATUS.md) is the single current-state document. This README's older milestone narrative below is historical context only; Goal 0B is complete and no successor work package is assigned.

## Reproducible quick start

On Linux x86_64, install CMake 3.20+, a C++20 compiler, OpenSSL development headers, Python 3, Git, and Godot 4.7.2. Set `GODOT_BIN` if Godot is not on `PATH`, then run:

```bash
python3 scripts/dev.py configure
python3 scripts/dev.py build
python3 scripts/dev.py smoke
python3 scripts/dev.py run
```

The build fetches pinned dependencies into `build/_deps`; it does not use an untracked `vendor/` directory. See [docs/DEVELOPMENT_WORKFLOW.md](docs/DEVELOPMENT_WORKFLOW.md) and [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

# Historical project overview

A large-scale near-future RTS prototype with a C++ fixed-tick simulation and Godot 4 presentation layer.

## Current Focus

Goal 03 — Economy, Combat, and Faction Vertical Slice is active for recovery. A fresh 2,000-vs-2,000 combat benchmark produces no projectiles or destructions, so later goal claims are gated regardless of the working features present in this checkout.

The repository contains substantial uncommitted combat, production, logistics, terrain, presentation, validation, and territorial-control work. It is intentionally preserved and is not part of `main` until explicitly committed. See [the worktree reconciliation](docs/WORKTREE_RECONCILIATION.md) for the exact boundary, including the preserved stash and unavailable secondary worktree.

## OpenCode Start Here

OpenCode automatically reads `AGENTS.md`. Its next session should then follow [docs/OPENCODE_HANDOFF.md](docs/OPENCODE_HANDOFF.md), which contains:

- the required reading order;
- baseline build and Godot commands;
- verified behavior;
- known correctness blockers;
- the active definition of done;
- a bounded first-task prompt.

Do not feed every milestone to OpenCode at once. The immediate task is `G03-COMBAT-BENCH`; do not proceed to Goal 04.

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
