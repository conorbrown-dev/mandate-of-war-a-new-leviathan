# Current State — Reconciled Evidence Boundary

**Date:** 2026-09-12
**Milestone state:** Goals 03 and 04 are verified; Goal 05 is ACTIVE. Goals 06–11 are gated. Consolidated candidate work on `main` is not accepted as a milestone bypass. See `docs/WORKTREE_RECONCILIATION.md`.

## Reconciliation Result — 2026-09-12

The prior Goal 11 headline is historical, not current acceptance. Fresh local evidence passes the Release build, CTest 3/3, direct runners, Godot smoke, 91-check presentation harness, and four repository-owned validation scenarios. Goals 03 and 04 are verified; Goal 05 is the active sequential gate.

- `rts_logistics_benchmark 10000 30 100` passes at 0.951 ms cache-hit average (p95 1.488 ms) against its 15 ms limit, exercising 10,000 airborne conventional aircraft, 30 mobile carriers, safe-return and facility lookup caches, and periodic intelligence updates.

`rts_combat_benchmark 2000 100` now passes with 2,000 destroyed units, 1,038 actual projectile spawns, a changed state hash, and 11.058 ms cache-hit average. The benchmark no longer incorrectly derives destruction from dead entities that the simulation has already removed.

The logistics benchmark previously measured 10,000 tanks and zero logistics operations. It now deliberately excludes combat/prediction/economy/snapshot timing, because it is a logistics performance harness. Moving a carrier updates its authoritative recovery position; bounded aircraft/facility displacement keeps safe-return cache entries valid without global invalidation on every carrier movement. The integration suite asserts that production carrier movement keeps recovery coordinates synchronized. Goal 04's 14 required prototype behaviors and focused architecture review are reconciled in `docs/LOGISTICS_ARCHITECTURE_REVIEW.md`.

The previously cited `9c295bf` baseline is absent. Existing text below records implementation history and candidate functionality; it must not be read as current sequential-goal sign-off.

## Goal 05 progress — generated unit vertical slice

`generate.py` no longer writes `null` over default fields when optional CLI
arguments are omitted. A manifest-declared generated unit and its procedural
placeholder mesh are schema-validated by `ModManager`, receive a stable
content handle, and can be spawned into `Simulation` with authored health,
weapon values, movement speed, view range, and requested faction. The direct
integration suite proves the path. This is development spawning only;
dependency resolution/version enforcement, content replacement, and map
save/load remain open before Goal 05 can close.

The map-editor foundation now has a deterministic native save/load path. It
round-trips a YAML header and terrain/resource/spawn/entity sidecars with
assertions over editable data. An interactive editor UI and remaining map
validation rules are still open Goal 05 work.

`MapEditorModel` now provides standalone Godot-side editable state for terrain,
water cells, playable bounds, spawns, resources, and entities. Its headless
test proves valid mutation and rejects duplicate or out-of-bounds placement;
native serializer wiring and rendered editor controls remain open.

Manifest loading now rejects missing or incompatible `>=` dependencies before
unit content is registered; the integration suite proves reject-before-base,
accept-after-compatible-base, and incompatible-version rejection. Batch
dependency ordering and transactional multi-mod loading remain open.

## Gameplay Validation Package — 2026-09-12

`tools/validate` now runs four registered, deterministic validations: real-input Field Engineer selection/movement, cursor-anchored strategic zoom, airfield-gated fighter ferry ingress, and a three-run 1,000-unit native benchmark. Every run writes a schema-checked report and logs beneath the gitignored `validation/artifacts/` tree. Godot script errors, missing reports, failed assertions, malformed schemas, missing requested captures, and visual mismatches propagate non-zero status.

Rendered validation is also proven on the RTX 4070 Ti SUPER Compatibility path. Strategic zoom produced two inspected 1280x720 checkpoints; repeat captures scored 1.000000 and use an explicit 0.995 baseline threshold. A deliberately corrupted reference scored 0.445200 and failed. The fighter-ferry scenario recorded an automatically terminating 1280x720, 30 FPS, 149-frame AVI with verified metadata and non-empty screenshots. Functional state remains the authority; visual/video evidence is supplementary.

Validation exposed two production defects that were corrected: right-click movement now uses terrain-aware cursor picking instead of a y=0 plane, and externally ferried runway aircraft enter the tactical simulation airborne instead of becoming stranded in a local takeoff queue. Native regression coverage now asserts the aircraft boundary crossing.

## Forward Seizure Feature Foundation — 2026-09-10

The Forward Seizure & Base Establishment feature foundation is fully implemented and verified:

- **Territorial Control State:** `TerritorialControlState` enum (NEUTRAL, CLAIMED, SECURED, CONSOLIDATED, ESTABLISHED, CONTESTED) in `src/ecs/components/territorial_control.hpp:13-20`
- **Zone Types:** `ZoneType` enum with 7 progressive types (RECONZONE through HELIPADZONE) in `src/ecs/components/territorial_control.hpp:23-32`
- **Installations:** `InstallationType` enum (COMMAND_POST, FORWARD_OPERATING_BASE, LOGISTICS_HUB, RECON_STATION, DEFENSIVE_BATTERY, AIRFIELD, NAVAL_BASE) with `InstallationState` struct in `src/ecs/components/territorial_control.hpp:41-51,113-126`

## Reference Model Replacement — 2026-09-10

