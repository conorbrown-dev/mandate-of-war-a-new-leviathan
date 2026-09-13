# Project Validation Audit

**Date:** 2026-09-13  
**Scope:** root goal documents, `docs/`, the feature/fix backlog, model-integration prompt pack, mandate-validation prompt pack, and validation package.

## Result

`tools/test` is the canonical executable gate. It covers the Release CMake
build and CTest, both playable Godot routes, the Goal 10 terrain contract, all
model-integration and map-editor harnesses, and every registered gameplay and
performance scenario. Current 2026-09-13 evidence passed CTest 3/3, the
framework tests, all Godot harnesses, and all registered scenarios.

The durable status documents remain inconsistent with the repository's
authoritative instructions: they claim Goals 03–11 are verified, while
`AGENTS.md` keeps `DOC-RECONCILE` and `G03-COMBAT-BENCH` active. This audit
does not advance any numbered milestone. The active benchmark now produces
real combat activity, but its 12 ms cache-hit average was host-sensitive in
three current invocations (12.133 ms FAIL, then 11.945 ms PASS and 11.733 ms
PASS). It needs a stable acceptance measurement before Goal 03 can close.

The root goals are not claims that every future commercial-game feature or art
polish item is finished. `VERIFIED` means the goal's acceptance table has
current code-level evidence. Hosted CI, desktop visual review, external
services/devices, and explicitly deferred art work remain outside local proof.

## Evidence matrix

| Documentation scope | Current executable proof | Boundary |
|---|---|---|
| Root goal documents | `tools/test`, native integration/benchmarks, Goal 10 terrain harness, and registered scenarios | Goals 04–11 remain gated by the active Goal 03 reconciliation. Earlier historical completion prose is not current acceptance evidence. |
| `docs/features-fixes-backlog` | Implemented fixes are covered by the native/Godot suites | Art-only and future-polish entries remain backlog items. |
| Model integration prompt pack | Model/presentation harnesses plus `validate_faction_prototype_mapping.py`; all pass, including 15 reference-model loads and zero missing GLBs | Prompt prose is guidance; production-quality asset replacement still follows its listed review requirements. |
| Mandate validation prompt pack | `tools/validate all`, forced-failure propagation, report-schema checks, rendered baseline/video evidence, and `CODEX_VALIDATION_REVIEW.md` | Local proof does not imply hosted CI or cross-platform renderer parity. |
| `docs/validation` | `tools/test all`, scenario catalog, and retained reports under `validation/artifacts/` | Headless PASS proves state assertions, not desktop usability or GPU FPS. |

## Commands and results

```text
./tools/test                             PASS outside the sandbox (CTest 3/3; framework, Godot, and registered scenarios)
python3 scripts/validate_faction_prototype_mapping.py  PASS (32 checks)
./tools/validate combat_benchmark_2000                PASS (11.733 ms; 2,000 destroyed; 1,038 projectiles)
```

This audit does not alter accepted visual baselines and does not claim
unperformed live, device, hosted, or compliance validation.
