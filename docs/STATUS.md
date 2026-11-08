# Current Status

## Core design rules

- **Research happens outside the skirmish.** It is not done in a warzone.
  `research_prerequisites` and `research_projects.json` are content for
  strategic authorization, which does not exist yet. A match has no research
  balance, queue, command or AI behavior, and command id 8 is retired.
- **Forces arrive; they are not built on the battlefield.** Almost no units are
  manufactured in the theater. They are flown in, brought in by road or railway,
  dropped by paratroopers, or landed by boat once sea control is established.
- **Fuel is shipped in or captured.** There is no time to source it from the
  earth, so nothing on the map produces fuel: no wells, pumps or refineries.

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

All 21 contracts and 9 scenarios pass on Windows and Linux (2026-11-07).
The `unit_route_overlay` contract was added on 2026-11-08; the full contracts
run passed on Windows that day. Later on 2026-11-08, after queued waypoints,
`python tools/validate.py all` passed all 37 entries on Windows (25 contracts,
10 gameplay scenarios including the new `queued_waypoint_move`, 2 performance).

Units stop within `kArrivalRadiusMeters` (0.75 m) of an order, so
`native_extension_smoke` asserts that radius, not the exact target.

From 2,000 m camera distance, clicks are approximate: site orders accept about
24 pixels of error, and an invalid structure target moves to a valid spot within
that distance. The build ghost shows the moved spot. Closer in, targets are exact.

## Fog of war and recon

Long-range recon happens before a battle, in the grand strategy view, and the
battlemap is the area it could not resolve. So terrain and civilian sites are
known from the start, but enemy forces and positions are not.

`src/simulation/vision_map.hpp` holds what each faction sees right now, rebuilt
every 0.25 seconds in 100 m cells, so ground stops being watched as soon as
nothing is looking at it. It uses each unit's `view_range` and each structure's
`structure_view_range`. The presentation only learns about enemies through
`vision_state`. Unseen enemies are not drawn and cannot be hovered or attacked.
The enemy command post and military depots stay off the map until found, then stay
there as remembered positions. A site shows the owner it had when last seen.
`fog_of_war.gdshader` darkens unwatched ground.

Sight runs over the terrain. Each observer casts rays across the heightfield, so a
ridge hides the ground behind it, and each kind of observer has an eye height
(ground 4 m, radar 40 m, aircraft 1,500 m). The fog, automatic fire, attack orders
and the AI all read this one map, so nothing can see or shoot through a hill. The
sky has its own layer that ignores terrain, so a plane behind a hill is still seen.
Earthworks change sight lines, because the height samples refresh with the terrain.

Recon sources: Pathfinder Scout Car (650 m), Longsight Recon Plane (1,600 m), Radar
Mast (3,000 m, fixed).

A recon plane flies a flight plan set when it is ordered: Strategic Command, Plot
Recon Flight, then a click on the map. It needs no airfield. It comes in from the
west map edge, flies straight to the point, circles there for
`recon_loiter_seconds` and flies back off the map. It takes no further orders.

`src/simulation/sighting_log.hpp` records where each faction last saw each enemy.
The map shows these as `?` markers with their age, merged when they would overlap.
A record goes when its unit is seen again, when a friendly unit reaches the spot
and can see it,
when the unit is seen to die, or after three minutes. Watching the spot from afar
does not clear it, because a unit that walks out of view was last seen on ground
still in view.

The AI plays under the same limits. It reads the same fog map and the same sighting
log. It is told only the sector named in the briefing (`ai.assault.search_area`),
buys two scouts and recon flights through the ordinary production line, and sends
its waves to the last known position or sweeps the stalest ground. Enemy-held sites
stay unknown to it until it sees them. Mass Warfare has its own recon units
(Outrider Scout Bike, Farwatch Recon Plane). Industrial/Experimental has none yet.

Known gaps: trees and buildings do not block sight, and units cannot hide in cover.

## The battle

