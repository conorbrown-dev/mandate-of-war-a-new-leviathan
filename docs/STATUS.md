# Current Status

## Core design rules

- **Research happens outside the skirmish.** It is not done in a warzone.
  `research_prerequisites` and `research_projects.json` are content for
  strategic authorization, which does not exist yet. A match has no research
  balance, queue, command or AI behavior, and command id 8 is retired.
- **Forces arrive; they are not built on the battlefield.** Almost no units are
  manufactured in the theater. They are flown in, brought in by road or railway,
  dropped by paratroopers, or landed by boat once sea control is established.

## Gameplay stabilization

The active limit is 2,000 live units per player, enforced natively by
`MAX_UNITS_PER_PLAYER`. A player is one `FactionId`, so the simulation supports
three players today. The commander counts; structures do not. Direct creation
is refused at the cap, a queued unit pauses until a slot opens, and a delivery
is refused without charge. Unit builds are paid when the command is accepted,
so a paused order keeps its materials. A paused unit at the head of a queue
also holds back anything queued behind it. The HUD does not show the cap yet.

`MAX_SNAPSHOT_ENTITIES` is 8,192, which holds all three factions at the cap.

Evidence from 2026-11-05 (Release, Ryzen 9 9900X under WSL, simulation only,
20 Hz, so 50 ms per tick): `rts_skirmish_benchmark 1950 400` runs 3,951 units at
4.4 ms average, 10.0 ms p95 and 44.1 ms maximum. Before the fire-pass target
index and AI vision rewrite the same run was 22.1, 85.8 and 124.6 ms. `rts_combat_benchmark` final
hashes matched before and after the index at 1,000 and 2,000 units per side.
This says nothing about Godot frame rate.

Known failures, none caused by this work:

- `native_extension_smoke`: expects a unit at x=8.0 after 10 ticks; it is at 7.4.
- `skirmish_presentation`: build catalog text, single-click selection and
  commander site demolition checks fail, and the script then never exits.
- `basic_selection_move`: the same single-click selection failure.
- `road_construction`: calls `get_faction_production_line`, which the extension
  no longer binds.
- `airfield_fighter_ferry`: all checks pass, but Godot reports resource leaks at
  shutdown.

## Content and balance

Units, factions and weapons are defined only in `data/unit_faction_stats.json`,
and terrain uses JSON and PNG. See `docs/content_format.md`.

Evidence from 2026-11-06: `rts_balance_benchmark` keeps all three ground army
pairings within its 0.12 tolerance (Elite vs Mass -0.08, Elite vs Industrial
+0.11, Mass vs Industrial +0.11). This covers ground armies in open terrain only,
without air, naval, terrain or player tactics. Simulation worst tick at the cap
is about 22 ms after the AI vision pass was rewritten.

## Terrain and vegetation

`tools/generate_terrain.py` deterministically generates the heightfield, with
ridged mountains, erosion and rivers routed along real drainage to the sea, plus
baked masks for rivers, forest, light and ground variation. Gameplay sites stay
level through `FLAT_ZONES`. `territory_terrain.gdshader` blends ground layers by
slope and elevation, and breaks up tiling with rotated multi-scale samples.

`terrain_vegetation.gd` adds about 710,000 presentation-only trees, shrubs and
rocks in chunks with visibility ranges, built one chunk per frame nearest the
player first. This sits alongside the original 960 oak trees. Past their view
range the shader paints forest canopy.

Limits: rivers and trees are visual only, so ground units cross water and pass
through trees, and structures do not clear trees. Rendering has only been run on
a software renderer, so GPU frame rate is unmeasured.

CI (`.github/workflows/ci.yml`) runs the build, CTest and Godot contracts, and
stays red until those failures are resolved. A local checkout needs
`godot --headless --path godot/project --import` once before running contracts.

## Verified tactical slice

The repository contains a native-authoritative `control_point_skirmish_smoke`
vertical slice. A player vehicle group can be selected and moved through the
existing command route; native simulation determines neutral, contested,
friendly, enemy, capture-progress, hold, victory, and defeat state. Godot
renders point feedback and the HUD objective result from native snapshots.

## Verified reinforcement slice

`reinforcement_delivery_smoke` verifies a native-authoritative one-unit delivery:
a friendly zone selection, invalid-zone rejection without resource loss, one-time
Materials/Energy deduction, native timed completion, native unit creation, and
post-delivery selection plus authoritative move command acceptance. Godot only
renders the zone, inbound F-15C transport presentation, and status feedback.

The same smoke scenario now exposes that delivery flow through the Godot
Strategic Command panel: completing one airfield authoritatively selects it as
the delivery anchor and `G` opens the panel ready to request a package. When
multiple friendly airfields are active, **Select Airfield** enters a map-click
route that accepts only a native-validated airfield. **Request Package** calls
the existing native request path. The panel is presentation-only and is
unavailable until that airfield-backed operation is configured.

Aircraft are excluded from the Field Engineer build menu. Completing an
airfield is the native prerequisite for both runway aircraft availability and
the presentation's Strategic Command delivery operation.

The regular Godot skirmish presentation starts the player with 5,000 Materials
and 5,000 Energy so construction and aircraft build flows can be exercised
without treating resource-point generation as part of this presentation slice.

The verified smoke scenario is documented in
`docs/refactor/CONTROL_POINT_BATTLE_LOOP.md`. Claims here are limited to the
assertion-backed scenario and native integration test; no strategic territory
or objective-aware AI completion is implied.