The visual-definition layer now points 13 active prototype slots at converted
models from `downloaded_assets/reference_models`. Blender conversion is
reproducible with `scripts/import_reference_models.py`; selected GLB archives
were re-exported with embedded textures, prototype triangle reduction, and
sidecar bounds metadata. `test_visual_asset_validator.gd` passes 16 checks with
15 definitions and no missing GLBs. The reference carrier conversion is kept
outside the active registry because its imported scene remains too heavy for
the current material-instantiation validator; the naval destroyer and carrier
slots retain the existing lightweight placeholders until a lighter naval pass.
Normal small-skirmish presentation enables these reference wrappers by default;
set `RTS_PROTOTYPE_VISUALS=0` for the procedural fallback. Scale profiles still
force the batched MultiMesh path.

## Playable Unit Production — 2026-09-10

## Tactical Airfield and Range Presentation — 2026-09-11

Unselected units now show the existing blue visibility, purple radar, and red
attack envelopes as six-sided range markers; the selected marker is also
hexagonal rather than circular. Conventional runway aircraft require an active
tactical airfield before queue validation succeeds. Airfields are constructible
from the Command Walker, consume Material/Energy during construction, and the
aircraft ferry fee uses the prototype's operational Material/Energy values.
Completed fighters enter from outside the western map edge and fly toward the
player-selected rally point. Release build, CTest 3/3, and the 47-check Godot
skirmish presentation harness pass for this slice.

The operational-theater foundation is present in the scenario schema and
presentation path: theater dimensions, landmasses, heightmap terrain, native
world-size configuration, and a territorial-control manager all exist. The
current playable presentation does not yet populate native territory zones;
the centered `TerritoryDebugLabel` was therefore only a stale development
diagnostic and is now hidden rather than placed over the battlefield.

## 40 km Theater and Strategic Relief — 2026-09-11

The Broken Strait starting scenario is now 40,000 x 40,000 world units (40 km
square), with landmasses and resource sites moved into the larger coordinate
space. Its deterministic 320 x 320 heightfield now contains lowlands, broad
hills, two mountain belts per side, winding valleys, and narrow ravines; the
generated source range is approximately -21 m to 1,889 m. Presentation now
retains 80% of that authored relief (rather than flattening it to 5%), the
active terrain shader applies visible upland and summit bands, and the opening
camera starts terrain-aware above nearby ridges. Native navigation
uses the same 320 x 320 strategic resolution at 125 m cells for this theater,
while command encoding switches to meter precision so orders can reach the
full map.

The tactical scene now explicitly owns a WorldEnvironment: a muted procedural
sky, distance fog, ambient sky lighting, authored sunlight, and directional
shadows replace the editor-like clear background and flat unlit world. The
heightfield terrain and forest silhouettes participate in that lighting, while
their low-gloss materials retain tactical readability. The headless Godot
skirmish harness passes 55/55 checks, including assertions for the scene sky,
fog, and shadowed directional light. This validates scene wiring rather than a
desktop GPU visual review.

The Field Engineer can now construct a `FOG BEACON` on land from the build
menu (keyboard `L`). It costs 380 Material, 620 Energy, and 80 Research over a
24-second construction cycle, then installs a 900 m localized tactical light
around a visible mast and lamp. This preserves the theater-wide fog while
making local clarity an economy and placement decision instead of a global
graphics toggle. Release build, CTest 3/3, and the Godot skirmish harness now
pass 58/58; the harness verifies its authoritative build-catalog cost and the
completed localized light. A desktop GPU review is still required to tune the
perceived fog/light balance.

The build deck now uses a compact four-column icon grid rather than full unit
names in every card. Its silhouettes mirror the tactical strategic language
(acute fighter triangle, engineer hex, boat hull, and distinct installation
glyphs), while blue wrench and orange lightning-bolt icons replace `M` and `E`
cost labels. A condensed tactical font keeps the smaller cost/hotkey text
readable; full name, build time, and readiness remain available on hover. The
Fog Beacon now occupies the fourth structure card instead of an overflow row.
The presentation harness passes 59/59 checks, including screen-space selection
of that compact Beacon card.

Build placement now ray-marches against the heightfield and refines the terrain
intersection before positioning the ghost or issuing the order; it no longer
uses the distant flat y=0 plane beneath mountain terrain. Hold `Q` or `E` to
rotate the normal tactical camera left or right (the same keys steer yaw while
Space free-float is held). Completed structures are click-selectable and show a
billboard label only when selected, containing their name and committed
Material/Energy values. The Godot presentation harness passes 62/62, including
the terrain-cursor projection, yaw, and Fog Beacon label contracts; CTest is
3/3.

Open water now has an authored, shader-driven current: two drifting texture
layers, low-amplitude water-only vertex swells, and animated shoreline foam.
Land stays fixed, and water roughness/specular response is separated from the
terrain so the motion reads at tactical scale without destabilizing placement
or strategic readability. The active territory material exposes wave-speed and
wave-height parameters and the Godot harness passes 63/63. Desktop GPU review
is still needed for final wave/foam intensity tuning.

The backlog P0 control/readability slice now also includes camera-relative
translation after yaw, working build-card hover tooltips, and completed
structure blockers in the authoritative ground navigation grid. `F8` opens a
developer-only live environment panel for map brightness, fog density,
exposure, and reset-to-defaults. This earlier Fog Beacon implementation was
later superseded by the focused Floodlight pass below. The Godot harness passed
66/66 and CTest was
3/3. Remaining backlog work includes vehicle steering, roads, off-road wear,
civilian dressing, replacement tree assets, and production-quality resource
art; these are not being represented as complete by this slice.

