# Model Integration Pack Worktree

This branch implements the Mandate of War model integration prompt pack
independently from the main working tree.

## Audit baseline

The first prompt-pack stage is represented by:

- `scripts/audit_model_integration_assets.py`, a path-configurable, read-only
  audit tool for the prepared CC0 library.
- `data/provenance/cc0_model_inventory.json`, the generated inventory of the
  24 current Godot-ready GLBs.

The inventory records stable donor-only IDs, source and metadata paths,
category, prototype role, bounds, triangle count, duplicate flag, provenance
source ID, and preliminary import status. It intentionally does not copy GLBs
into this worktree. The current library remains in the parallel main checkout
until the asset handoff is explicitly merged or otherwise versioned.

Regenerate from the repository root with:

```bash
python3 scripts/audit_model_integration_assets.py \
  ../near-future-rts-game/downloaded_assets/Mandate_of_War_CC0_Asset_Import_Pack \
  data/provenance/cc0_model_inventory.json
```

Next: place/version the approved Godot-ready GLBs under the project asset tree,
then use the inventory to replace the prototype paths with validated imports.

## Stage 02 visual-definition layer

`data/unit_faction_stats.json` now supplies each gameplay unit with a stable
presentation-only `visual_id`. `VisualDefinitionRegistry` resolves that ID from
`godot/project/visuals/visual_definitions.json`, caches successful resource
loads, rejects duplicate/malformed definitions, reports missing models, and
returns a development fallback for unknown IDs. Definitions include transforms,
selection radius, LOD intent, hardpoints, faction material intent, and donor
status. They do not contain gameplay statistics.

The standalone registry test passes:

```bash
/home/conor/repos/near-future-rts-game/Godot_v4.7.2-stable_linux.x86_64 \
  --headless --path godot/project --script res://test_visual_registry.gd
```

The fresh CMake build is currently blocked before compilation of this work by a
missing `glm/glm.hpp` dependency in the clean worktree. That environment issue
must be resolved before native integration tests can validate the added
`visual_id` field.

## Stage 03 reusable visual wrapper

`UnitVisualRoot` provides the prompt-pack scene structure without a per-unit
process loop or render-mesh physics:

```text
UnitVisualRoot
  ModelRoot
  OptionalTurretRoot
  OptionalHardpointMarkers
  SelectionVisual
  DebugVisuals
```

It consumes only `VisualDefinitionRegistry` data, applies scale/rotation/ground
offset corrections, accepts simulation transforms, and uses a lightweight box
fallback until an approved GLB exists. `test_unit_visual_root.gd` passes with
six assertions, including fallback safety, transform driving, selection, and
hardpoint access.

The available CC0 library has no approved logistics-truck prototype in its
Godot-ready folder, so the required Stage 03 category coverage remains an asset
handoff gate rather than a fabricated visual mapping.

## Stage 04 LOD, footprints, and benchmark

`UnitVisualRoot` now has a central-call `update_lod(camera_distance)` hook. It
uses detailed imported visuals near the camera, suppresses per-child imported
`_LOD0/_LOD1/_LOD2` meshes so they cannot render simultaneously, and swaps to
a single strategic hex marker at long range. Fallback materials are shared by
color. `presentation_footprint()` returns data only; no render-mesh collision,
physics body, or collision shape is created by the visual wrapper.

Headless fallback-wrapper benchmark results on this host:

| Instances | Create | One strategic-LOD pass |
|---:|---:|---:|
| 100 | 4.942 ms | 0.253 ms |
| 1,000 | 35.702 ms | 4.574 ms |
| 10,000 | 368.855 ms | 42.945 ms |

This deliberately proves the correctness path, not 10k render readiness:
10,000 independent wrapper nodes are too expensive for a per-frame LOD pass.
The production integration must batch/instance common visual tiers and update
LOD in a central, amortized presentation pass. The benchmark uses fallback
boxes because approved GLBs are not in this worktree; it does not claim GPU
frame rate or final asset memory usage.

## Stage 05 movement, turrets, and hardpoints

