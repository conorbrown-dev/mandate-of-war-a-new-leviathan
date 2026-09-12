# Worktree and Evidence Reconciliation

**Date:** 2026-09-12
**Authority:** This record reconciles the local Git object graph, working trees, stash, and fresh validation. It supersedes unsupported completion claims in historical status documents.

## Repository boundaries and integration result

- Current `main`: merge commit `7332fc5` (`Merge recovered pre-model-integration work`).
- The current dirty-tree implementation was committed as `a14cc79` (`Integrate current RTS implementation and validation work`).
- The recovered pre-model-integration tracked/source/content slice was committed as `abf5684` and merged into `main` by `7332fc5`.
- The historically cited checkpoint `9c295bf` is not an object in this repository. Do not cite it as a baseline or source of verified behavior.
- The registered `feature/model-integration-pack` branch points to `d41b6a7`, now an ancestor of `main`; it had no unique commits to merge.
- Its registered external worktree at `/home/conor/repos/near-future-rts-game-model-integration-pack` is unavailable because its `.git` file points at a missing Git directory. Do not prune, repair, remove, or recreate it without explicit authorization.
- `stash@{0}` (`pre-model-integration-merge-preserve`) is retained as a recovery record. Its repository-owned tracked/source/content changes are in `abf5684` and therefore `main`. Its only excluded payload is the ignored external dependency cache: the 140 MB Godot binaries and 1.4 GB `downloaded_assets/` directory remain on disk but are not version-controlled.

## Current dirty-tree implementation

The checkout's terrain/road/off-road systems, territorial control, production and command changes, Godot presentation/UI work, validation tooling, scenarios, generated repository assets, and documentation are committed to `main`. They remain candidate functionality rather than accepted milestone proof; merging work does not change the sequential acceptance gates.

## Fresh evidence

| Area | Result | Evidence |
|---|---|---|
| Release build | PASS | `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`; `cmake --build build` |
| Configured native tests | PASS | CTest 3/3 |
| Direct native tests | PASS | 18/18 core, 21/21 portable snapshot, 164/164 integration assertions |
| Godot baseline | PASS | Editor scan and `res://test.gd` smoke exit successfully; sandbox user-settings/log writes are environmental limitations |
| Godot presentation | PASS | `test_skirmish.gd` reports 91 checks, 0 failures |
| Gameplay validation | PASS | `basic_selection_move`, `strategic_zoom_transition`, `airfield_fighter_ferry`, and `simulation_scale_1000` |
| Goal 03 combat benchmark | FAIL | `rts_combat_benchmark 2000 100`: zero projectiles and zero destructions; non-zero exit |
| Goal 04 logistics benchmark | FAIL | `rts_logistics_benchmark 10000 30 100`: 20.920 ms/tick average versus the 15 ms acceptance limit; non-zero exit |

## Routing decision

`G03-COMBAT-BENCH` is the sole active acceptance item. Goal 04 and all later goals are gated. Candidate later-system work remains available in the dirty tree and must be regression-tested as Goal 03 is repaired; it is not permission to skip the numbered sequence.

## Required follow-up

1. Diagnose why the representative combat workload produces no projectile activity.
2. Run the narrow corrected combat benchmark and its relevant behavior tests.
3. Reassess Goal 03 documentation and only then consider Goal 04.
4. Do not apply `stash@{0}` or alter the unavailable external worktree without explicit authorization.