Broken Strait is fought on the west landmass, with the command posts about 7 km
apart. The east landmass is scenery for now, because ground units cannot cross
the strait. Destroying the enemy command post wins, and losing yours loses:
`Simulation::begin_match` holds the result and `match_state` reports it. The match
then stops and shows an end panel.

Without an objective the AI never left its base, so `AIManager::configure_assault`
now sets one and sends waves. Units wait at home until a wave is ready (6, then 4
more each time up to 24), and reserves answer a raid within 1.5 km of their post.
The numbers live in the scenario file under `ai.assault`.

Ground units **road-march** on moves over 150 m when no enemy is within about a
kilometre, and wear 5x slower while doing it. The march multiplier is now x1.6, the
real road to cross-country ratio, because unit speeds themselves are realistic
(see "Realistic ground speeds and the rebalance").

Evidence from 2026-11-07 (`match_flow`, simulation time, idle player): the first
wave leaves at 42 seconds. With only the briefing sector to go on, the AI first
sees the player's post at about 16.6 minutes, its first wave hits at about 22.3
minutes, and the match ends the same minute. Before the AI was held to player
limits it started with a position estimate and won at about 7.9 minutes. A tank
takes about 6 minutes to reach the enemy.

Vision cost, same date (Release, Windows, 3,902 stationary units on the real
terrain, 200 ticks): 6.41 ms per tick with sight lines against 6.25 ms without, so
about 0.16 ms per tick on average for a refresh every fifth tick.

Known gaps: a command post falls in about 9 seconds to the first wave, and the AI
has not been played against by hand. Timings above predate forces arriving by
transport (next section) and the realistic speeds, and have not been re-measured.

## Forces arrive by transport

Ground units are not built on the battlefield. A scenario with
`"forces_by_transport": true` (Broken Strait) turns on
`Simulation::set_forces_by_transport`. Every ground unit order, from the player or
the AI, then goes through `Simulation::order_unit` to `order_delivery`: the unit is
paid for at once at the catalog price, and lands at the faction's landing site after
its build time plus a 20 second flight (`kTransitSeconds`). Aircraft and boats keep
their own arrival. Native rules:

- **Landing site:** just clear of the faction's active airfield (460 m beyond
  either end of its 750 m site, nearest open ground), or, for a side with no
  airfield, its landing zone. With neither, the order is refused and costs nothing
  (`AIRFIELD REQUIRED FOR REINFORCEMENTS`).
- **Cap and queue:** forces in the air count against the 2,000-unit cap and hold a
  slot; at most 24 may be in the air per faction. A force whose landing spot is
  refused tries the site itself and nearby spots, and waits rather than being lost.
- **Catalog:** `get_build_catalog` marks ground entries `delivered` and gives the
  refusal as `reason`; `delivery_state(faction)` lists forces in the air with their
  remaining time.

The Field Engineer builds structures and earthworks only: the unit section, unit
hotkeys and unit hover rectangles are gone. Forces are ordered from **Strategic
Command (G)**, now a real panel (`ui/components/strategic_command_panel.gd`) with
GROUND and AIR tabs of cards built from the unit data, an inbound list with time
remaining, fuel shipment and recon flight buttons. The old panel was the UI design
showcase scene with hard-coded cards, a RESEARCH tab and a locked research button; the
showcase scene still exists for the UI design system. The F-15C fly-in is shown for
the last 20 seconds of the next delivery, then sits parked on the runway.

The AI cannot build an airfield yet, so it is given a landing zone beside its command
post (`_apply_force_arrival_rules`). Its orders cost and take the same as the player's,
but it does not have to build an airfield first. That is an AI advantage until it can.

Not done: paratroop drops, the AI building its own airfield, sea landings, and a
refund if an airfield is destroyed with forces in the air (they wait for a landing
site instead). The legacy single-package `reinforcement_delivery_*` native calls
remain, and only anchor the parked F-15C.

## AI styles, difficulty and match records