`UnitVisualRoot` now separates the authoritative hull transform from
presentation-only turret yaw, gun elevation, and aircraft bank. It normalizes
stable hardpoint IDs (`turret_root`, `gun_root`, `muzzle`, and
`weapon_primary`) rather than reading donor node names. Definitions can provide
`hardpoint_offsets`; the Elite MBT prototype records a muzzle offset as the
first example. Missing authored markers use predictable wrapper fallback
anchors until Blender cleanup replaces them.

`hardpoint_transform()` supplies projectile/FX presentation positions without
authorizing a shot, changing a simulation heading, or adding naval/aircraft
physics. The wrapper test passes 14 checks, including hull/turret separation,
muzzle movement, and preservation of the authoritative hull position.

## Stage 06 normal spawn integration

Normal scenario spawning now has an optional presentation bridge:

```text
UnitType (simulation/content identity)
  -> visual_id (unit_faction_stats.json)
  -> VisualDefinitionRegistry
  -> VisualSpawnBridge
  -> UnitVisualRoot
```

`RtsExtension.get_unit_visual_id(unit_type)` exposes the content-defined ID;
neither scenarios, replay data, nor factory/debug spawn calls supply GLB paths.
`VisualSpawnBridge` is the shared presentation entry point and passes three
normal-spawn/fallback assertions.

The branch intentionally keeps this mode behind `RTS_PROTOTYPE_VISUALS=1` and
caps it at 200 wrappers (`RTS_PROTOTYPE_VISUAL_LIMIT`). Default presentation
remains batched MultiMesh, consistent with the Stage 04 benchmark. This is a
prototype integration hook, not a 10k final rendering claim. GLB-backed visual
coverage, destruction/wreck replacement, and factory completion integration
remain gated on the asset handoff and native build dependency.

## Stage 07 temporary faction mapping

`data/provenance/faction_prototype_mapping.json` and
`docs/faction_prototype_mapping.md` explicitly map eight temporary fictional
prototype IDs to audited donor assets. The table includes faction, gameplay
role, stable prototype/visual IDs, donor ID, replacement priority, reason, and
future-art direction. All donors are present in the inventory. Historical
assets such as the T-34 are labeled prototype placeholders; the reused T-34
recon mapping has `immediate` replacement priority because no dedicated
Godot-ready recon donor is available.

`scripts/validate_faction_prototype_mapping.py` verifies every mapped donor and
visual ID, unique prototype ID, and replacement priority. It passes 32 checks.

## Stage 08 asset validation and viewer

`VisualAssetValidator` validates definition shape, duplicate IDs, scale safety,
model presence, LOD/hardpoint metadata, and fallback-marker warnings. It emits
both machine-readable JSON and concise Markdown reports through `write_reports`.
At the time of this stage the expected result was that all 15 definitions
reported `missing_glb`. Stage 12 supersedes that temporary condition: all
referenced donor GLBs are now imported and validator coverage requires zero
missing GLBs.

`visual_asset_viewer.tscn` provides a reusable developer scene with category
filtering, visual selection, orbit/zoom controls, definition/provenance detail,
selection footprint display, and the same safe fallback visuals. Run it from
Godot or with:

```bash
/home/conor/repos/near-future-rts-game/Godot_v4.7.2-stable_linux.x86_64 \
  --path godot/project res://visual_asset_viewer.tscn
```

The validator test passes five assertions and the viewer scene loads headlessly
without parse or runtime errors.

## Stage 09 review

The evidence-based review is recorded in `docs/model_integration_review.md`.
It found no critical simulation-identity or determinism violation, but it
originally recorded three high-priority gates. Stages 10–13 resolved the
profile visibility regression, imported the approved prototype GLBs, and added
local native-build evidence; the remaining review findings are listed in the
current handoff below.

## Stage 10 scale-profile regression fix

`VisualPresentationPolicy` now treats `RTS_PROFILE_FRAMES` as an explicit
batched-MultiMesh mode. When a scale profile and `RTS_PROTOTYPE_VISUALS=1` are
both requested, `main.gd` emits a warning and retains the MultiMesh instead of
hiding it for wrapper nodes that the scale-profile spawn path does not create.
`test_visual_presentation_policy.gd` covers all requested/profile combinations
with four assertions. This preserves the existing profile's purpose: measuring
the supported batched rendering path rather than the intentionally capped,
non-10k prototype wrapper path.

