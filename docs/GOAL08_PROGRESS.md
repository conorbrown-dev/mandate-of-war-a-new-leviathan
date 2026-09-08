# Goal 08 Progress Reconciliation

Updated 2026-09-07 after reconciling the Goal 08 vertical-slice work with the
pre-existing dirty worktree.

## Verified in this slice

- `G08-SCENARIO`: the Broken Strait scenario is wired into the Godot startup
  path with 20 player units and 26 AI units.
- `G08-COMMANDS`: authoritative move and stop orders are exposed through the
  native binding and Godot controls. The path uses signed centiunit protocol
  coordinates, exact-tick validation, ownership checks, bounds/walkability
  checks, duplicate rejection, and atomic queue insertion. Reset clears stale
  queued commands.
- AI tactical/operational command generation now filters by controlled faction
  and assigns the originating player id; same-faction targeting is covered by
  an assertion-backed test.
- Validation evidence: Release build succeeds; CTest reports 3/3; direct
  runners report 16/16, 21/21, and 131/131 assertions; the native Godot smoke
  test passes; headless Broken Strait startup passes without the former AI
  debug flood.

## Still open for Goal 08

- Attack, patrol, return, build, and research orders remain to be implemented
  and verified end to end.
- The existing map loader remains provisional: it is not yet the authoritative
  validated loader, has unsafe default initialization and negative-coordinate
  parsing gaps, and its hash omits map content fields. This blocks closing
  `G08-SCENARIO`.
- The headless movie/offscreen renderer attempt is diagnostic only and is not
  acceptance evidence.

## Reconciliation boundary

This commit contains the Goal 08 document, Godot presentation/control changes,
and smoke-test updates. The related C++ command, AI, serializer, transport,
and behavior-test edits remain in the shared dirty worktree because those files
overlap pre-existing Miriam changes. They were rebuilt and tested, but are not
silently folded into this documentation/presentation commit.