The AI's behaviour now comes from `data/ai_doctrines.json` instead of constants
in `ai_manager.cpp`. There are four styles:

- **Balanced:** the earlier behaviour, unchanged. It keeps the scenario's wave
  numbers and builds the cheapest fighting unit.
- **Rush / Swarm:** waves of 4, growing by 2. It buys only the cheapest units,
  keeps three scouts and spends everything rather than saving for recon.
- **Turtle:** keeps a home guard of 8 that never leaves, defends out to 3 km and
  buys a mix of units. Its waves start at 12 and do not leave until the enemy
  has been sighted.
- **Tech:** buys only its most capable unit and flies recon every 90 seconds. It
  holds two units at home. Research stays outside the match, so for now "tech"
  means quality over numbers.

There are three difficulties. **Easy** thinks every 3 seconds, needs 8 seconds to
take in a new contact, gives at most 10 unit orders per decision and never pulls
damaged units back. **Normal** is the earlier behaviour. **Hard** thinks twice a
second, pulls units back at 40% health and re-tasks scouts after 40 seconds. No
style or difficulty gets extra resources or vision. The scenario chooses with
`ai.doctrine` and `ai.difficulty`, the `RTS_AI_DOCTRINE` and `RTS_AI_DIFFICULTY`
environment variables override it, and the end panel names the enemy commander.
There is no menu to pick a style yet.

Every match is recorded (`MatchRecord`). The record holds every order that ran,
every presentation call that changed the match outside the command queue, a
state hash each second, a summary of each side every 10 seconds, and the result.
Godot saves it as JSON to `user://match_records/` when the match ends. The format
is in `docs/content_format.md`. `Simulation::play_back_commands` feeds recorded
orders back in at their original ticks.

Evidence from 2026-11-07:

- **Determinism (native tests):** two 3-minute runs of the real map with the
  swarm AI agree on every one of 180 state hashes. A match played back from its
  JSON record with the AI switched off reproduces every hash, and a run with no
  orders gives different hashes, so the check can fail.
- **Pacing against an idle player** on Broken Strait (simulation time):

  | Style / difficulty | First wave | AI first sights the player | First hit on the post | Waves |
  |---|---|---|---|---|
  | Balanced / normal | 42 s | 16.5 min | 22.3 min | 12 |
  | Balanced / hard | 42 s | 16.5 min | 22.3 min | 12 |
  | Balanced / easy | 44 s | 12.6 min | 18.2 min | 10 |
  | Rush / Swarm / normal | 38 s | 6.0 min | 12.5 min | 12 |
  | Turtle / normal | 15.3 min | 15.3 min | 21.3 min | 3 |
  | Rush / Swarm / hard | 38 s | 3.1 min | 9.3 min | 9 |
  | Tech / normal | 1.7 min | 27.7 min | 36.2 min | 13 |

  An idle player only tests the search, and the search is sensitive to small
  timing changes: easy happened to find the player sooner than normal. The
  difficulty handicaps (reaction time, order limit, no retreat) only matter in a
  fight, and no AI-against-defender measure exists yet.

Known gaps:

- Only Godot matches that run entirely through orders can be played back. Actions
  such as queueing a structure are recorded but not yet replayed, and there is
  no playback tool in the game.
- A match abandoned before it ends is not saved.
- Nothing learns from the records yet.
- The tech style finds the player slowly, because it has fewer units out
  searching.
- None of the styles has been played against by hand.

## Structure sites and earthworks

`src/simulation/structure_sites.hpp` holds the placement limits. Ground within the
direct limits takes a structure as it stands. Steeper ground up to a 0.70 slope
needs earthworks, a queue job that runs first and costs Materials and fuel by
how much ground moves. It changes the simulation heightfield, so units, later
placement checks and the visible terrain all use the new height. Steeper ground
stays unbuildable, and ground within one height cell of a claimed site cannot be
levelled.

