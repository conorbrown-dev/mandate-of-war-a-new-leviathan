# Development workflow

Read [STATUS.md](STATUS.md) first. The commands below are verified only for the stated Linux x86_64 toolchain until another platform is explicitly tested.

## Clean-clone quick start

Prerequisites: Git, CMake 3.20+, a C++20 compiler, OpenSSL development headers, Python 3, and Godot **4.7.2**. `godot-cpp` at `9c8aeff0f58ad030f3d1030e8262de1322cd0ccd` and GLM 1.0.1 are fetched by CMake; Blender is not required for normal build, test, or play workflows.

```bash
git clone <repository-url> mandate-of-war
cd mandate-of-war
export GODOT_BIN=/absolute/path/to/Godot_v4.7.2-stable_linux.x86_64  # if not on PATH
python3 scripts/dev.py configure
python3 scripts/dev.py build
python3 scripts/dev.py smoke
```

`configure` needs internet on its first run and stores dependencies only below the ignored `build/` directory. The GDExtension descriptor is checked in and the build copies its two Linux shared libraries into `godot/project/bin/`.

## Run and validate

```bash
python3 scripts/dev.py run
python3 scripts/dev.py test
python3 tools/validate.py native_extension_smoke
python3 tools/validate.py gdextension_boundary
python3 tools/validate.py hud_bridge
python3 tools/validate.py basic_selection_move
```

The first is an editor launch; close Godot to return to the shell. The named smoke scenario tests class registration and a small native operation; `gdextension_boundary` checks the intentionally retained and retired public bridge methods; `hud_bridge` runs the active scene through aggregate-backed HUD state. Gameplay scenarios write disposable output below `validation/artifacts/`.

## A small content or UI change

1. Read `ARCHITECTURE.md` and choose one boundary: for example edit a value in `data/unit_faction_stats.json`, or a presentation-only label in `godot/project/command_hud.gd`.
2. Make the smallest scoped edit. Do not modify both authority layers merely to make a check pass.
3. Run `python3 scripts/dev.py build`, then the smallest relevant test or `tools/validate.py` scenario. For a Godot UI change, run the matching Godot contract and inspect the editor/runtime when available.
4. Record what command actually passed; do not update status from an assumed result.

## Before asking an agent to change code

- State the goal, files/modules in scope, and explicit non-goals.
- Name the observable acceptance criteria and validation command.
- Say whether visual evidence, benchmark evidence, or only a state assertion is required.
- Identify any user-owned dirty work that must be preserved.

## Before accepting an agent's work

- Inspect files changed and confirm scope.
- Run the relevant command yourself where practical.
- Compare behavior with the stated expected result.
- Confirm benchmark/test evidence is meaningful and successful.
- Update `STATUS.md` only when current evidence exists.

## Future task template

```md
## Goal
## Allowed modules/files
## Explicit non-goals
## Acceptance criteria
## Validation command(s)
## Evidence required
## Rollback notes
```
