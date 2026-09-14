# Mandate of War — Goal 0A: Reproducible, Human-Operable Foundation

You are working in the `mandate-of-war-a-new-leviathan` repository.

Your task is to restructure the project’s operational foundation so that a human developer can clone it, understand its current state, build/run it, make a small scoped change, and validate that change without relying on an AI agent’s hidden context.

This is not a gameplay-feature task and not a full engine rewrite. Preserve existing gameplay behavior unless a change is necessary to make the project buildable, reproducible, or clearly structured.

## Why this matters

The project is a hybrid architecture:

* Native C++ simulation/core
* Godot 4 + GDExtension for rendering, UI, editor, and presentation
* CMake-based native build
* Intended long-term target: very large RTS battles, deterministic validation, mod-friendly data, and cross-platform support

The current project has structural problems that make both human and AI work unreliable:

* A clean checkout is not currently reproducible.
* Required Godot/GDExtension dependencies and generated descriptors are missing or ignored.
* Documentation contradicts itself about which goals are complete.
* Machine-specific paths and Linux-specific assumptions are embedded in scripts/configuration.
* The repository includes derived artifacts and validation output that should not define the source state.
* There are duplicate/stale-looking extension/source paths with unclear ownership.

Your priority is to establish a trustworthy development foundation.

## Non-negotiable rules

1. Do not rewrite Git history, force-push, or delete source assets.
2. Do not perform a broad simulation, ECS, pathfinding, networking, or rendering rewrite in this task.
3. Do not silently claim a build, benchmark, or visual test passed if the environment cannot actually run it.
4. Do not remove tracked generated/derived files until you first produce a clear inventory of what is source, generated, validation evidence, temporary output, or machine-local state.
5. Preserve existing user-authored work. Make focused, reviewable commits or clearly grouped changes.
6. Prefer explicit scripts, documented commands, and checked-in configuration over assumptions about the developer’s machine.
7. Godot remains the presentation/editor layer. Do not propose an engine migration.

## Deliverables

### 1. Establish one canonical project status

Create `docs/STATUS.md` as the single current-state document.

It must include:

* The exact current Git commit SHA when this work begins.
* What is known to build/run/validate today.
* What is unverified or blocked.
* The active milestone: `Goal 0A — Reproducible, Human-Operable Foundation`.
* A concise list of known architecture realities:

  * C++ simulation is authoritative.
  * Godot is presentation/UI/editor.
  * GDExtension is the bridge.
  * Current simulation remains single-threaded/global-state-based until a later refactor.
  * Current benchmark evidence does not prove 10k+ combined-arms combat readiness.
* A short “Do not trust as current” section identifying stale or conflicting status claims found elsewhere.

Then update README, AGENTS.md, handoff documents, and execution ledgers so they link to or defer to `docs/STATUS.md` rather than contradicting it. Do not erase useful historical detail; mark historical claims as historical when appropriate.

### 2. Make clean-clone setup explicit and reproducible

Audit every required build/run dependency:

* Godot version
* `godot-cpp` source and exact version/commit
* GDExtension descriptor
* CMake/toolchain requirements
* Python tooling
* Blender only if genuinely required for normal development

Implement the safest practical dependency strategy. Prefer one of:

* A pinned Git submodule for `godot-cpp`, or
* CMake `FetchContent` pinned to an exact revision.

Do not depend on an untracked local `vendor/` directory.

Ensure the repository contains or reliably generates the required `.gdextension` descriptor. A descriptor is source configuration and must not be treated as disposable build output.

Remove or replace machine-specific absolute paths such as `/home/conor/...`.

Create clear developer commands/scripts for:

```bash
# first-time setup
# build native extension
# launch Godot/project
# run headless/static validation
# run available tests
```

Use a small cross-platform-friendly approach where reasonable. If Linux-only support remains the actual verified state, document that honestly instead of implying macOS/Windows support is proven.

### 3. Clean up repository ownership without risky deletion

Create `docs/REPOSITORY_INVENTORY.md` with a concise classification of:

* Source code
* Hand-authored Godot scenes/scripts/assets
* Third-party dependencies
* Generated Godot imports
* Build output
* Validation artifacts/screenshots/videos/logs
* Python cache files
* Known duplicate or inactive source paths

Then improve `.gitignore` so future generated output is not added accidentally.

Do not remove existing tracked validation artifacts or generated files in this task unless all of the following are true:

1. They are clearly reproducible or disposable.
2. Their removal does not remove source assets or irreplaceable evidence.
3. You list them in the inventory.
4. You summarize the removal in the final report.

Prefer moving ongoing validation output under one ignored `validation/output/` location while retaining any intentionally committed example evidence in a clearly named location.

### 4. Clarify architecture ownership

Create `docs/ARCHITECTURE.md` for a human developer. Keep it practical, not academic.

It must identify:

* The authoritative simulation entry points.
* The active GDExtension entry points and build target.
* The Godot presentation entry scene/script.
* The command/data flow: input → bridge → simulation → snapshot/events → Godot visuals/UI.
* Which duplicate or uncompiled paths are active, inactive, legacy, or require later reconciliation.
* Where a developer should make each kind of change:

  * unit/faction/balance data
  * simulation rule
  * pathfinding behavior
  * Godot visual/presentation change
  * HUD/input change
  * benchmark/test
* A short “current technical debt intentionally deferred” section covering:

  * global `Simulation` singleton
  * broad GDExtension API
  * map-backed ECS/spatial structures
  * synchronous flow-field/pathfinding work
  * oversized `main.gd`
  * partial mod-data model

Do not attempt to solve all of that debt in this task. Make the current boundaries understandable.

### 5. Add a human-oriented contribution workflow

Create `docs/DEVELOPMENT_WORKFLOW.md` with:

* A clean-clone quick start.
* How to run a named smoke scenario.
* How to make and validate a small content/UI change.
* A “before asking an agent to change code” checklist.
* A “before accepting an agent’s work” checklist:

  * inspect files changed
  * run relevant command
  * compare expected behavior
  * confirm benchmark/test evidence
  * update status only when evidence exists
* A task template for future work:

```md
## Goal
## Allowed modules/files
## Explicit non-goals
## Acceptance criteria
## Validation command(s)
## Evidence required
## Rollback notes
```

The workflow should make it feasible for Conor to work directly in the project when AI credits are unavailable.

### 6. Validate honestly

Run every validation command that is available in the environment.

At minimum:

* Run existing Python/static tests.
* Run configuration/static checks you add.
* Attempt the documented native configuration/build only if the required tools/dependencies are available.

If a build cannot run, clearly state:

* The exact blocker.
* Whether it is an environment limitation or a repository limitation.
* The exact command a developer should run on a prepared machine.

Do not fabricate performance or visual validation.

## Required final report

At the end, provide:

1. A concise summary of changed files and why.
2. Exact setup/build/run/validation commands.
3. What was successfully verified.
4. What remains blocked or unverified.
5. Any files deliberately not removed and why.
6. A recommended next task, limited to one of:

   * Goal 0B: Thin the Godot/GDExtension boundary.
   * Goal 0C: Split Godot presentation responsibilities.
   * Goal 1: Create a data-driven content registry.
   * Goal 2: Pathfinding and hot-path performance foundation.

## Definition of done

This task is done only when a new developer can read `docs/STATUS.md`, `docs/ARCHITECTURE.md`, and `docs/DEVELOPMENT_WORKFLOW.md`; identify the authoritative code paths; and follow documented commands without needing undocumented paths, missing local vendor folders, or contradictory milestone claims.