## Stage 11 native visual-ID validation

The source repository intentionally ignores `vendor/`, which means a clean
worktree cannot build until its known local dependency cache is made available.
For this worktree only, `vendor` is a symlink to the existing main-checkout
cache; it is ignored and not part of the branch diff. The Release build and all
three CTest entries pass. `test_native_visual_ids.gd` exercises the built
`RtsExtension.get_unit_visual_id()` binding for all 11 production unit types,
then resolves each returned ID through `VisualDefinitionRegistry`; it also
asserts invalid type values return an empty string. This removes the native
build gate from the review while keeping fresh-clone dependency bootstrapping
out of scope. With the local descriptor and dependency cache present, the
headless editor scan, normal smoke test, and native visual-ID script all
complete successfully.

## Stage 12 approved prototype GLB import

The 13 unique CC0 Godot-ready donor GLBs referenced by the current 15 visual
definitions are now versioned beneath `godot/project/assets/source_3d/cc0/`,
alongside their generator metadata. The source pack's CC0 policy is preserved
by the existing provenance inventory and mapping documents. Godot imported all
13 source scenes headlessly; the validator now reports 15 definitions with zero
missing GLBs. These are explicitly prototype donor assets, not final fictional
shipping art. Import success does not prove triangle budget, materials,
authored LOD hierarchy, or hardpoint readiness; those remain the next gate.

## Stage 13 metadata and shipping-validation gate

`VisualAssetValidator` now reads each imported `.meta.json` sidecar and reports
bounds plus triangle counts per visual definition. It warns on prototype donors
that exceed the 20,000-triangle budget or have bounds above 400 m, preserving
the evidence for cleanup instead of silently accepting unsuitable geometry.
Future `status: "shipping"` content fails closed if any validation issue is
present, including a weapon-bearing definition without an authored muzzle
offset. The validator test passes 11 assertions, including a shipping fixture
that proves this hardpoint rule. Existing imported models still use the
strategic-marker LOD mode; an authored imported-node LOD hierarchy remains a
future art-pipeline task.

## Current handoff — remaining model-integration work

This branch has a validated **prototype** visual path, not shipping-ready unit
art or a proven 10,000-unit visual renderer. Continue in this order:

1. **Authoring cleanup:** replace CC0 donor meshes with original fictional art;
   normalize the donor outliers flagged by metadata (bounds above 400 m or more
   than 20,000 triangles); create authored `_LOD0`/`_LOD1`/`_LOD2` hierarchies;
   supply authored weapon/FX anchors.
2. **Scalable renderer:** implement central batched visual tiers before raising
   `RTS_PROTOTYPE_VISUAL_LIMIT` above 200. The existing per-node wrapper is
   explicitly not a 10k path.
3. **Asset checks:** extend validation to imported materials/textures and
   enforce exactly one active imported LOD for shipping visual definitions.
4. **Compatibility:** version/hash the visual-definition pack for mod and
   multiplayer compatibility, while keeping gameplay IDs authoritative.
5. **Fresh-clone setup:** replace the worktree-local `vendor/` and
   `rts.gdextension` links with a documented, reproducible dependency bootstrap.
6. **Integration closure:** run the full Release build, CTest, Godot editor and
   smoke checks after each substantial renderer or asset-pipeline change; then
   request a follow-up review before calling the pack shipping-ready.

Current evidence: 13 imported CC0 GLBs cover 15 definitions; validator has 11
assertions with zero missing GLBs; visual registry, wrapper, spawn bridge,
presentation policy, and native visual-ID checks pass. The worktree remains
uncommitted by design.

## Stage 17 visual-pack compatibility identity

`VisualPackCompatibility` computes a deterministic SHA-256 over the visual
definition JSON and exposes a handshake containing `pack_id`, `pack_version`,
and `sha256`. Compatibility requires all three fields to match and rejects an
empty hash, while gameplay unit/entity IDs remain authoritative. The manifest
is recorded at `data/provenance/visual_pack_manifest.json`; its current hash is
verified by `test_visual_pack_compatibility.gd`, which passes seven assertions.
The helper is presentation-side evidence; wiring this handshake into the
existing multiplayer negotiation is still required before network compatibility
can be claimed.

## Stage 19 join-side compatibility enforcement

