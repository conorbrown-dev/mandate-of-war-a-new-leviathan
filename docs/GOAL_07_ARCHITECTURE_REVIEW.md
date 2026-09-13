# Goal 07 Architecture Review

**Date:** 2026-09-12
**Scope:** deterministic AI foundation acceptance.

## Findings evaluated

1. **Authoritative commands:** AI decisions use the same `Simulation` command
   issue methods as human input. Tests require generated ATTACK, MOVE, BUILD,
   RESEARCH, and HARVEST commands to enter the authoritative queues/state.

2. **Tactical behavior:** the AI selects one visible, deterministic focus target;
   favors threats near its command base; moves into engagement range; and sends
   damaged units to the base. It does not query hidden enemy state.

3. **Operational behavior:** mobile units are sorted and partitioned into
   bounded eight-unit groups. Front and staging positions derive only from the
   selected visible target, public objective, and own base.

4. **Strategic behavior:** deterministic research/production selection remains
   data-driven through production validation. Expansion chooses the highest
   value unclaimed resource, with entity-ID tie-breaking, and claims it via an
   ordinary HARVEST command.

5. **Determinism and performance:** equal worlds generate byte-identical
   commands and plans. A 128-friendly/128-enemy active-force decision is
   asserted to remain below the 50 ms fixed-tick budget.

## Accepted boundary

This is a deterministic heuristic foundation, not a full doctrine, transport,
naval, air, or multi-theater planner. Later goals must add those behaviors only
with their own explicit scenario proof.

## Verdict

**ACCEPT.** The Goal 07 criteria have current behavioral evidence and the
Release build, CTest 3/3, `rts_tests` 20/20, and direct integration 176/176
all pass.