A finished structure blocks pathfinding over the same footprint the placement
rules claim (60 x 60 m, or 250 x 750 m for the airfield), rounded out to whole
navigation cells. The old hard-coded block was 250 m square for small
structures and 600 x 1,800 m for the airfield, and it sealed in the Field
Engineer that built it. The engineer now stops on clear ground outside the
footprint (`clearance_x`/`clearance_y` from `get_structure_site`). Any ground
unit still inside when a structure completes is moved to the nearest walkable
cell just outside it. The test
`an_engineer_inside_a_finished_airfield_steps_out_and_can_move` covers this.

Known gaps: network snapshots do not carry terrain edits and roads are not
checked against levelled ground.

The finished airfield is drawn inside the 250 x 750 m site its rules level:
one 45 x 700 m runway (designated 36/18), a parallel taxiway with two link
taxiways, a concrete apron, an arched hangar and a control tower
(`AIRFIELD_PAVING` in `main.gd`). The paving uses CC0 Poly Haven asphalt and
concrete photographs (`godot/project/assets/airfield/`); the ICAO-style
markings, lane joints, repair patches and tyre rubber are painted by
`structures/airfield_surface.gdshader`. All paving lies on one plane with no
overlaps. The camera's near plane now follows zoom (1% of camera distance,
0.05-40 m): the old fixed 5 cm near plane against a 120 km far plane left the
depth buffer too coarse, so flat paving flickered through the ground. The
earlier crossed 1,200 m runways reached about 300 m beyond the levelled site.

## Fuel supply

`src/simulation/fuel_supply.hpp` holds the rules, and
`godot/project/scenarios/fuel_depots.json` places the depots. A depot is a
finite store, not a source: it never refills.

- **Civilian fuel terminals:** four depots holding 1,500 fuel each, spread across
  the front and unclaimed at the start.
- **Enemy fuel depots:** two depots holding 4,000 fuel each. They keep their
  stock until captured, because the AI does not spend fuel yet.
- **Capture:** a depot changes hands when a unit gets within 40 m of it. Any
  friendly unit can do it. Stock then reaches the fuel store at 10 per second,
  and waits in the depot while the store is full. A redirected or dead unit
  abandons the capture.
- **Demolition:** a unit must be at a depot to demolish it, and the stock is lost.
- **Shipments:** with a friendly airfield, J or Strategic Command orders 2,000
  fuel for 300 Materials. It lands after 45 seconds, one at a time, and is lost
  if no friendly airfield remains. A store that cannot hold the load refuses it.

Known gaps: shipments only come by air and cannot be intercepted, the AI neither
orders shipments nor spends fuel, material sites still change hands the moment
they are ordered, and network snapshots do not carry captures or shipments.

## Content and balance

Units, factions and weapons are defined only in `data/unit_faction_stats.json`,
and terrain uses JSON and PNG. See `docs/content_format.md`.

Evidence from 2026-11-06: `rts_balance_benchmark` keeps all three ground army
pairings within its 0.12 tolerance (Elite vs Mass -0.08, Elite vs Industrial
+0.11, Mass vs Industrial +0.11). This covers ground armies in open terrain only,
without air, naval, terrain or player tactics. Simulation worst tick at the cap
is about 22 ms after the AI vision pass was rewritten.

## Windows and GPU

The game builds natively with MSVC and renders on the player's graphics card.
Run `tools/build_windows.ps1`, then `tools/run_game.ps1`. Godot's Windows build
is expected in `~/mow-tools/godot`. WSL only reaches a software renderer, so use
it for simulation work, not for judging visuals or frame rate.

`tools/gpu_benchmark.ps1` runs the real game and reports the adapter and frame
times for four camera distances. Evidence from 2026-11-07 (Release, RTX 4070 Ti
SUPER, driver 610.88, Ryzen 9 9900X, 1920x1080, vsync off, match start with
712,434 vegetation instances and only the starting force on the map):