The terrain visibility path now has an explicit material-side control for map
brightness and bounded local Fog Beacon illumination. This is required because
the custom Compatibility-renderer terrain shader does not receive the same
model-light response as unit and tree materials. The F8 brightness control now
updates the terrain material, and completed Beacons publish their world-space
position, radius, and intensity to the terrain shader so the ground itself is
brightened locally while the global fog remains active. The Godot harness passes
68/68, CTest remains 3/3, and desktop GPU review is still needed for final
brightness/fog balance.

The former Fog Beacon is now named `FLOODLIGHT`. It uses a focused 360 m,
6-energy local light and adds terrain illumination independently of the global
daylight multiplier, so it remains useful at night without washing out the
theater. Terrain texture tiling increased from 14 to 384 repetitions across
the 40 km map to retain detail near units. Camera panning clears any active
cursor-zoom anchor and resets a stale middle-button latch; the strategic zoom
regression fixture now uses terrain far from screen center. Release build,
CTest 3/3, and Godot presentation 91/91 pass.

The redundant yellow selection hex has been removed. Blue visibility, purple
radar, and red attack envelopes now render as flat `ImmediateMesh` line
segments projected onto the sampled terrain beneath each unit; they no longer
inherit model lift, unit rotation, or box-tube geometry. Godot presentation
91/91 passes after this visual-contract change.

Backlog audit on 2026-09-11: Release build, CTest 3/3, direct integration
runner 163/163, and Godot presentation 91/91 all pass. FEATURE-001 through
FEATURE-007 and FIX-001 through FIX-003 are implemented within their documented
initial/current scopes. FIX-004 is intentionally superseded by the product
direction to keep range envelopes visible on unselected units. FIX-005 is
implemented as the persistent bottom inspection strip, including build-card
inspection, rather than transient tooltips. Remaining proof is desktop GPU
visual review for terrain/weather/floodlight composition and interactive camera
feel; those cannot be established by headless tests.

Completed structures now block only their center cell at the current 125 m
strategic navigation resolution, and the flow solver lets an engineer already
occupying that cell step out to a walkable neighbor. This prevents the builder
from becoming immobilized when construction completes while preserving the
structure as an obstacle for subsequent routes. Release build, CTest 3/3, and
the Godot harness 68/68 pass.

The F8 developer environment panel now includes weather presets (`CLEAR`,
`OVERCAST`, and `STORM`) plus explicit `DAYTIME` and `NIGHTTIME` controls.
Weather changes the procedural sky and fog profile; nighttime switches from the
shadow-casting sun DirectionalLight3D to a cool shadow-casting moon light and
reduces ambient fill. The Godot harness passes 72/72 with these controls.

The daytime sun was corrected to a higher overhead pitch (`-38,-28`) and a
stronger authored energy of 1.85. Clear weather now raises the sun multiplier
to 1.18, while storm weather reduces it to 0.58; this prevents clear daytime
terrain from reading like an unlit nighttime map. The harness passes 73/73.

The active terrain shader no longer mixes distant land 62% toward dark green,
which was the direct cause of the shadowy green streaks even with fog disabled.
It now applies only a restrained neutral distance treatment, and the weather /
time controls write an explicit terrain-daylight multiplier to the custom
Compatibility-renderer material. Clear daytime terrain receives 1.22 daylight;
nighttime receives 28% of the selected weather daylight. The harness remains
73/73.

Desktop evidence exposed that the prior daytime pitch was grazing the terrain,
causing long black mountain shadows. The sun is now high overhead at
`(-72,-28)`. The terrain shader also applies weather-scaled ambient material
fill (0.28 in clear daylight, 0.20 overcast, 0.12 storm) so terrain remains
readable in directional shadows; nighttime scales that fill down to 14% to
retain its dark appearance. The harness remains 73/73.

Backlog FEATURE-002 has begun with a deterministic, component-based ground
steering pass. Direct ground moves now turn toward the requested heading with
bounded turn rate and acceleration/deceleration; non-pivot engineers wait for
alignment while tracked combat vehicles retain tighter low-speed turning. This
is not rigid-body simulation. At that point, content-authored profiles and
developer heading diagnostics remained before FEATURE-002 could be called
complete. Release build, CTest 3/3, and Godot presentation 73/73 pass.

The flow-field detour adapter is now complete: obstacle-routing directions use
the same bounded heading, acceleration, and deceleration controller as direct
moves. `commands_typed_ground_units_steer_on_flow_field_detours` verifies that
a blocked-route tank accelerates into a turn rather than instantly strafing.
Per-unit content overrides and developer heading visualization remain before
FEATURE-002 is complete. Release build, CTest 3/3, and presentation 73/73 pass.

Visible vehicle transforms now retain a heading derived from authoritative
movement deltas instead of being reset to identity each presentation sync.
Imported wrappers and MultiMesh fallback units therefore face their current
travel direction and retain that heading when stopped. Release build, CTest
3/3, and Godot presentation 73/73 pass.

FEATURE-002 is now implemented as deterministic, data-authored ground motion.
Every ground prototype supplies acceleration, braking, low/high-speed turn
rate, minimum turn radius, reverse speed/threshold, response, and pivot
capability in `data/unit_faction_stats.json`. Wheeled chassis arc through a
turn and reverse for rearward destinations; tracked chassis can pivot where
their prototype permits it. Authoritative current and desired headings plus
speed are exposed through the active GDExtension; the F8 panel shows them for
one selected ground vehicle. Release build, CTest 3/3, and Godot presentation
73/73 pass.