`NetworkManager::set_expected_handshake()` now lets a server/client configure
the expected visual-pack identity before receiving a connection handshake.
`receive_handshake()` rejects the peer when pack ID, version, or hash differs,
or when either identity is empty, while leaving the simulation protocol/content
fields and gameplay IDs unchanged. The native handshake suite now passes 18
assertions, including matching, version-mismatch, hash-mismatch, and empty-hash
cases. The caller still needs to populate its outgoing fields from the
presentation manifest; this branch does not silently derive a filesystem path
inside the simulation layer.

## Stage 20 reproducible dependency bootstrap

`scripts/bootstrap_model_integration_worktree.sh` now provides a deterministic
bootstrap for the ignored native `vendor/` tree and the ignored
`godot/project/rts.gdextension` descriptor. It requires explicit
`NEAR_FUTURE_RTS_VENDOR_SOURCE` and `NEAR_FUTURE_RTS_DESCRIPTOR_SOURCE` paths,
refuses to replace an existing path that points elsewhere, and creates only
missing symlinks. This keeps clean-clone setup reproducible without guessing a
checkout location or overwriting user work.

## Stage 21 presentation-backed handshake bridge

The native simulation now exposes manifest-field adapters for both sides of
negotiation: `network_send_visual_pack_handshake()` populates the outgoing
wire fields, while `network_set_expected_visual_pack()` configures join-side
validation. `RtsExtension` exposes these as validated
`send_visual_pack_handshake(pack_id, pack_version, sha256)` and
`set_expected_visual_pack(...)` methods, rejecting empty IDs, negative
versions, and hashes that are not 64 characters. The Godot join flow can now
pass the dictionary returned by `VisualPackCompatibility.handshake()` without
making the simulation layer read presentation files.

## Stage 22 Godot startup wiring

`main.gd` now loads `VisualPackCompatibility.handshake()` immediately after
instantiating `RtsExtension`, validates the manifest identity, and applies it
through `set_expected_visual_pack()` when that native method is available. The
join transport can use the same extracted fields with
`send_visual_pack_handshake()` when a connection is established. The feature
worktree does not contain a local Godot binary; native Release/CTest validation
passes, while the editor smoke check remains dependent on the shared binary
from the main checkout.

## Stage 18 network handshake transport fields

The native `ConnectionHandshake` now carries fixed-width visual-pack ID,
version, and SHA-256 fields alongside the existing protocol/map/content
identity. Serialization/deserialization and the wire-size assertion were
updated together, and `connection_handshake_visual_pack_round_trip` verifies
the fields survive the 220-byte wire format. This is transport support only:
callers still need to populate the fields from `VisualPackCompatibility.handshake`
and reject incompatible peers before joining a match.

## Stage 14 prototype scale normalization

The Industrial MBT and Elite VTOL definitions now apply data-driven scales
(`0.018` and `0.03`) derived from their imported sidecar bounds. The validator
records both raw and effective bounds and asserts that all current definitions
have valid effective dimensions without extreme runtime extents. Raw source
bounds and triangle-budget overruns remain visible as prototype cleanup
warnings; the donor meshes still require original art and authored LOD work.

## Stage 15 imported material validation

`VisualAssetValidator` now instantiates each imported PackedScene and counts
mesh, material, and albedo-texture bindings in addition to reading sidecar
bounds/triangles. Missing imported meshes or material references are reported
as validation issues, and the Markdown report exposes the counts per visual.
The current 13-GLB donor set passes these checks; texture counts remain
informational because some CC0 donors use embedded flat materials. The test now
passes 13 assertions. Imported-node LOD hierarchy validation remains separate
because the current donors intentionally use strategic-marker LOD mode.

## Stage 16 imported LOD visibility gate

For definitions that opt into `lod.mode = "imported_nodes"`, the validator now
loads the PackedScene state, confirms that at least two LOD-named nodes exist,
and inspects imported `VisualInstance3D.visible` values. It reports missing,
zero-visible, or multiple-visible LODs; shipping definitions fail closed on
those issues. A shipping fixture exercises the gate against a donor scene and
the validator now passes 16 assertions. The production donor definitions remain
on strategic-marker mode until authored LOD hierarchies are supplied.