| View | `gl_compatibility` | `forward_plus` |
| --- | --- | --- |
| Close (120 m) | 313 FPS, 3.2 ms | 273 FPS, 3.7 ms |
| Default (420 m) | 354 FPS, 2.8 ms | 298 FPS, 3.4 ms |
| Wide (2,200 m) | 384 FPS, 2.6 ms | 306 FPS, 3.3 ms |
| Strategic (9,000 m) | 568 FPS, 1.8 ms | 381 FPS, 2.6 ms |

`gl_compatibility` stays the project renderer: it is faster here, and the terrain
was tuned under it. Under `forward_plus` the same shaders look pale and washed
out, so switching needs a lighting pass. Neither column says anything about a
full battle, because thousands of units have not been rendered yet.

Windows has a 1 MiB thread stack, so large buffers and `Snapshot` values must
stay on the heap. `std::getenv` misses values set through Godot's
`OS.set_environment` there, so use `read_environment`.

## Terrain and vegetation

`tools/generate_terrain.py` deterministically generates the heightfield, with
ridged mountains, erosion and rivers routed along real drainage to the sea, plus
baked masks for rivers, forest, light and ground variation. Gameplay sites stay
level through `FLAT_ZONES`. `territory_terrain.gdshader` blends ground layers by
slope and elevation, and breaks up tiling with rotated multi-scale samples.

`terrain_vegetation.gd` adds about 950,000 presentation-only trees, shrubs and
rocks in chunks with visibility ranges, built one chunk per frame nearest the
player first. This sits alongside the original 960 oak trees. Past their view
range the shader paints forest canopy.