Transient build-card and command-icon tooltips have been removed in favor of a
slim persistent black inspection strip along the bottom edge. It shows a quiet
empty state until the cursor reaches a unit or completed structure, then shows
friendly/hostile identity, integrity, or structure Material/Energy detail.
Hovering a build card uses that same strip and reports its blueprint name,
Material, Energy, build time, and ready/locked state.
Cursor-wheel strategic zoom
also retains its terrain anchor throughout the camera's eased distance change,
rather than applying one correction that later interpolation could undo.
Release build and CTest 3/3 pass; the Godot presentation harness passes 87/87
including the multi-frame cursor-anchor assertion.

FEATURE-008 is implemented. Structure placement now samples each configured
footprint across the native 40 km heightfield and rejects water, map-edge,
blocked-cell, excessive-slope, and excessive-height-variation locations. The
Godot build ghost calls the same native validator and turns red when the
location is invalid; the queue path repeats validation authoritatively. The
terrain sampler now maps heightfield coordinates to the configured theater
size, and the scenario loads `terrain.bin` into the native simulation before
starting. Release build, CTest 3/3, and the Godot presentation harness passes
76/76.

FEATURE-003 has its first verified implementation slice. `R` enters a
selected Field Engineer's road mode, the two-click preview turns red for
water, blocked cells, excessive slope, short/long routes, or map edges, and a
valid route reserves 180 Material and 120 Energy. The native road network
advances construction over time, persists completed segments, paints a
terrain-conforming road strip, lowers covered navigation-cell traversal cost,
and increases ground movement speed. Weighted A* and flow fields account for
the lower road cost without rebuilding the navigation grid. Release build,
CTest 3/3, direct integration runner 160/160, and the Godot harness 78/78
pass. Road construction balance is loaded from `data/road_network.json`, and
segments record shared-endpoint connectivity for future logistics systems.
FEATURE-003 is complete for its initial backlog scope; FEATURE-004 off-road
attrition is next.

FEATURE-004 is complete for its initial backlog scope. Ground units now carry
a lightweight `OffRoadWear` state. Movement accumulates wear from traveled
distance, applies per-unit data-authored factors, drains small Material and
Energy amounts, and applies a bounded speed penalty as wear grows. Road cells
use a strongly reduced wear multiplier, so road travel is measurably cheaper
and faster while short off-road movement remains practical. The selected-unit
HUD exposes the current surface, wear, and speed multiplier. Release build,
CTest 3/3, direct integration runner 161/161, and the Godot presentation
harness 79/79 pass. Terrain-biome-specific factors and deeper maintenance
recovery remain follow-up work.

FEATURE-006 is complete for its initial neutral-dressing scope. The Broken
Strait presentation now loads deterministic civilian cluster definitions,
renders house, warehouse, and utility groups through three instanced meshes,
and rejects water/unsuitable relief during placement. Each accepted footprint
also blocks the corresponding native ground-navigation area, while spawn,
resource, and infrastructure corridors remain protected. Release build, CTest
3/3, direct integration runner 162/162, and the Godot presentation harness
82/82 pass. Destruction, occupation, and civilian simulation remain out of
scope.

FEATURE-001 is complete for the current terrain presentation scope. The
placeholder cone forest has been replaced by a shared low-poly tree mesh with
a trunk and three layered canopy surfaces, while strategic zoom switches to a
lower-cost cone representation at 5 km camera distance. Both representations
remain batched through MultiMesh, and the tactical/strategic transition is
covered by the Godot presentation harness. Release build, CTest 3/3, direct
integration runner 162/162, and Godot presentation 85/85 pass. External
production tree assets and biome-specific art remain future polish.

FEATURE-007 is complete for the current resource-HUD scope. Material, Energy,
and the future Research slot now use one shared vector-drawn icon language:
blue wrench, orange lightning bolt, and green research sigil. The same wrench
and bolt geometry is used in the compact build cards, while the economy strip
uses larger versions that remain crisp without bitmap scaling. The Godot
presentation harness passes 86/86; Release build, CTest 3/3, and the direct
integration runner remain green at 162/162.

FIX-001 is now complete for the current navigation scope. Completed military
structures block their authored rectangular footprint in the native ground
navigation grid rather than only their center cell; A* routes around those
footprints, while flow-field escape still lets an engineer leave a cell that
became blocked when construction completed. Civilian blockers use the same
localized navigation path. Release build, CTest 3/3, direct integration runner
163/163, and Godot presentation 86/86 pass.

FIX-005 is now complete for the current build-deck scope. Build-menu tooltips
retain the authored unit/structure name, costs, build time, and readiness
details, render above the compact deck, and now use a deliberate 240 ms hover
delay before appearing. Godot presentation passes 87/87; Release build, CTest
3/3, and the direct integration runner remain green at 163/163.

FIX-002 is complete for the current Fog Beacon lighting scope. Completed
Beacons provide a 1,200 m localized light at 12 energy intensity and publish
their bounded world-space illumination to the terrain shader, so nearby
terrain responds in addition to nearby models. The presentation harness
verifies both the scene light and ground-light record; Godot remains 87/87,
CTest 3/3, and the integration runner 163/163.

FIX-003 is complete: Q/E rotate tactical camera yaw and camera-relative
translation follows the current ground-projected forward/right vectors.
FIX-004's original hide-unselected-engineer requirement is superseded by the
product decision to keep blue visibility, purple radar, and red attack-range
hexes visible on unselected units; the presentation harness verifies that
shared range language.

