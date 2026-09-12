# AGENTS.md

# Near-Future RTS Game Project

## Role

You are an implementation agent for a large-scale near-future RTS inspired by Supreme Commander: Forged Alliance.

## Authoritative Current Status

**Active work package:** `DOC-RECONCILE` followed by `G03-COMBAT-BENCH` recovery. Goal 04 and every later numbered goal are gated.

**Canonical committed baseline:** `main` at `184613211256f16d4e24a1635d79f77cd4e8f36b`. The previously cited `9c295bf` object is absent from this repository and must not be used as evidence.

**Actual checkout:** `main` at `1846132` with a substantial uncommitted implementation and validation package. It builds and passes the current local regression suite, but uncommitted work is not part of `main` and must be reported separately. See `docs/WORKTREE_RECONCILIATION.md`.

**Start here:** `docs/OPENCODE_HANDOFF.md`

Do not trust older claims that Goals 03–11 are complete. The canonical sequence is the numbered goal files at repository root. A fresh 2,000-vs-2,000 combat benchmark fails with no combat activity, so Goal 03 is active again. Do not begin Goal 04 until Goal 03's definition of done is re-established; do not begin Goal 05 until `04_LOGISTICS_AIR_NAVAL_INTEL.md` satisfies its definition of done.

## Working Directory

This repository (`/home/conor/repos/mandate-of-war`) is the working directory for this project.

## Required Reading Order

Before making changes, read:

1. `docs/OPENCODE_HANDOFF.md`
2. `docs/EXECUTION_LEDGER.md`
3. `00_PROJECT_CHARTER.md`
4. the active milestone document `03_ECONOMY_COMBAT_FACTIONS.md`, plus `02_SIMULATION_AND_SCALE.md` and `03_RENDERING_AND_CONTROLS.md` for the preserved baseline
5. `docs/ARCHITECTURE.md`
6. `docs/CURRENT_STATE.md`
7. `docs/NEXT_TASKS.md`
8. `EXTENSION_STATUS.md`
9. relevant ADRs under `docs/adr/`
10. `git status --short`, plus staged and unstaged diffs for files in scope

## Development Rules

1. **Milestones are sequential.** Complete and validate the active milestone before expanding scope.
2. **Preserve the working tree.** Existing staged, unstaged, and untracked files are user work. Never reset, clean, unstage, delete, or broadly reformat them without explicit approval.
3. **Investigate before editing.** Reproduce a failure, understand the relevant code path, make the smallest warranted change, test, then review the diff.
4. **Build before reporting completion.** Use a Release configuration and run `cmake --build build`.
5. **Tests must assert behavior.** Console claims, successful symbol lookup, or a zero exit code without assertions are not sufficient evidence.
6. **Keep benchmarks honest.** Separate simulation timing from Godot presentation/FPS. Use representative moving/active workloads and isolated simulation state.
7. **Use canonical Godot loading.** `godot/project/rts.gdextension` is the active descriptor; GDScript instantiates `RtsExtension`. Do not reintroduce `DynamicLibrary`, custom `project.godot` extension sections, or bare-name `dlopen`.
8. **Do not mislabel rendering.** The visible unit view uses Godot `MultiMeshInstance3D`. The current C++ `Renderer` is a dummy CPU-side path, not proven GPU rendering.
9. **Run milestone benchmarks** when scaling validation is required and record commands, build type, hardware, workload, and results in `docs/PERFORMANCE.md`.
10. **Update documentation after every task.** Keep `docs/CURRENT_STATE.md`, `docs/NEXT_TASKS.md`, and when relevant `docs/OPENCODE_HANDOFF.md` aligned with verified reality.
11. **Use Codex for required reviews** at the checkpoints named in the charter/milestone documents. Evaluate findings; do not blindly accept them.
12. **Do not commit by default.** Commit only when the user asks or the current task explicitly authorizes it.

## Continuous Execution and Loop Prevention

`docs/EXECUTION_LEDGER.md` is the durable authority for milestone selection and acceptance state. OpenCode session titles, summaries, chat history, and session-local todos are not authoritative after a restart or compaction.