Trees and scrub come from EZ-Tree (MIT; bark textures from ambientCG, CC0;
see `assets/vegetation/CREDITS.md`). `tools/build_near_trees.gd` grows 19
variants with the vendored generator (`vegetation/ez_tree/`): five pines,
three each of oak, ash and aspen, two young pines and three bushes, about
5–23k triangles each, saved in `assets/vegetation/near/`.
`tools/bake_tree_impostors.gd` renders each mesh from 64 directions over the
upper hemisphere into a colour atlas (384 px frames for trees, 256 px for
young pines and bushes) and a half-size normal atlas
(`assets/vegetation/impostors/`, catalogued in `impostors.json` with each
model's mean colour and source mesh). All 38 atlases import VRAM-compressed
with mipmaps. `vegetation/tree_impostor.gdshader` draws every tree as one
camera-facing quad that blends the three nearest views, lit by the scene's
sun, and supports crowns stretched wider or narrower than their height.
Conifers grow 15–28 m on high ground, broadleaves 12–22 m lower down, young
pines 2.5–6 m in a third of the thin forest edge, and the far layer is drawn
1.5x larger. Variety: each 350 m stand favours one species of each kind (65%)
with the rest mixed in; each tree has its own crown width (0.82–1.22x) and
colour (conifers blue-green to deep green, broadleaves deep green to
yellow-olive, 3% turning autumn); each species is drawn at a target
brightness from its baked mean colour. If the atlases are missing, the old
faceted shapes stand in. Rocks are still faceted shapes.

Near trees are real models. Every tree is recorded in a 100 m grid as it is
placed (638,800 trees on Broken Strait, about 25 MB), and levelling a site
removes its trees from the grid as well as from the impostor batches. The
trees within 150 m of the camera, plus a 20 m margin, are drawn from their
EZ-Tree meshes in one `MultiMeshInstance3D` per variant; the ring is rebuilt
when the camera has moved 20 m or the trees change. The leaf and bark shaders
(`vegetation/tree_near_leaf.gdshader`, `tree_near_bark.gdshader`) shade the
model as the baker shaded the impostor (bent crown normals, darker crown
interior, the same brightness and per-tree tint), and both sides share one
dithered fade (`vegetation/tree_fade.gdshaderinc`) over 120–150 m: each keeps
the pixels the other discards, so a tree dissolves from model to impostor
with no gap and no double drawing. Bushes stay impostors. The models cast no
shadows: the sun's shadow map spans the whole 28 km theatre, so tree shadows
are a few texels and earlier cost 7–14% of frame rate.

`tree_impostors` (in `tools/validate.py contracts`) checks the catalog (every
variant loads, has its near model at the impostor's height, and the forest
has pines, oaks, ashes, aspens, young pines and bushes), that conifers,
broadleaves and young pines are planted at their height ranges with tight
bounds, that every tree is in the grid and hands over to a model while bushes
do not, that the ring holds exactly the trees within reach, follows the camera,
is empty for a high camera and drops levelled trees, and, with a GPU, that
levelling a site clears its impostors.

`tools/forest_benchmark.gd` measures frame rate over the densest forest with
and without the near models in one run (vsync off, RTX 4070 Ti SUPER,
1920x1080, Compatibility renderer, Broken Strait, camera circling 220 m out;
953,020 impostor instances in 4,264 batches):

| Camera distance | Near models | Impostors only | Earlier Poly Haven impostors |
| --- | --- | --- | --- |
| 60 m | 223 FPS (195 models, 4.1 M primitives) | 313 FPS | — |
| 180 m | 271 FPS (71 models, 1.6 M primitives) | 302 FPS | 351 FPS |
| 600 m | 388 FPS (no models) | 337 FPS | 352 FPS |
| 2,500 m | 380 FPS (no models) | 346 FPS | 353 FPS |
| 7,000 m | 369 FPS (no models) | 344 FPS | 363 FPS |

Close in, the models cost about 29% of frame rate at 60 m and 10% at 180 m.
From 600 m out there are no models, so the two columns differ only by run
order and noise. The earlier column used a different camera path over a
different forest, so it is a rough reference, not like-for-like. Startup,
from launch until the vegetation has filled in, was 15.7 s.

Limits: rivers and trees are visual only, so ground units cross water and pass
through trees, and structures do not clear trees. See "Windows and GPU" for frame rates.

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

Aircraft in flight are drawn 160 m above the land or sea beneath them and climb
or descend at 45 m/s; parked aircraft stay on the ground. The view asks the
native `get_airborne_flags` batch call which units are taking off, flying or
returning; the simulation itself stays 2D. Before this, every aircraft was
drawn at ground height, so a fighter ferried in over the sea sat under the
water. The fighter and helicopter models are now drawn at real size (F-15C and
F-14 about 19 m, Z-10 about 14 m) instead of 2.8 m, and the F-15C airframe
material is drawn opaque (`opaque_materials` in `visual_definitions.json`)
because the donor model marked it alpha-blended. `airfield_fighter_ferry`
asserts the flying fighter is drawn at least 100 m above the surface; with the
cruise height set to 0 it fails at 0.5 m.

## How units are drawn moving

The simulation steps at 20 ticks a second with native steering (turn rates,
pivot turns, acceleration and braking per unit in `unit_faction_stats.json`).
The view now draws it the way Forged Alliance does:

- **Interpolation.** Each unit is blended from its previous to its latest tick
  by the native `get_tick_clock` fraction, so motion and turning are smooth at
  any frame rate; the picture runs at most one tick (50 ms) behind. A unit that
  jumps more than 60 m in a tick snaps.
- **Slope tilt.** Vehicles on land are tilted to the terrain normal, measured
  3 m either side; aircraft in flight and vessels stay level.
- **Model facing.** Eight models were authored nose along X or backwards and
  drove sideways or in reverse. `rotation_degrees` now turns each nose to the
  game's forward (Elite MBT, logistics truck, Elite artillery and F-15C +90,
  F-14 -90, Osa, Buk and Z-10 180), checked from top-down and three-quarter
  renders.

