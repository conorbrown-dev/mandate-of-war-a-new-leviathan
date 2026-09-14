# Repository inventory

This is a classification, not a deletion list. Goal 0A deliberately retains tracked derived files and validation evidence until a later, reviewed cleanup.

| Classification | Locations | Ownership/action |
| --- | --- | --- |
| C++ source | `src/`, `gdextension/`, `tests/`, `benchmark/` | Hand-authored project source and assertion/benchmark code. |
| Godot source | `godot/project/*.gd`, `*.tscn`, `*.gdshader`, `project.godot`, `rts.gdextension`, assets | Hand-authored scenes/scripts/configuration and source assets. |
| Gameplay/content data | `data/`, `test_maps/`, `test_mods/` | Source data and test fixtures. |
| Third-party dependencies | CMake `FetchContent` into `build/_deps`; legacy ignored `vendor/` | Active dependencies are pinned by CMake. Do not rely on `vendor/`. |
| Generated Godot state | `godot/project/.godot/`, new `*.import` files | Ignored cache/import products. Existing tracked imports are retained historical evidence. |
| Build output | `build/`, `godot/project/bin/`, root `.so`/executables | Reproducible, ignored output. |
| Validation output | `validation/artifacts/` | Disposable, ignored reports/logs/screenshots/video. |
| Intentional example evidence | `validation/baselines/` | Tracked visual references; change only with a reviewed visual task. |
| Python caches | `__pycache__/`, `*.pyc` | Ignored generated interpreter cache. |
| Legacy/inactive extension paths | `src/gdextension/`, `gdextension/gd_extension.hpp` | Not compiled by root CMake; retain until a dedicated reconciliation task. |

`downloaded_assets/` contains source/reference art supplied to the project; it is not treated as disposable build output. No tracked file was removed for Goal 0A.