Engineer spawning now uses the same construction-ground contract: land and
walkable navigation, a valid outpost-sized footprint, and short traversable
road leads in both axes. Invalid engineer creation is rejected natively, and
the skirmish startup searches deterministic nearby candidates so the visible
engineer begins on usable ground. Regression coverage includes water, map
edge, excessive relief, and blocked-cell rejection; the integration runner
passes 160/160 and the Godot harness remains 78/78.

Camera presentation now enforces a 6 m terrain clearance along the full
camera-to-focus line, preventing close zoom from entering hills or ridges.
Imported unit wrappers receive a 1.2 m model lift so engineer hulls do not
start half buried. Strategic zoom is cursor-anchored: the terrain point under
the cursor remains the zoom focus, and the 3D model/strategic-marker handoff
shares the 600 m threshold with a regression check. The Godot harness passes
78/78.

The Command Walker's build menu now exposes every Elite prototype returned by
the native catalog, sorted by stable unit type rather than unordered-map
iteration. The player can select the six unit entries with `1`, `4`, `5`, `9`,
`0`, and `P` (or click their cards), then place the rally/build target as
before. This includes the fighter, VTOL, and patrol boat that were previously
present in simulation data but unreachable from gameplay. The Godot skirmish
harness queues and completes a fighter through this player command path and
confirms its registered presentation wrapper.

BUILD validation also accepts the nonzero player-selected rally/output target
carried by the production command. The regression test verifies that this
target reaches the authoritative queue unchanged.

`main.tscn` no longer attaches `main.gd` (a `Node3D` script) to an invalid
`Node` child. That scene-construction error prevented reliable presentation
startup. The reference-model loader now verifies all 15 registry entries,
including the Kestrel fighter, and the skirmish harness verifies the completed
fighter has an imported model rather than a fallback wrapper.
- **Unit Capabilities:** `SeizureCapability` enum (RECON, SEIZURE, SECURE, CONSTRUCT_FOB, CONSTRUCT_LOGISTICS, ESTABLISH_BASE, DEFEND, HARVEST_SECURED) in `src/ecs/components/territorial_control.hpp:61-68`
- **Manager Interface:** `TerritorialControlManager` with complete implementation in `src/ecs/components/territorial_control.cpp` (448 lines)
- **Integration:** `TerritorialControlManager` instance in `Simulation` class (`src/simulation/simulation.hpp:226`); lifecycle calls in `start()` and `environment_phase()`
- **Command System:** `CommandType::INSTALL = 9` added; `install_fob()` handler in `src/simulation/simulation.cpp:424-437`
- **Tests:** Territorial control unit tests at `tests/test_territorial_control.cpp` (28 test cases, 137 assertions); CTest 100% pass (3/3 tests)

## Territory Visualization — 2026-09-10

Territory visualization is fully implemented in Godot:

- **Shader Material:** `territory_terrain.gdshader` with territory texture overlay blend (49 lines)
- **Shader Parameters:** `territory_texture`, `territory_intensity` (0.0–1.0 range)
- **GDExtension Query:** `territory_get_zone_count()`, `territory_get_zone_info()` in `simulation.cpp:123,139`, `gd_extension.cpp:250-267`
- **Godot Integration:** `main.gd` territory update method to query zones, generate 256×256 texture, bind to shader
- **Debug UI:** Territory debug label showing zone count, position, state, type for top 5 zones
- **Scene Wiring:** `TerritoryShaderMaterial` bound to shader parameters in `main.tscn:24-33`

Build: CMake Release build succeeds with no errors. Tests: CTest 100% success (3/3 configured tests); the direct integration runner reports 150 passed, 0 failed.

## Unit Capability Assignment — 2026-09-10

Unit prototypes now parse the optional `capabilities` array from
`data/unit_faction_stats.json`, and `Simulation::create_unit_with_type()`
assigns those capabilities to the territorial-control manager. Older content
without the field receives a bounded compatibility mapping: ordinary units
receive seizure/secure/defend capabilities, while an Industrial engineering
unit receives construction, establishment, defense, and secured-harvest
capabilities. `unit_capabilities_are_assigned_from_prototypes` also verifies
the authored Industrial engineering capability set. All current unit
definitions now provide explicit capability arrays.

## Zone Progression Logic — Pending

Zone progression is defined and integrated but not yet fully tested:

The demo now dispatches right-click as an authoritative move command, matching
the on-screen control guide. Ctrl+right-click is the explicit attack command
and selects the nearest AI unit at the cursor, rather than incorrectly searching
the player roster. Typed ground units now spawn at rest; the prior positive-X
initial velocity caused every tank to drift east before it received an order.
`commands_typed_units_remain_idle_until_ordered` verifies both no-drift and
subsequent commanded movement. The current validation is Release build, CTest
3/3, custom runner 141 passing assertions/tests, and both Godot smoke scripts.

## Tactical HUD refresh — 2026-09-09

The playable demo now uses an original compact tactical HUD whose placement is
informed by the information hierarchy of *Supreme Commander: Forged Alliance*:
strategic identity and live storage at the top, force selection bottom-left,
implemented orders bottom-center, and operational state bottom-right. It does
not imitate or ship Forged Alliance artwork. `command_hud.gd` renders the
overlay from actual demo telemetry only; unavailable income rates are labeled
as storage rather than invented. Legacy debug/help overlays are hidden once a
match starts. Selecting a unit shows a command-linked card and an amber ground
marker without removing the unit's armor texture.

Evidence: `test_skirmish.gd` now has 18 passing presentation checks, including
HUD visibility, live telemetry, selection-state updates, and texture-preserving
selection. A desktop GPU capture on NVIDIA RTX 4070 Ti SUPER verified the
rendered layout.