- **One vehicle per unit.** Six visuals came from donor packs that lay several
  vehicles side by side, some merged into one mesh. A definition's `isolate`
  rectangle (`unit_visual_root.gd`) keeps only the surfaces centred inside it,
  then centres that vehicle and scales it to `length_m`: Elite artillery (one
  of two SPGs, 10 m), Industrial MBT (LAV-25, 6.4 m; its authored steering is
  wheeled), assault vehicle (T-72, 9.5 m), missile platform (MZKT radar truck,
  12 m), recon (BRDM, 5.75 m) and Swarm Tank (PT-76, 7.6 m; its old T-62 file
  had no texture). The Osa, Buk and Boxer are scaled to their real lengths.

`test_unit_motion.gd` (`tools/validate.py unit_motion`) builds every visual
through `UnitVisualRoot` and checks it is one vehicle (no ground vehicle over
14 m), drawn at its isolated real length, and longer along forward than across;
that a moving tank's drawn position and heading change in even per-frame
steps; that it faces its destination; and that it tilts on an 11 degree slope.
Each check fails when its fix is removed. The old
`vehicle.rendered_hulls_follow_motion` check passed throughout because it
measured the wrapper, not the model inside it.

Known gaps: the F-14's nose was checked by eye only (its planform is square);
a wheeled vehicle cannot reach a point inside its turning circle (an engineer
cannot reach a spot 14 m square to its side) because steering has no
three-point turn.

## Route preview while Shift is held

Holding Shift shows each selected unit's remaining route as a thin cyan ribbon
laid on the terrain, with a small diamond at every turn. It refreshes ten times
a second while units move, and disappears when Shift is released.

The simulation decides the route. `Simulation::get_unit_route` repeats what
movement does: it walks the unit's flow field one cell at a time from where the
unit stands, keeps the cells where the direction changes, and stops once the
destination is in line of sight, because from there movement drives straight
at it. A route has at most 128 points. Querying a route never changes the match.
The bridge exposes one batched, presentation-only call,
`get_unit_routes(entity_ids, player_id)`. It returns nothing for another
player's units. `unit_route_renderer.gd` only draws.

Evidence: the native test
`unit_route_query_reports_the_corners_the_unit_drives_through` orders a tank
around a wall. It checks that the route bends, that no leg of it crosses
blocked ground, and that the tank passes within 0.5 m of every corner and never
strays further than two cells plus its turning radius from the route. It also
checks that a twin match with no route queries ends in exactly the same place.
`test_unit_route_overlay.gd` (`tools/validate.py unit_route_overlay`) does the
same around a civilian block through the real extension, and checks that every
drawn vertex lies on the reported route. In a live skirmish it checks that Shift
shows the selected unit's route, that the route follows the unit, and that
release hides it.

Queued legs are now drawn too; see "Queued waypoints" below. The route is the
cell path; a vehicle's hull
swings wide of each turn by up to its turning radius. On small test grids
(1 m cells) a vehicle that clips a wall thinner than 16 cells can be pushed out
the far side by the blocked-cell escape rule. Real maps use 125 m cells, so
this was not seen there.

## Queued waypoints (Shift + right click)

Shift + right-click on the ground appends a waypoint to each selected unit's
order queue. A plain right-click still replaces the queue with one destination.
A unit drives its current destination, then each queued waypoint in order,
starting the next leg on the tick it arrives. `X` (stop) clears the current
order and the whole queue. Shift + left-click still adds to the selection, and
holding Shift alone still only shows routes. While Shift is held the route
overlay draws every queued leg in order, with its turns as diamonds and each
waypoint as a larger square.

The queue is native. `Simulation::queued_moves_` holds the waypoints after the
current move target. A queued order travels as an ordinary MOVE command with
`Simulation::MOVE_QUEUE_FLAG` (bit 31) set in `extra` beside the formation
spacing, so replays and network commands carry it unchanged. A queued group
order keeps its formation slots. A unit holds at most
`MAX_MOVE_WAYPOINTS` (32) destinations; a further queued order is rejected. A
waypoint that is no longer walkable when its turn comes is skipped. Any
non-queued move (player, AI, patrol, build approach) replaces the queue.
`get_unit_route` traces each queued leg from the previous leg's arrival point,
still capped at 128 points in total.