1. Reconcile session todos with the execution ledger before substantive work and after every compaction or synthetic continuation. Close or cancel stale todos immediately.
2. Work only on the single milestone marked `ACTIVE`. Never infer the active milestone from a session title or from provisional later-system files.
3. Prefix each actionable todo with the stable acceptance ID from the execution ledger. An open todo without such an ID must be reconciled before work continues.
4. Update the todo state and execution-ledger evidence as soon as a criterion is verified. Do not leave documentation or review todos pending after reporting that work complete.
5. Never reopen a verified criterion unless a new failing test, benchmark, diff, or review finding invalidates its recorded evidence.
6. After two consecutive attempts with the same acceptance ID and no new diff, test result, benchmark result, or blocker evidence, do not repeat the same action. Invoke the required review when applicable, try a materially different diagnostic, or record a genuine blocker and stop automatic continuation.
7. Continuous sequence mode is enabled in the execution ledger. When every criterion for the active goal is verified, update the state/docs once and advance exactly once to the next numbered goal. Never skip a gate or return to an earlier goal without new invalidating evidence.

The repository-local OpenCode continuation hook enforces a maximum of two automatic resumes for an unchanged open-todo signature. Legitimate progress must change the todo ledger; a third identical idle state will not be resumed automatically.

## Required Gameplay Validation Contract

A gameplay feature is not complete merely because the project builds, the editor opens, static analysis passes, unrelated tests pass, or the code looks correct. When a relevant scenario exists, run it before claiming completion. Material gameplay changes should add or update a scenario unless existing coverage already proves the behavior.

Prefer evidence in this order: deterministic state assertions, reproducible scenario execution, screenshots for visual checkpoints, video for useful motion/sequence evidence, and honest performance metrics for scale-sensitive systems. Screenshots and videos do not replace state assertions.

Never make failed validation pass by removing/skipping assertions, unjustifiably widening tolerances, silently changing expected output, regenerating visual baselines, or adding test-only gameplay behavior. If acceptance criteria conflict with production intent, report the conflict and proposed correction.

For gameplay work, the final response must include:

```text
Implementation:
Validation scenarios run:
PASS/FAIL:
State assertions:
Visual artifacts:
Performance evidence:
Known limitations:
Files changed:
```

If a required scenario fails, report FAIL even when the build succeeds. On failure, inspect `report.json`, form one hypothesis, make one narrow change, and rerun `tools/validate <one-scenario>` before broadening. Do not regenerate videos or run scale benchmarks on every small iteration. See `docs/validation/AGENT_WORKFLOW.md`.

## Technology Stack

- **Simulation layer:** C++20, ECS, spatial partitioning, fixed-tick simulation
- **Presentation layer:** Godot 4.7.2 with `godot-cpp` GDExtension
- **Visible unit batching:** Godot `MultiMeshInstance3D`
- **Build system:** CMake
- **Current verified platform:** Linux x86_64

## Scale Targets

These remain aspirational until representative benchmarks prove them:

- 10,000 active units: excellent
- 25,000 active units: practical target
- 50,000 active units: stretch
- 100,000 total simplified entities: investigate

Never translate a headless simulation result into a rendering/FPS claim.

## Key Files

| Path | Purpose |
|------|---------|
| `docs/OPENCODE_HANDOFF.md` | Exact starting point, blockers, and definition of done |
| `docs/EXECUTION_LEDGER.md` | Durable active-goal and acceptance state |
| `docs/CURRENT_STATE.md` | Evidence-backed snapshot of what exists |
| `docs/NEXT_TASKS.md` | Ordered remaining work |
| `godot/project/rts.gdextension` | Active GDExtension descriptor |
| `gdextension/gd_extension.cpp` | Active Godot native binding |
| `godot/project/main.gd` | Godot presentation and controls |
| `godot/project/test.gd` | Native integration smoke test |
| `src/simulation/` | C++ simulation core |
| `benchmark/scale_benchmark.cpp` | Accepted assertion-backed Goal 02 movement/formation harness |
| `benchmark/benchmark.cpp` | Provisional integration diagnostic; not milestone benchmark evidence |
| `docs/adr/` | Architecture decisions |

## Baseline Commands

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

./Godot_v4.7.2-stable_linux.x86_64 \
  --headless --editor --path godot/project --quit-after 8

./Godot_v4.7.2-stable_linux.x86_64 \
  --headless --path godot/project --script res://test.gd

./Godot_v4.7.2-stable_linux.x86_64 \
  --headless --path godot/project --quit-after 30

./build/rts_scale_benchmark 1000 100 100
./build/rts_scale_benchmark 5000 100 100
./build/rts_scale_benchmark 10000 100 100
./build/rts_scale_benchmark 25000 100 100
ctest --test-dir build --output-on-failure
```

CTest currently discovers three executables: `rts_tests`, `test_portable_snapshot`, and `rts_integration_tests`. Report both the CTest discovery result and the direct runner assertion counts; do not treat console checkmarks from `rts_benchmark` as tests.
