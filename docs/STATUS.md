# Status — Goal 0B GDExtension boundary

**Status recorded at start of this work:** `d7e6f45cec045921813c3e9a7074ff09964e800d` (2026-09-13, `main`).

**Completed milestone:** **Goal 0B — Thin the Godot/GDExtension boundary**.

**Next work package:** None assigned. Do not infer a gameplay or engine-rewrite follow-up from historical goal documents.

This is the repository's single current-state document. Historical reports are preserved, but must not be read as fresh proof unless their command, checkout, and output are reproduced.

## Known today

- The source tree contains a C++20 simulation, a Godot 4.7.2 project, and a `godot-cpp` GDExtension bridge.
- The active descriptor is `godot/project/rts.gdextension`; it is source configuration, not generated output.
- `python3 scripts/dev.py configure` configures the native build and fetches pinned dependencies into `build/_deps`; `build`, `smoke`, `test`, and `run` provide the documented next operations.
- Goal 0B retired unused GDExtension bindings in favor of batched presentation state and aggregate skirmish data. `gdextension_boundary` is the retained-surface contract.
- Fresh Goal 0A evidence: a clean `/tmp` CMake directory fetched the pinned dependencies and built all targets; the documented `GODOT_BIN=... python3 scripts/dev.py smoke` entrypoint also configured the repository build, completed the Release build, and passed `native_extension_smoke`; CTest passed 3/3 outside the localhost-restricted sandbox; and `tools/tests` passed 6/6.
- Fresh Goal 0B evidence: 42 bindings were retired from the original 93-method surface; four aggregate/snapshot reads plus a faction-owned structure command leave 56 public methods, a net reduction of 37. Every remaining binding has a Godot consumer. The main HUD uses one aggregate read for timing, economy, queue, selected-unit, FOB, and catalog data; entity discovery uses one snapshot instead of per-entity bridge calls; transforms use one packed snapshot instead of separate position and heading calls; single-entity callers use one position snapshot instead of scalar X/Y reads; and structure queues accept a faction rather than exposing a production-line entity ID. Unused direct destruction, patrol, return, defend, standalone AI, combat mutation, scalar diagnostic, and queue-read calls are no longer public. `get_unit_health` remains for live hover inspection. `gdextension_boundary` passes 61 interface/payload assertions; `hud_bridge` passes 5 active-scene HUD assertions; `native_extension_smoke` passes; `skirmish_presentation` passes 95 assertions; CTest passes 3/3; and direct native runners pass 20, 21, and 178 assertions.

## Unverified or blocked

- Linux x86_64 is the only verified platform. Windows and macOS build/editor support are not currently proven.
- Godot 4.7.2 is required for the supported workflow. A Godot executable is not provisioned by CMake; set `GODOT_BIN` or install it separately.
- Network access is required once to fetch `godot-cpp` and GLM. An offline clean clone needs a pre-populated CMake dependency cache or a future source package/mirror.
- Current benchmark evidence does not establish 10,000+ combined-arms combat readiness. Do not infer render FPS from native timing.

## Architecture realities

- C++ simulation is authoritative.
- Godot is the presentation, UI, and editor layer.
- GDExtension is the bridge between them.
- The simulation remains single-threaded and global-state-based pending a deliberate later refactor.
- Existing benchmark evidence does not prove 10k+ combined-arms combat readiness.

## Do not trust as current

- Earlier README, AGENTS, handoffs, and ledger entries that name Goal 03–11 as active, complete, gated, or verified.
- Claims tied to absent commits, ignored local `vendor/` contents, local Godot binaries, old worktrees, or generated `validation/artifacts/` output.
- The duplicate `src/gdextension/` implementation; it is not in the active CMake target.

For setup and small changes, read [DEVELOPMENT_WORKFLOW.md](DEVELOPMENT_WORKFLOW.md). For current ownership, read [ARCHITECTURE.md](ARCHITECTURE.md).