Bridge: `issue_move_commands` gained an optional `queued` argument (default
false), and `get_unit_waypoints(entity_ids, player_id)` returns each unit's
destinations in driving order (nothing for another player's units).

Evidence (2026-11-08, Windows, Release): native tests
`commands_queued_waypoints_visit_in_order` and
`commands_plain_move_replaces_queue_and_stop_clears_it` in
`rts_integration_tests` (where the command tests live; `rts_tests` holds only
ECS and network tests). The gameplay scenario `tools/validate.py
queued_waypoint_move` right-clicks one spot and Shift + right-clicks two more
through `main.gd` input, checks the selection is unchanged, the three native
waypoints are in click order, held Shift draws 3 legs ending at the last
waypoint, and the vehicle arrives at them on ticks 104, 217 and 324. It then
queues two more and checks that `X` clears them.

Known gaps: not play-tested live by a person yet. There is no waypoint limit
feedback in the HUD, and queued orders cannot be edited or removed one at a
time. Only move orders queue; attack, patrol and build do not.

## Realistic ground speeds and the rebalance

Ground unit `speed` in `unit_faction_stats.json` is now a realistic
cross-country speed, used in combat too, as in Forged Alliance: Elite MBT 11,
Elite Artillery 8, Elite AA 12, Swarm Tank 14, Assault Vehicle 13, Mass AA 12,
Industrial MBT 11, Missile Platform 9, Engineering 8, scout car 17, scout bike
20 m/s (previously 2 to 11). Reverse speeds rose to 3 to 6 m/s. The road march
on long moves away from the enemy (`kRoadMarchSpeedMultiplier`) fell from x5
to x1.6, the real road to cross-country ratio, so long trips keep their old
pace (Elite MBT 17.6 m/s, was 17.5) and the old crawl at 3.5 m/s over the last
150 m of every move is gone. A tank now reaches the enemy post in 396 s.

Faster closing broke faction balance: the cheap Mass swarm reached long-range
Elite and Industrial lines together and won every pairing (scores -0.61,
-0.33, +0.59 against a 0.12 tolerance). Faster shells or longer ranges did not
fix it. The rebalance, chosen with `rts_balance_benchmark`:

- Mass ground units have 60% of their HP (Swarm Tank 60, Assault Vehicle 108,
  Mass AA 90, Scout Bike 36).
- Mass no longer gets a 20% unit discount; its units cost 10% more.
- Industrial ground weapons do 88% of their damage.

At realistic speeds one fight per pairing is close to winner-take-all (a 1%
cost change swung a score by 0.3), so `data/balance_matchups.json` now lists
`separations` and the benchmark averages each pairing over starting distances
of 500 to 900 m. The tolerance is unchanged. Averaged scores: Elite vs Mass
+0.12, Elite vs Industrial -0.04, Mass vs Industrial +0.11, so two pairings
sit near the edge and the balance needs real-play tuning.

Wheeled steering now slows enough for its turn to fit a nearby slot (radius
d / 2 sin e); at the new speeds an engineer otherwise circled a destination
5 m off its side. Test changes, each for the new speeds rather than a weaker
check: the road-march test expects x1.6; the engineer's no-maneuver control
case moved from 25 m to 16 m (at 25 m it now arrives on its turning radius
alone) with a new paired check that the corrected engineer reaches it; the AI
last-seen test allows 75 s for a 1 km advance; the AI C API test uses an
Assault Vehicle, which survives the adjacent tank's first shot that now kills
a 60 HP Swarm Tank.

The regular Godot skirmish presentation starts the player with 5,000 Materials
and 5,000 Energy so construction and aircraft build flows can be exercised
without treating resource-point generation as part of this presentation slice.

The verified smoke scenario is documented in
`docs/refactor/CONTROL_POINT_BATTLE_LOOP.md`. Claims here are limited to the
assertion-backed scenario and native integration test; no strategic territory
or objective-aware AI completion is implied.