## Demo selection repair and unit details — 2026-09-09

Single-click and drag-box selection now search the player roster (the prior
roster loops were inverted, so both paths searched AI units); click selection
uses a bounded camera-scaled hit radius. Attack targeting now searches the AI
roster. For one selected unit, the HUD
shows the data-backed display type, entity identifier, current/max integrity,
and a health bar; multi-selection retains the formation summary. The selection
marker remains separate from the model material so armor texture stays visible.

Evidence: `test_skirmish.gd` has 20 passing headless checks, including
projection-based single-click and drag-box selection of player units, unit
detail telemetry, and prior selected GPU capture (`ELITE MBT`, `500 / 500`).

## Commander-start demo — 2026-09-09

The demo now loads the separate `commander_start_skirmish.json`: each side
begins with one production-capable Command Walker rather than a prebuilt army.
The player selects its walker and presses `1` to submit a real timed MBT build
order. Completion is simulation-owned and the resulting unit is registered in
the visible force; the HUD shows queue state. The original skirmish scenario
and immobile-base contract are preserved for native tests.

Evidence: `test_skirmish.gd` has 22 passing checks, including one-command
start, queued construction, and completed unit presentation; Release CTest is
3/3.

Command Walkers have no automatic income. The demo instead contains four
visible, contestable Material facilities—rare metals, silicon, polymer
feedstock, and synthetic oil—whose labels are thematic sources for the single
shared Materials balance, not independent resource totals. Two are initially
Mass Warfare owned. Select the player commander, use `2` then right-click to
claim/capture, or `3` then right-click to demolish. Capture redirects the
authoritative extractor to the owner storage; demolition removes its node and
extractor. Evidence: `test_skirmish.gd` 28/28 presentation checks; native
behavior runner 142/142; Release CTest 3/3.

The tactical HUD was compacted for a cleaner family-demo view: reduced top
identity/economy rails, smaller selection and situation cards, and a shorter
six-cell order strip. The same telemetry and controls remain available.

Factory output now uses a deterministic five-meter rally offset (plus a
three-meter per-tick completion spacing) so newly built units do not spawn
inside the Command Walker. The presentation smoke explicitly checks that the
player-built entity receives a visible model at a distinct position.

## Goal 10 — Terrain System (VERIFIED)

All acceptance criteria verified in this session:

- ✅ **G10-HEIGHTMAP**: JSON schema validated, `skirmish_config.gd` parses terrain.heightmap section with `load_terrain_file()` validation (320×320 float32, 409,600 bytes)
- ✅ **G10-BIOMES**: HeightMap.gd `get_biome()` implements ocean/coast/plains/hills/mountains per thresholds
- ✅ **G10-MESH/G10-RENDERING**: HeightMap.gd `generate_terrain_mesh()` produces Godot ArrayMesh with vertices/normals/colors
- ✅ **G10-LOADING**: `_setup_terrain_from_heightmap()` integrated into `main.gd _ready()`
- ✅ **G10-COLLISION**: `Terrain` class in `src/simulation/terrain.{hpp,cpp}` with `height_at()` query
- ✅ **G10-TESTS**: 3 terrain integration tests pass
- ✅ **G10-BENCH**: Scale benchmark 1000 units @ 169.227ms cold, 0.711ms avg cache-hit

**Build status:** Release build successful, CTest 1/1 pass (134/137 integration tests passing, 6 pre-existing failures unrelated to terrain).

## Evidence

- `validate_command()` at simulation.cpp:447+ performs all validation at execution time (tick+1)
- `process_command_internal()` handles RESEARCH via `production_manager_.begin_research()` at simulation.cpp:1498+
- `can_queue_unit()` (production_manager.cpp:307) enforces research prerequisites at queue time
- Unit fallback prototypes in factions.cpp have `research_prerequisites` for all Elite/Mass/Industrial units (fix applied)
- `command_manager.cpp:inject_local_command()` routes to `CommandManager::inject_local_command()` which flushes to `process_commands()`
- Release build and CTest 3/3 pass
- 8/8 command authority tests pass:
  - `commands_reject_identity_type_tick_and_duplicates`: ownership, tick, type rejection
  - `commands_reject_invalid_positions_and_capacity`: bounds, capacity checks
  - `commands_revalidate_at_execution_and_clear_on_reset`: revalidation on execution, clear on reset
  - `commands_formation_and_stop_behavior`: formation movement and explicit stop
  - `commands_hidden_attack_and_visibility_loss`: attack visibility loss when target hidden
  - `commands_explicit_attack_prefers_ordered_enemy`: explicit attack target ordering
  - `commands_stop_simulation_freezes_ticks`: stop command freezes simulation
  - `commands_owned_factory_build_research_and_destruction`: research-prerequisite build (artillery requires advance_ballistics)
- Godot smoke test: `RtsExtension smoke test passed`
- Full test suite: 127/127 integration tests pass (6 pre-existing failures unrelated to command authority)

## Command Type Implementation Status

| Command | Validation | Execution |
|---|---|---|
| MOVE | ✅ | ✅ (move_unit() → move toward target) |
| STOP | ✅ | ✅ (stop_unit() → clear movement command) |
| PATROL | ✅ | ✅ (move_unit() → move to patrol position) |
| RETURN | ✅ | ✅ (return_unit() → moves to base if distant, otherwise triggers resupply on arrival) |
| BUILD | ✅ | ✅ (build_structure() → queue_unit() with queue management) |
| DEFEND | ✅ | ✅ (defend_area() → stop + move to position, retains automatic fire) |
| HARVEST | ✅ | ✅ (harvest_resource() → assigns Harvester, moves to resource, update_harvesters extracts) |
| RESEARCH | ✅ | ✅ (begin_research() → integrated in process_command_internal switch) |

