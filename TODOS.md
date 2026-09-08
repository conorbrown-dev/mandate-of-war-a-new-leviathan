# Task List - Codex Review Remediation

**Session:** 2026-09-05
**Status:** In progress - Phase 1: Restore honest validation

## Phase 1 — Restore honest validation

- [ ] P1.1: Review and repair CMakeLists.txt to include all test executables
- [ ] P1.2: Register test_portable_snapshot with CTest
- [ ] P1.3: Add rts_logistics_benchmark target to CMakeLists.txt
- [ ] P1.4: Ensure all 126 tests are actually run and counted
- [ ] P1.5: Run full test suite and document exact assertion counts
- [ ] P1.6: Update docs/PERFORMANCE.md with corrected benchmark commands
- [ ] P1.7: Run CTest with --output-on-failure and report results

## Phase 2 — Fix Goal 03 review findings

- [ ] P2.1: Make economy/production resolution deterministic for shared resources
- [ ] P2.2: Fix combat benchmark projectile metric (should be cumulative fired counter)
- [ ] P2.3: Fix benchmark output order: avg, p50, p95, max
- [ ] P2.4: Add behavior tests for deterministic shared-resource competition
- [ ] P2.5: Add behavior tests for accurate combat metrics

## Phase 3 — Fix Goal 04 logistics architecture

### A. Intelligence ownership and lifecycle
- [ ] P3.A.1: Revert unsafe global rule in ComponentManager::remove_entity
- [ ] P3.A.2: Create dedicated intelligence-memory/archive system
- [ ] P3.A.3: Prevent entity-ID reuse causing stale intelligence attachment
- [ ] P3.A.4: Update callers/tests to use intelligence-memory API
- [ ] P3.A.5: Store all required fields and define deterministic transitions
- [ ] P3.A.6: Add tests for destruction persistence, ID reuse, transitions

### B. Moving facilities and safe-return scaling
- [ ] P3.B.1: Synchronize carrier movement through authoritative path
- [ ] P3.B.2: Replace full facility scan with spatial index
- [ ] P3.B.3: Avoid clearing aircraft estimates on carrier movement
- [ ] P3.B.4: Preserve deterministic nearest-facility tie-breaking
- [ ] P3.B.5: Add tests for carrier movement, cache invalidation, capacity

### C. Runway/deck scheduling
- [ ] P3.C.1: Prevent takeoff starvation under sustained landing traffic
- [ ] P3.C.2: Define behavior when runway/deck becomes unusable
- [ ] P3.C.3: Add assertions for mixed traffic, bounded progress, unusable transitions

### D. Naval endurance
- [ ] P3.D.1: Validate non-finite/negative inputs
- [ ] P3.D.2: Ensure stranded vessels cannot continue normal movement
- [ ] P3.D.3: Define and test deterministic unstranding thresholds

### E. Replace invalid logistics benchmark
- [ ] P3.E.1: Create new assertion-backed logistics benchmark
- [ ] P3.E.2: Spawn aircraft, naval vessels, airbases, carriers
- [ ] P3.E.3: Exercise recovery-facility lookup and safe-return
- [ ] P3.E.4: Move carriers through authoritative facility-update path
- [ ] P3.E.5: Vary aircraft/facility counts independently
- [ ] P3.E.6: Assert nonzero operations, state changes, cache stats
- [ ] P3.E.7: Run and document benchmark results

## Phase 4 — Repair Goal 05 design

### A. Content IDs
- [ ] P4.A.1: Select one normative ID representation
- [ ] P4.A.2: Use consistently across all docs and code
- [ ] P4.A.3: Fix idempotent re-registration vs collision detection
- [ ] P4.A.4: Add tests for normalization, idempotency, forced collisions

### B. Manifest/dependency system
- [ ] P4.B.1: Use real YAML parser or define limited format
- [ ] P4.B.2: Validate manifest schema/version/dependencies
- [ ] P4.B.3: Prevent path traversal/symlink escape
- [ ] P4.B.4: Implement deterministic topological ordering
- [ ] P4.B.5: Test missing deps, cycles, version constraints

### C. Lua/mod safety
- [ ] P4.C.1: Remove false claims about Lua sandboxing
- [ ] P4.C.2: Define runtime, version, allowed APIs
- [ ] P4.C.3: Document security boundaries

### D. Asset reproducibility
- [ ] P4.D.1: Define canonical input serialization
- [ ] P4.D.2: Pin pipeline versions and settings
- [ ] P4.D.3: Separate metadata from cache key

### E. Map format longevity
- [ ] P4.E.1: Select normative version-1 format
- [ ] P4.E.2: Define exact binary framing and validation
- [ ] P4.E.3: Resolve timestamp/hash contradiction
- [ ] P4.E.4: Add tests for all required concepts

## Phase 5 — Documentation and handoff

- [ ] P5.1: Update EXECUTION_LEDGER.md with G04-REVIEW and G04-CLOSEOUT
- [ ] P5.2: Update CURRENT_STATE.md with full evidence
- [ ] P5.3: Update NEXT_TASKS.md with remaining work
- [ ] P5.4: Update OPENCODE_HANDOFF.md with current state
- [ ] P5.5: Update PERFORMANCE.md with corrected numbers
- [ ] P5.6: Run full validation and report results
