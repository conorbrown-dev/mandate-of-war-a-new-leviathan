# Model Integration Review

Reviewed against `docs/Mandate_of_War_Model_Integration_Prompt_Pack/09_CODEX_REVIEW.md`.
This is a review of the feature worktree, not a claim that GLB-backed gameplay
is ready to ship.

## Critical issues

None found. Gameplay statistics remain in `data/unit_faction_stats.json`; the
visual registry consumes only `visual_id` and presentation data. No visual code
writes simulation state, and replay/network serializers do not reference GLB
paths.

## High-priority issues

1. **Prototype scale-profile visibility regression — resolved in this worktree** —
   `godot/project/main.gd`, `_start_scale_profile()` →
   `_create_unit_multimesh()` → `_spawn_units()`. With
   `RTS_PROTOTYPE_VISUALS=1`, `_create_unit_multimesh()` hides the MultiMesh,
   but `_spawn_units()` does not create `UnitVisualRoot` instances. The scale
   profile therefore had no visible units. `VisualPresentationPolicy` now
   disables prototype wrappers whenever `RTS_PROFILE_FRAMES` is enabled, so
   `_create_unit_multimesh()` retains the batched MultiMesh path. The policy
   test covers every requested/profile combination. A future batched prototype
   renderer may opt into profile coverage only after it can render the profile
   workload itself.

2. **Actual GLB import — resolved for the current prototype donor set** —
   The 13 CC0 Godot-ready donor GLBs referenced by the 15 definitions are now
   present under `res://assets/source_3d/cc0/` with their pipeline metadata.
   Godot headless editor import completed for all 13 and the validator reports
   zero missing GLBs. These remain prototype donors, not final fictional art;
   bounds, triangle budgets, LOD hierarchy, and authored hardpoints still need
   shipping-grade validation.

3. **Native visual-ID integration — resolved with a local dependency cache** —
   `src/ecs/components/factions.cpp` and `gdextension/gd_extension.cpp`.
   The source tree intentionally ignores `vendor/`, so a clean worktree does
   not contain the local GLM and godot-cpp cache. A worktree-local symlink to
   the already-present main-checkout cache enabled a Release build and CTest.
   `test_native_visual_ids.gd` additionally verifies all 11 production unit
   types resolve through the actual `RtsExtension.get_unit_visual_id()` binding
   and through `VisualDefinitionRegistry`.
   This is local validation evidence only; a fresh-clone dependency bootstrap
   remains a project setup concern. With the local descriptor and dependency
   cache present, the headless editor scan and normal headless smoke scripts
   both complete successfully.

## Performance risks

1. **Independent wrapper nodes cannot be a 10k render path** —
   `godot/project/unit_visual_root.gd`, `update_lod()`, and
   `profile_visual_wrapper.gd`. Measured 10,000 fallback wrappers: 368.855 ms
   creation and 42.945 ms for one strategic LOD pass. The current cap of 200 in
   `main.gd` is appropriate. Smallest correction: build a central batched
   renderer for common visual/LOD tiers before raising that cap.

2. **Imported LOD naming convention is still only a hook** —
   `UnitVisualRoot._set_imported_lod_visibility()`. It selects `_LOD0/_LOD1/
   _LOD2` nodes but no imported hierarchy has been inspected. Smallest
   correction: validate the actual Godot-imported scene trees and normalize
   Blender output to one top-level LOD hierarchy per unit.

## Determinism and network risks

No direct GLB path is serialized: `visual_id` is presentation metadata on
`UnitPrototype`, while existing network/replay code serializes simulation
entities and state. `UnitVisualRoot.apply_presentation_pose()` only modifies
presentation nodes.

Future mod content must still use the compatibility helper when negotiating a
   session. `VisualPackCompatibility` now provides a stable pack ID, version, and
   SHA-256 definition hash, and rejects mismatched or empty hashes while retaining
   gameplay unit IDs as the authoritative network identity. The native
   `ConnectionHandshake` now transports these fixed-width fields, has a
   round-trip test, and `NetworkManager` can reject mismatches through
   `set_expected_handshake()` before a join proceeds. Outgoing callers still
   need to populate the fields from the presentation manifest.

## Asset-pipeline risks

1. **Donor models are not approved game assets yet.** The audited inventory and
   `faction_prototype_mapping.json` correctly label them prototype/donor assets,
   including immediate replacement priority for the reused T-34 recon role.
2. **Metadata validation now distinguishes prototype warnings from shipping
   blockers.** `VisualAssetValidator` reads every imported sidecar for bounds
   and triangle counts, reports extreme prototype bounds and triangle-budget
   overruns, and fails shipping content with unresolved validation issues.
   Material-reference inspection is now covered, with mesh/material/texture
   counts in the report. Imported-node LOD visibility is also validated for
   shipping definitions; current donors remain strategic-marker prototypes.
3. **Hardpoint fallback can mask missing art markers.**
   `UnitVisualRoot._configure_hardpoints()` synthesizes anchors. This is useful
   for prototypes, but a shipping visual could silently use a generic muzzle.
   `status=shipping` now fails validation when a weapon-bearing definition
   lacks an authored muzzle offset; prototype donors retain warnings until art
   cleanup provides anchors.

## Missing tests

- Shipping GLB test covering material/texture references and exactly one active
  imported LOD (fixture coverage exists; a production authored LOD set is still
  pending).
- Multiplayer negotiation integration test for exchanging the visual-pack
  handshake (the helper-level compatibility test now exists).

## Recommended implementation order

1. Replace/normalize prototype donor art and provide authored LOD and hardpoint
   data; the two extreme source-bound donors now have runtime scales, while
   triangle-budget cleanup and shipping validation remain required.
2. Implement batched visual tiers before increasing the 200-wrapper prototype cap.
3. Call the new `RtsExtension` handshake adapters from each multiplayer join
   path using `VisualPackCompatibility.handshake()`, then add the remaining
   shipping tests.
4. Make the ignored vendor/descriptor bootstrap reproducible for a clean clone.

The bootstrap script now covers the dependency-linking portion of this item;
the source checkout still has to be supplied explicitly by the operator.