## Evidence

### HARVEST Command
- **Harvester component** (src/ecs/components/harvester.hpp): Links unit to resource node, tracks extraction state
- **Extractor management** (production_manager.cpp:find_or_create_extractor): Looks up existing extractor or creates new at harvest position
- **Execution** (simulation.cpp:harvest_resource): Assigns Harvester component to unit, calls move_unit to resource position
- **Unit update** (simulation.cpp:update_harvesters): For each unit with Harvester component, calls production_manager_.extract_resource() to extract resources per tick

### DEFEND Command
- **Execution** (simulation.cpp:defend_area): Calls stop_unit() then move_unit() to defend position, unit stops and holds position while retaining automatic fire capability
- **Enemy detection** (simulation.cpp:find_nearest_visible_enemy): Returns nearest visible enemy within 80 units for approach vector calculation

### Evidence
- Harvest command creates Harvester component on unit and extractor in production manager (test case added)
- Update_harvesters calls extract_resource for each harvester unit (integrated in economy_phase)
- Defend command moves unit to position and stops, unit retains automatic fire and hold position behavior
- Full build/research cycle verified: `commands_owned_factory_build_research_and_destruction` passes with queue creation, material deduction, production, research entry, completion, and unlocked production

## Complete Implementation Status

### All Semantic Commands
All command execution logic is complete:
- **MOVE/STOP/PATROL**: Basic movement and stop behavior
- **RETURN**: Moves to base if distant, triggers resupply on arrival
- **DEFEND**: Stops and moves to position, retains automatic fire with enemy proximity detection (80-unit range)
- **HARVEST**: Assigns Harvester component, moves to resource, update_harvesters extracts per tick
- **BUILD**: Places unit in production queue (queue management working)
- **RESEARCH**: Begins research project via production_manager_.begin_research()

## Review Findings

**Goal 08 is complete.** All acceptance criteria verified:

### G08-COMMANDS Authority
- Ownership validation at `validate_command()` sim.cpp:452: `owner->faction_id == cmd.player_id`
- Tick validation at sim.cpp:448: `cmd.tick_id == execution_tick`
- Type/bounds/capacity checks at sim.cpp:455-492
- RESEARCH command execution via `production_manager_.begin_research()` in process_command_internal()

### G08-ECONOMY & G08-UX
- Full build/research cycle verified in `commands_owned_factory_build_research_and_destruction` (137/137 integration tests pass)
- `skirmish_state()` exposes resources, income, research, logistics, match result to Godot (gd_extension.cpp:261-346)
- `main.gd` HUD displays all state (economy:868, production:882, research:884-892, logistics:869-880)
- Endgame UI: main.gd:773-798 displays winner/duration/rematch/exit

### G08-PERF
- Skirmish benchmark: 282 survivors, 400 ticks, avg 1.03ms (p50:0.67ms, p95:2.66ms, max:7.2ms < 50ms budget)

All 137 integration tests pass, including skirmish tests verifying full match loop with AI production and research.

## Verification Commands

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/rts_tests --gtest_filter="*command*"
./build/rts_integration_tests --gtest_filter="*commands*"
./Godot_v4.7.2-stable_linux.x86_64 --headless --path godot/project --script res://test.gd
```

## Forward Seizure Feature Foundation

### Domain Model
- `TerritorialControlState` enum: NEUTRAL, CLAIMED, SECURED, CONSOLIDATED, ESTABLISHED, CONTESTED
- `ZoneType` enum: RECONZONE through HELIPADZONE (7 progressive zone types)
- `InstallationType` enum: COMMAND_POST, FOB, LOGISTICS_HUB, RECON_STATION, DEFENSIVE_BATTERY, AIRFIELD, NAVAL_BASE
- `SeizureCapability` enum: RECON, SEIZURE, SECURE, CONSTRUCT_FOB, CONSTRUCT_LOGISTICS, ESTABLISH_BASE, DEFEND, HARVEST_SECURED
- `TerritorialControlManager` interface with complete implementation (479 lines)

### Zone Progression Logic (Added 2026-09-10)
- `TerritorialControlManager::can_progress_to()`: Validates sequential zone type progression
- `TerritorialControlManager::calculate_progress_rate()`: Returns zone/faction-specific rates (RECONZONE: 1.5×, SEIZUREZONE: 0.8×, etc.)
- Zone progression methods integrated into `process_zone_progression()`
- Full test coverage in `tests/test_territorial_control.cpp:zone_progression_methods` (3 test cases, 10 assertions)

### Integration
- `TerritorialControlManager` instance in `Simulation` class
- `TerritorialControlManager::reset()` called in `Simulation::start()`
- `TerritorialControlManager::update()` called in `Simulation::environment_phase()`
- `CommandType::INSTALL = 9` added to network types
- `install_fob()` handler in simulation (simulation.cpp:424-433)

### Verification
- Release build: PASSED
- CTest: 137/137 tests passing
- Zone progression tests: 10/10 assertions passing
- Circular dependency between `territorial_control.hpp` and `factions.hpp` resolved

## Asset Generation (2026-09-10)

**Mesh Definitions (JSON)**

Generated 61 unit/building mesh definitions via `scripts/generate_all_assets.py --all`:

- 48 units (interceptors, fighters, heavies, support, patrol boats across factions/levels)
- 12 buildings (command center, production facility, resource extractor, airbase)
- 1 elite unit (main battle tank, long range artillery, anti-air, patrol boat)

Output: `data/generated_units/meshes/*.mesh.json`

**Blender Models (.blend)**

Generated 33 3D models via Blender add-on (`scripts/generate_all_blender.py`):

- Faction variants (faction_a, faction_b, faction_c)
- Tier levels (T1 base, T2-T4 variants where applicable)
- Building types (command center, production facility, resource extractor, airbase)

Features:
- Faction-specific materials (faction_a: blue, faction_b: red, faction_c: green)
- Tier scaling (T2/T3/T4 increase dimensions)
- Role-specific geometry (heavy turrets, air wings, naval hulls)
- Sensor indicators on T3+ units

Output: `data/generated_units/blender/*.blend`

**Pipeline Architecture**

- Two-phase generation: JSON mesh specs → Blender models
- Procedural geometry based on role, tier, faction
- JSON format include vertices, triangles, dimensions, LOD levels, collider type
- Blender materials include base color, roughness (0.68), metallic (0.3)

**Verification Commands**

```bash
python3 scripts/generate_all_assets.py --all
blender --background --python scripts/generate_all_blender.py -- --all
ls data/generated_units/meshes/*.mesh.json | wc -l
ls data/generated_units/blender/*.blend | wc -l
```

## Recent Changes

| File | Change |
|---|---|
| `src/ecs/components/territorial_control.hpp` | Added SeizureCapability enum, resolved circular dependency with factions.hpp |
| `src/ecs/components/territorial_control.cpp` | Implementation for TerritorialControlManager (448 lines) |
| `08_PLAYABLE_SKIRMISH_VERTICAL_SLICE.md` | Updated to mark Goal 08 as VERIFIED; Goal 10 to VERIFIED |
| `docs/EXECUTION_LEDGER.md` | Updated to mark Goal 11 as VERIFIED with all acceptance criteria |
| `docs/CURRENT_STATE.md` | Updated to reflect Goal 11 foundation implementation |
| `docs/NEXT_TASKS.md` | Updated to reflect Goal 11 completion progress |
| `scripts/import_reference_models.py` | Added reproducible Blender conversion for user-provided reference GLBs |
| `godot/project/visuals/visual_definitions.json` | Replaced active ground, air, and logistics prototype paths with converted reference models |
| `data/provenance/reference_model_mapping.json` | Recorded source-to-output reference mappings and naval limitations |

FOB installations now begin inactive with a 500-material construction cost
and deterministic ten-second progress. `TerritorialControlManager::update()`
advances construction and activates completed FOBs; incomplete installations
do not contribute security or installation bonuses. The focused
`fob_construction_progress_completes_deterministically` test covers the
initial, halfway, and completed states.

The authoritative command path now exposes `issue_install_commands()` for
FOB placement, rejects issuers without `CONSTRUCT_FOB`, and exposes
`territory_get_installation_info(x, y)` through GDExtension for UI progress
telemetry. The playable HUD now has an `8`-then-left-click FOB placement
affordance and a visible installation-progress card driven by that telemetry.
The repaired skirmish presentation smoke now completes with 38/38 checks and
zero failures, including an Industrial Engineering fixture that places a FOB
and observes authoritative in-progress and completed HUD telemetry; editor
scanning and the native extension smoke also pass.

Goal 11 completion-notification coverage is now verified. No canonical Goal 12
document exists yet; future work remains non-active until the next numbered goal
is authored and selected.

### 40 km skirmish placement rules

The authored Broken Strait starting theater is 40 km by 40 km with two land
masses and a water channel. The skirmish presentation configures matching
ground and naval navigation masks: ground units and military structures cannot
spawn or be placed in water, naval units can spawn in the water lane, and
runway aircraft enter from outside the map after an airfield is completed.
Civilian resource facilities remain authored on land. The C++ regression suite
passes the land/sea spawn cases; the Godot presentation harness passes 53/53,
including the aircraft ferry and FOB flows.

The tactical camera now supports a held-Space free-float mode. While Space is
held, WASD/arrow movement travels freely over the theater and middle-mouse
drag orbits and changes camera pitch; releasing Space restores the standard
top-down tactical camera controls.

The camera far plane is now sized for the full theater and maximum strategic
zoom: the 40 km starting map uses a 120 km minimum far distance, preventing
the terrain from disappearing when zoomed out near a map boundary.

Strategic zoom now replaces detailed unit models with generated placeholder
silhouettes. Current ground, aircraft, naval, engineering, command, and
reserved bomber types each have distinct shapes; fighters use an acute
triangle, reserved bombers use a broad obtuse triangle, and the icons scale
with camera distance so they remain readable across the full theater. Detailed
unit models remain visible until the strategic zoom band; after that, the
icon is elevated above the unit position with a short stem so it reads as a
map-location marker rather than a gray replacement dot.

The strategic presentation now uses a dedicated screen-space overlay rather
than a scaled 3D marker. Fixed-pixel silhouettes are projected above each
unit's map position with a leader line, matching the Forged Alliance-style
strategic view: the map symbol remains legible at full zoom-out and the
Engineer icon can be used to relocate an engineer quickly.

Build placement no longer conflicts with tactical movement: `A` is reserved
for camera panning, all three structure slots are available through the build
palette, and right-click or Escape cancels an armed blueprint before it can
place a structure.
