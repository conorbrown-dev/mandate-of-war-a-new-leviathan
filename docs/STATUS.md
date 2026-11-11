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
On 2026-11-08 the Mac (Apple M1 Pro, macOS, Release, headless) passed CTest (4
programs; 270 integration and 20 unit assertions) and `python3 tools/validate.py all`
(37 of 37) after the AI airfield and paratroop drops.

Units stop within `kArrivalRadiusMeters` (0.75 m) of an order, so
`native_extension_smoke` asserts that radius, not the exact target.

From 2,000 m camera distance, clicks are approximate: site orders accept about
24 pixels of error, and an invalid structure target moves to a valid spot within
that distance. The build ghost shows the moved spot. Closer in, targets are exact.

## Fog of war and recon

Long-range recon happens before a battle, in the grand strategy view, and the
battlemap is the area it could not resolve. Terrain is known from the start.
Materials and Fuel sites, including civilian terminals, are hidden until a
friendly unit or structure scouts them; their world models and map markers then
remain visible with the last observed owner. Enemy forces remain visible only
while watched.

`src/simulation/vision_map.hpp` holds what each faction sees right now, rebuilt
every 0.25 seconds in 100 m cells, so ground stops being watched as soon as
nothing is looking at it. It uses each unit's `view_range` and each structure's
`structure_view_range`. The presentation only learns about enemies through
`vision_state`. Unseen enemies are not drawn and cannot be hovered or attacked.
The enemy command post, airfields and transport terminals stay off the map
until found, then stay there as remembered positions. A site shows the owner
it had when last seen. `fog_of_war.gdshader` now applies a lighter 24% tint to
unwatched ground rather than the previous 62% dark overlay.

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
transport (next section) and the realistic speeds.

Later on 2026-11-08, with ground fuel burn and equal starting funds (see "Fuel
supply"), the same match ends at 939 s with 11 waves and first contact at 937 s.

Re-measured 2026-11-08 on macOS (`match_flow`, balanced/normal, idle player), after the
AI had to build its own airfield: the airfield is finished at 37 s, the first wave
leaves at 69 s, the first hit on the player's post comes at 1,268 s (21.1 minutes) and
the match ends at 1,270 s after 27 waves. Earlier the same day, with the free landing
zone, a run gave the first wave at 33 s, contact at about 12.4 minutes and 17 waves.
Why the wave count rose so far (small waves that never find the player?) is not
investigated.

## Forces arrive by transport

Ground units are not built on the battlefield. A scenario with
`"forces_by_transport": true` (Broken Strait) turns on
`Simulation::set_forces_by_transport`. Standard orders from the player and AI
still use `order_delivery`: the unit is paid for at once at the catalog price,
and lands at the faction's landing site after its build time plus a 20 second
flight (`kTransitSeconds`). The player can also order a ground unit by ROAD or
RAIL from Strategic Command. Authored corridors carry these paid orders to
separate terminals without an airfield; rail travels at 45 m/s and road at
25 m/s after build time. Road corridors also reduce normal ground traversal
cost. Aircraft and boats keep their own arrival. Native rules:

- **Landing site:** just clear of the faction's active airfield (460 m beyond
  either end of its 750 m site, nearest open ground), or, for a side with no
  airfield, its landing zone. With neither, the order is refused and costs nothing
  (`AIRFIELD REQUIRED FOR REINFORCEMENTS`).
- **Cap and queue:** forces in the air count against the 2,000-unit cap and hold a
  slot; at most 24 may be in the air per faction. A force whose landing spot is
  refused tries the site itself and nearby spots, and waits rather than being lost.
- **Catalog:** `get_build_catalog` marks ground entries `delivered` and gives the
  air refusal as `reason` and separate `road_reason` / `rail_reason` fields;
  `delivery_state(faction)` lists all inbound forces, mode, remaining time and
  corridor progress.
- **Road/rail:** Broken Strait has one authored road and rail corridor per side.
  Native BUILD commands record road/rail orders for replay, deduct Materials,
  respect the common inbound and unit caps, and unload at their mode's terminal.
  Godot shows a roadbed, ballasted two-rail track, rail ties, compact terminal
  labels and traveling truck/train silhouettes. Enemy corridors are hidden
  until their terminal is scouted, then stay visible. Corridors are fixed map
  infrastructure; convoy silhouettes represent the authoritative timed order
  but cannot yet be attacked or blocked, and the AI still chooses air delivery.

The Field Engineer builds structures and earthworks only: the unit section, unit
hotkeys and unit hover rectangles are gone. Forces are ordered from **Strategic
Command (G)**, now a real panel (`ui/components/strategic_command_panel.gd`) with
GROUND and AIR tabs of cards built from the unit data, with AIRLIFT/ROAD/RAIL
choices for ground units, an inbound list with mode and time remaining, fuel
shipment and recon flight buttons. The old panel was the UI design
showcase scene with hard-coded cards, a RESEARCH tab and a locked research button; the
showcase scene still exists for the UI design system. The F-15C fly-in is shown for
the last 20 seconds of the next delivery, then sits parked on the runway.

**The AI builds its own airfield** (`AIManager::build_airfield`, since 2026-11-08). It
has no free landing zone any more: `_apply_force_arrival_rules` in `main.gd` only turns
the transport rule on. While the AI has no airfield and none queued, its Field Engineer
orders one on the nearest open ground to its command post that takes a runway
(`find_airfield_site`: rings 250 m apart out to 4 km, level ground preferred over
earthworks, every footprint sample walkable, and only sites its own funds can pay for).
The line builds it at the ordinary price and time. With no engineer alive it cannot
build one, and it orders no ground force the transport rule would refuse, so nothing is
paid for until there is somewhere to land it. On Broken Strait the airfield is finished
37 seconds into the match.

A structure order is now an ordinary recorded command: BUILD with
`Simulation::STRUCTURE_ORDER_FLAG` in `extra`, given by a construction unit. So the AI's
airfield plays back from a match record. The player's structure orders still go through
`queue_faction_structure` and are recorded as actions, which are not yet replayed.

Once the player sees an enemy airfield (centre or either runway end in sight) it is
drawn and stays drawn, with "ENEMY AIRFIELD LOCATED" (`enemy_airfields_in_sight`).
Airfields still cannot be attacked or destroyed, the player's included.

## Paratroop drops

Light ground units marked `"airdrop": true` in `unit_faction_stats.json` can be dropped
by parachute (Pathfinder Scout Car, Outrider Scout Bike and Assault Vehicle today). In
Strategic Command their cards carry a **PARADROP** button; a click on the map then picks
the drop zone. Native rules (`Simulation::paradrop_refusal`, `order_paradrop`):

- **Scouted ground only:** the drop zone must be watched by the faction when the order
  runs (the fog map), and be land a unit can stand on. Otherwise `DROP ZONE NOT SCOUTED`
  or `NO DROP ZONE THERE`.
- **No airfield needed.** A transport flies in from the west map edge at 140 m/s. The unit
  lands after its build time, the flight from the edge to the zone and 12 seconds under
  canopy (`paradrop_seconds`). About 74 s for a drop beside the player's start.
- **Same price and limits as a delivery:** paid at once at the catalog price, counts
  against the 2,000-unit cap and the 24-in-the-air limit, and waits if the cap is full.
  A refused drop costs nothing.
- **Spread landing:** each unit comes down on its own spot within 150 m of the zone,
  scattered by a fixed hash so playback is exact.
- **Recorded:** the bridge's `order_paradrop` answers at once with the refusal, then sends
  a BUILD command from the command post with `PARADROP_ORDER_FLAG`.

The map shows a ring and countdown over each zone with a drop in the air, and the inbound
list marks drops with PARADROP.

Since 2026-11-08 each drop flies in on the side's cargo aircraft: it takes off
from beyond the west map edge once the unit is ready, releases the unit within
100 m of the zone and leaves; the unit lands 12 s later. If the aircraft is
shot down first, the drop is lost, its price is not refunded and the HUD says
"PARADROP TRANSPORT SHOT DOWN" (`paradrops_lost`, `delivery_state.airdrops_lost`).
About 104 s for a drop beside the player's start.

Known gaps: the AI does not
use paratroop drops yet; Elite Precision has only the scout car to drop and Industrial
none; a drop zone that is lost between order and landing is still used.

Still not done under the transport rule: sea landings, and a refund if an airfield is
destroyed with forces in the air (they wait for a landing site instead). The legacy
single-package `reinforcement_delivery_*` native calls remain, and only anchor the
parked F-15C.

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
  fuel for 300 Materials, one at a time. Since 2026-11-08 the load flies in on
  the side's cargo aircraft (Skybridge, Packhorse or Hauler Airlifter: unarmed,
  160 HP, 90 m/s, `cargo_transport` in the unit data). It enters at the west
  map edge level with the airfield and flies straight there, which takes about
  80 s to the player's start. Anything under the route that sees it can shoot
  it down ("FUEL SHIPMENT SHOT DOWN", load lost). It cannot be ordered, the AI
  does not count it as a unit it commands, and it leaves when unloaded. A store
  that cannot hold the load, or a side at the unit cap, refuses the order.
- **Ground fuel burn** (scenario `"ground_vehicles_burn_fuel": true`, on in
  Broken Strait): every metre a ground vehicle drives draws its `fuel_per_km`
  from its side's store (MBTs 9-10 per km, Swarm Tank 5, scouts 1-2). A vehicle
  whose side cannot pay for the next step stays put, but still turns and fires.
  The HUD shows "OUT OF FUEL // N VEHICLES HALTED".
- **Materials sites change hands on arrival**, like depots: an order sends the
  unit, and the site is taken when it gets within 40 m. A redirected or dead unit
  gives up. Sites the enemy holds at the start are set with
  `economy_assign_material_site`. A view is told only of its own side's captures
  in progress; before this, `capturing` also leaked the enemy's.
- **The AI and fuel:** below 1,500 fuel with an airfield it orders a shipment
  through a recorded order, and while short with none coming it keeps 300
  Materials back from unit purchases. Short of fuel, it sends its engineer to
  depots before Materials sites, and it now treats a depot it holds as its own.
- **Equal starts:** the AI now starts Broken Strait with the player's 5,000
  Materials and 5,000 fuel. Before, its starting funds went to the wrong store,
  so it began with 3,000 Materials and no fuel.

Aiming: shots at an aircraft in flight now lead it (fire control aims where it
will be); ground targets are still fired on where they stand.

## Air and ground targets (2026-11-08)

Each weapon lists what it can engage in the unit data (`weapon.targets`), as
the real system can. The user asked for this to follow real-life warfare:

| Unit (real system) | Engages |
| --- | --- |
| MBTs, artillery, Swarm Tank, Assault Vehicle (T-72), Industrial MBT (LAV-25), Missile Platform, engineer, scouts, patrol boat | ground |
| Elite AA (Osa) and Mass AA (Buk-M3), both surface-to-air missile systems | air |
| Kestrel fighter (F-14/F-15C, air superiority) | air |
| Harrierwing VTOL (Z-10 attack helicopter: anti-tank and air-to-air missiles) | ground and air |
| Recon planes, cargo aircraft | nothing (unarmed) |

An aircraft is an air target from take-off to touch-down; parked on a runway it
is a ground target. A weapon never picks a target outside its domain, a shot
only damages things in its own domain (a shell passes under a jet, a ground
burst does not touch an aircraft overhead), and an attack order onto a target
the weapon cannot engage is refused. The rules live in `combat/air_domain.hpp`.

**Rebalance (user approved tuning stats):** the faction balance benchmark had
relied on the SAM vehicles fighting tanks. With SAMs air-only, Elite vs Mass
went to +0.177, outside the 0.12 tolerance. Taking SAMs out of the ground
armies made it worse (+0.41, +0.37, +0.14), and Mass's price did not converge
(+0.22 to +0.28 at markups 0.02 to 0.08). The fix is the Mass Assault Vehicle:
damage 55.4 to 59.5 and HP 108 to 102, a harder-hitting, slightly lighter
vehicle. Scores: Elite vs Mass +0.104, Elite vs Industrial -0.016, Mass vs
Industrial +0.045. Every neighbouring setting tried (damage 58.5 to 60.5, HP
100 to 104) stays inside the tolerance (Elite vs Mass +0.103 to +0.119, Mass vs
Industrial +0.044 to +0.087), so this is not a knife-edge. Single changes that
did not work: Swarm Tank HP 66 or 72, Swarm damage 31, Assault HP 120, Elite MBT
damage 63 or 66. Ground balance still needs real-play tuning.

Evidence (2026-11-08, macOS, Release): native tests for burn and stall, the AI's
shipments, reserve and depot choice, site capture on arrival, the cargo flight,
interception and a cargo aircraft refusing orders. Each was checked to fail with
its rule removed. In `match_flow` the idle-player match with fuel on and equal
starts now ends at 939 s (15.7 minutes): first wave at 69 s, first hit at 937 s,
11 waves, and the AI is never halted for fuel. Before the reserve and equal
starts it ran dry at about 12 minutes and never reached the player in 30.

Known gaps: the cargo aircraft uses the fighter model; network snapshots do not carry captures or
shipments; the fuel numbers have not been tuned by play.

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
because the donor model marked it alpha-blended. Until 2026-11-09 the F-15C still
drew almost black in sunlight: its meshes sat under a mirrored transform
(negative scale on z) and 26% of its triangles wound against their own normals,
so with double-sided drawing Godot took sunlit faces for back faces and flipped
their normals away from the sun. `"repair_winding": true` in its definition now
bakes the mirror into the mesh and rewinds every triangle to match its normals
when the model loads (`UnitVisualRoot.repaired_winding`, cached per model); its
own texture then lights normally. A scan of every unit model found no other
mirrored meshes and 99-100% consistent winding elsewhere. `test_unit_visual_root.gd`
asserts the F-15C has no mirrored mesh and no triangle clearly against its
normals; without the repair it reports 3 mirrored meshes, or 2,530 of 3,528
triangles wrong. Before and after images: `validation/artifacts/f15c_lighting/`. `airfield_fighter_ferry`
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

## Camera panning and the magnifier (2026-11-08)

The user reported jumpy keyboard panning and no units in the magnifier. Measured
on an M1 Pro (Compatibility renderer on Metal, rendered, panning right at 420 m):

- **Cause of the jumps:** the forest (953,020 instances in 196 chunks) built one
  whole chunk per frame, which took 100-240 ms a frame on the Mac for the first
  ~23 s of a match. The camera's easing (`clamp(delta * k)`) also lurched after
  every long frame, so camera speed swung 0.14-0.53 m/ms. Once the forest was
  built, frames were a steady 28 ms; the game's own scripts take about 0.6 ms.
- **Fixes:** `terrain_vegetation.gd` now places trees in slices of about 4 ms a
  frame (`BUILD_BUDGET_USEC`), with the same order and random draws, so the
  forest is identical; a paused build that is replaced drops its leftovers.
  Keyboard panning moves the camera with its target (`_pan_camera`), so a pan
  covers exactly speed x time; zoom and position easing are exponential
  (`1 - exp(-k * delta)`).
- **After:** while the forest builds, panning ran at 32.5 ms median and 34.5 ms
  p95 (was 139 ms p95), with the camera's speed within 0.285-0.303 m/ms. Rendered
  timings on this Mac vary between runs (one run measured 83 ms frames
  throughout), so these are indicative. The forest now takes longer to fill in.
- **Units in the magnifier:** unit models were hidden whenever the main camera was
  beyond 600 m, where screen-space icons replace them, so the magnifier (at 1/8
  of the main distance) had nothing to draw. Unit model meshes now sit on their
  own render layer (`UnitVisualRoot.MODEL_LAYER`); the main camera leaves that
  layer out beyond 600 m and the magnifier camera always draws it, and model
  detail follows the nearer of the two cameras while the magnifier is on.
- **The magnifier draws the whole world a second time**, so with it on frames
  were slower: from about 10% to 2.5x slower across these runs. Near-tree models follow only the main
  camera, so the magnifier shows tree impostors.

`test_presentation_pacing.gd` (`tools/validate.py presentation_pacing`) checks,
through the real game, that the forest builds with at most 3 frames over 50 ms
(104 before), that panning keeps an even pace through uneven frames (speed
ratio 1.00, was 2.34), and that models stay up and on the right layers for the
magnifier. Each check fails with its fix removed. The magnifier's own checks
(`test_magnifier.gd`) are now registered as `tools/validate.py magnifier`.

## Merged range outlines (2026-11-09)

Each unit's sight, radar and weapon hexes used to be drawn separately, so a
group showed a tangle of overlapping hexes. Ground units now hand their hexes to
`range_envelope_renderer.gd`, which draws one outline per side and range kind:
an edge is kept only where it is not strictly inside another hex of the same
kind and side, so where units' outlines meet they cancel and the outline shows
the group's combined reach. A lone unit keeps its own three hexes. Colours,
dashes (where sight and weapon range coincide) and draping over the terrain are
unchanged. Aircraft in flight still draw their own flat hexes at flight height,
since those sit in another plane. The union of offset hexes has a stepped edge,
so a tight group's outline shows small notches.

Cost: merging is GDScript. 20 overlapping hexes take a few milliseconds and 100
take about 36 ms (M1 Pro, headless). An outline is rebuilt only when its units
move half a metre or change, and at most one of the (up to six) outlines is
rebuilt per frame, so a large moving army spreads the work over frames.

`test_range_envelopes.gd` (`tools/validate.py range_envelopes`) checks the
geometry (a lone hex, separate hexes, a hex inside a bigger one, an overlapping
pair, a crowd of 100 where nothing is drawn inside the merged area) and, in the
real game, that a group gets one outline per kind, units stop drawing their own
ground hexes, and standing units do not redraw. With the cancelling removed,
four checks fail.

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

## Ground-unit separation (2026-11-09, reworked 2026-11-10)

Ground vehicles no longer stack. Previously nothing kept units apart, so units
ordered to one point, 3 m player formation slots and spawns on one spot ended
0–3 m apart (new native tests measured 2.9 m and 0.0 m before the fix).

- Each ground prototype has `footprint_radius` (half the hull length, default
  4 m; MBT 4, artillery 5, missile platform 6, scout car 2.9, scout bike 1.25).
- **Making room by driving (2026-11-10).** The first version pushed overlapping
  hulls apart a little each tick, so vehicles visibly slid sideways and even
  backwards without turning (the `vehicle_movement_smoke` hull-alignment check
  failed at -0.9997). The user chose "drive out of the way". `Simulation::
  separate_ground_units` (end of `prediction_phase`, per-tick hash grid) now works
  like this:
  - A *stopped* unit overlapped by more than a tenth of hull contact is asked to
    make room. A stopped unit gives way to a moving one; of two stopped units the
    higher id gives way. A unit already driving clear needs nothing more from its
    neighbour.
  - It looks for a free spot on rings around the neighbour (16 directions, up to
    6 rings from just clear of it outwards), taking the cheapest by distance plus
    how far it must turn, with a penalty for reversing, and never a spot that
    overlaps another hull or where another unit is headed. It is then given an
    ordinary move order there (`GroundSteering.yielding`), so it turns and
    accelerates like any drive. At most 16 start per tick (lowest ids first); a
    yield gives up after 8 s; any real order replaces it
    (`Simulation::is_making_room`).
  - When its old spot is clear again it drives back (to its ordered slot if it
    stood on it, so a unit shouldered aside several times does not drift), and
    gives up waiting after 10 s. Only a unit that a passer-by overlapped goes back:
    one that was stopped on top of another has no spot of its own.
  - Hulls are still pushed apart (at most 0.4 m a tick, never into blocked or
    water cells) only when nothing can drive clear: two moving units overlapping
    deeper than half of contact, and a packed crowd with no free spot (such a unit
    retries its search after 8 ticks). Aircraft and vessels have no `GroundSteering`
    and are untouched. Everything is deterministic.
- Crowded arrival: a moving unit ends its leg (and starts its next queued leg)
  when a touching unit is on its destination and is stopped or bound for the
  same spot, or when it is within 60 m of the destination and touches a stopped
  unit whose last move was to that spot. Ten tanks sent to one point form a
  settled cluster instead of circling it. A yield is not a settled goal.
- Ground formation slots are at least 1.25 hull lengths apart (10 m for MBTs),
  whatever spacing the order asks for.
- Evidence (native, Release, Apple M1 Pro). Six tanks parked 9 m apart with one
  driving through: before, 20 of the 109 steps the parked tanks made were mostly
  sideways (65% of their motion across their hulls); now none, and each tank ends
  within 0.7 m of where it stood. In the 16-vehicle `vehicle_movement_smoke`
  scenario the hull-alignment check passes again, and fails with the old
  sliding restored.
- Tests: `tests/test_unit_separation.cpp` (same-point orders, 3 m formation,
  stacked and overfilled spawns, blocked wall, determinism, plus parked tanks
  driving aside and back without sliding, two stopped tanks, a new order while
  making room, and determinism of making room). Each part fails with it removed:
  no yielding fails six tests, no return home two, letting the neighbour yield
  too three, and not reserving other units' destinations the formation test.
  Constants are in `movement_tuning.hpp`.
- `vehicle_movement_smoke`'s `formation_arrives` limit was a fixed 12 m from the
  click, which the formation grid itself exceeds: 16 vehicles on an 8.1 m grid
  average 12.17 m from its centre. It now reads each vehicle's slot from the
  simulation and requires the group's mean within 1 m of the slots' mean and every
  vehicle within 3 m of its own slot.
- Cost: with the combat benchmark's 4,000 tanks packed into two 20 m squares (the
  worst case, every tick thousands of overlapped stopped units) `combat_benchmark_
  2000` averages 10.6 ms (gate 12 ms; p95 17 ms) and `rts_skirmish_benchmark 1950
  400` averages 4.0 ms (p95 12.5, max 20.7), against 6.3 and 2.8 for the push-only
  version. Per candidate spot the search is a few distance checks, built lazily,
  and a packed crowd backs off between searches. The 1,000-unit scale benchmark is
  unchanged at 0.39 ms.
- Balance. Pushing overlapped hulls apart had been compensated for by lowering the
  Elite MBT's health from 500 to 460. Under the new rules 460 left Elite vs
  Industrial at -0.26 (outside 0.12); 490 to 520 keep every pairing inside, so
  the health is back at its original 500: Elite vs Mass +0.063, Elite vs
  Industrial -0.073, Mass vs Industrial +0.086. The balance benchmark's order
  logic stopped any unit with a move target, which would have cancelled every
  step aside; it now leaves a unit that is making room alone.
- Limits: formation slots are still assigned by selection order, so rear-slot
  units drive through front rows (the units in front now step aside and come back).
  Moving units do not steer around parked ones or each other; the parked one makes
  way, and two moving units can overlap briefly. A yielding unit counts as having
  a move target for a few seconds, which the AI and route overlay can see. Vessels
  can still overlap. Not hand-played.

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

- Mass ground units have 60% of their HP (Swarm Tank 60, Assault Vehicle 108, since
  2026-11-08 102 with damage 59.5, see "Air and ground targets",
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

## Slice 4 briefing and unit names (2026-11-08)

The startup screen already presented the scenario description and victory condition.
It now explains building an airfield, ordering forces with G, scouting and supplying
fuel. The objective banner remains the in-match objective display. HUD and hover
names now use `get_unit_display_name(entity_id)`, a presentation-only native query
of the entity's UnitData and prototype name, independent of visual wrappers. This
replaces the incomplete type-name table that omitted scouts and cargo aircraft.
Invalid IDs return an empty name; the HUD uses UNKNOWN UNIT for missing data.

Release build passed on macOS arm64. CTest discovers and passes four programs;
direct runners report 20 unit, 21 portable-snapshot and 285 integration checks.
`slice4_briefing` passes 15 assertions; replacing the name output with COMBAT UNIT
fails five of them, then restoring the fix passes again. `gdextension_boundary`
passes 70 assertions. Sandbox loopback/certificate restrictions required unrestricted
test execution. No new visual, hand-play or performance evidence is claimed.
Sound and combat effects remain open.

## UI design system v2 (2026-11-09)

The UI was inconsistent: three fonts (Courier on the briefing, a narrow sans in
the drawn HUD, the Godot default in panels), drawn HUD text at 8 to 9 px,
Strategic Command as an opaque full-screen takeover with the magnifier drawn on
top, an unstyled briefing whose START SKIRMISH button opened the 2D test route
while OPEN PRESENTATION LAB started the real match, and a build catalog with a
fixed height that overlapped the objective banner.

Now one system covers every surface (see `docs/ui-design-system.md`): tokens with
a type scale, three system-font faces (`ui/theme/mandate_fonts.gd`, DIN-style:
Bahnschrift on Windows, DIN Alternate on macOS), and a Theme that styles every
control by default with a separate keyboard focus ring. Panels share an accent
rail and corner brackets, drawn by the same helper in node panels and the
custom-drawn HUD. Hotkeys appear as key caps everywhere.

- Briefing: SITUATION / OBJECTIVE / FIRST ORDERS in sentence case; BEGIN
  OPERATION is the one primary, focused action; the 2D route is DETERMINISTIC
  TEST. `slice4_briefing` now checks the description in sentence case and the
  victory condition under OBJECTIVE.
- HUD: rebuilt layout (operation/speed, objective+logistics banner, resources
  with capacity bars, selection with segmented integrity, orders reference with
  key caps, progress cards, inspect strip with G/M/F1 hints). It also fixes the
  drawn text being scaled twice below 1920x1080.
- Build catalog: docked bottom-centre, content-sized, cost and key cap per card.
- Strategic Command: centred modal over a dimmed battlefield, raised above the
  magnifier, NATO symbol cards, short status badges, colour-coded status line,
  IN FLIGHT footer. Close is a ghost button (danger is for destructive actions).
- F1 sheet, after-action report and magnifier restyled to match.

Validation (macOS arm64, headless unless noted): build_catalog 7, hud_bridge 8,
hotkey_sheet 41, slice4_briefing 15, magnifier 11, audio_library 1,546,
game_speed 27, match_flow 80, skirmish_presentation 151, map_editor_ui 4,
test_responsive_ui 16 checks, all 0 failures; ui_design_system_showcase PASS.
`hud_bridge` first failed (catalog 552 px wide, limit 540) and passes after the
buttons were narrowed. Rendered before/after screenshots of every screen
(Compatibility renderer, 1920x1080) are under `validation/artifacts/ui_tour/`,
made with `godot/project/tools/capture_ui_tour.gd`. Not checked: Windows and
Linux font fallback rendering, other resolutions by eye, and hand play.

## Slice 4 audio library and UI playback (2026-11-08)

`godot/project/audio/` contains 382 unchanged source clips (23,477,610 bytes):
complete Kenney Interface, Impact and Sci-fi packs, rubberduck's destruction pack,
three selected Free Firearm Library WAVs, and Nayckron/qubodup's heavy engine loop.
All except the engine are CC0; the engine uses its CC BY 3.0 option. Pack notices,
engine credit, source URLs and per-file SHA-256 values accompany the assets. Kenney's
current signed-download flow resolved the earlier 403. No Sonniss assets included.

`audio/ui_audio.gd` preloads only four UI streams, limits overlap to four voices at
-18 dB on Master, and stops/releases voices on teardown. Strategic Command open/close
and native unit/fuel order outcomes play corresponding cues. Silent repeated closes
do not play. No native simulation behavior changed. Other clips remain source assets;
firearm WAVs contain source material requiring editing before combat use.

Release build and four CTest programs pass. `audio_library` passes 1,546 checks:
all files hash correctly and decode with positive duration, carry provenance, cues
start the correct streams, disabled/unknown cues are refused, overlap stays bounded,
and real accepted/refused orders choose corresponding sounds. Removing the panel-open
hook fails the integration check; restoring it passes. The teardown check waits for
the mixer's asynchronous release after stopping voices. Native direct runner counts
remain 20 unit, 21 portable-snapshot and 285 integration checks (previous direct run).

No listening, rendered, or performance claim. Combat/vehicle audio, editing/mixing,
volume UI and player-facing credits remain open; engine attribution must be included
in the shipped game's credits as well as the packaged source notice.

## Slice 4 first combat feedback pass (2026-11-08)

Native combat now publishes transient SHOT, HIT and DESTROYED evidence. Shots only
emit after a projectile is actually spawned; hits come from projectile damage;
deaths are captured before components are removed, and an alive unit leaving the
map does not explode. This adds no gameplay rules, RNG, replay content or hash
state. A shared deque retains at most 2,048 recent events. Each viewer has its own
monotonic sequence/cursor, so polling cannot disclose hidden battle counts. Reads
are non-consuming and deterministic. Match resets clear history while keeping
sequences monotonic, so old cursors cannot replay earlier matches.

Visibility is captured natively at emission: the owner knows its own events;
other sides receive only what their ground/sky vision sees at that moment.
Later scouting cannot reveal old hidden fights. The bridge's presentation query
`get_combat_events(after_sequence, faction_id)` returns only that viewer's records.
`can_see` has an optional airborne argument (default false) for current sky sight.

`combat_effects.gd`, fed by the real main loop, draws short muzzle flashes and
firing streaks, impact bursts, destruction flashes and fading procedural smoke.
It bounds live visual effects at 96 and positional voices at 12, attenuates sound
with distance and rejects audio beyond 4,500 m. Sounds are rate-limited by kind
(60 ms minimum); delayed polling skips old sounds/effects. Current sight also
removes enemy effects and stops enemy audio. Reset/teardown clears nodes/voices.
Three existing CC0 Kenney clips provide a synthesized firing boom, metal hit and
explosion; these are initial cues, not authentic weapon recordings. Tracers are
short local firing streaks using native direction/speed, not the full projectile
flight. Aircraft effects use a 160 m presentation altitude approximation.

Validation on macOS arm64, Release: `cmake --build build -j10`; CTest discovers
and passes four programs. Direct runner counts: 20 unit, 21 portable-snapshot,
290 integration tests. Five new native tests assert real event order and positions,
read-only repeated queries, identical match hashes with/without reads, hidden
battle/count suppression (including after later scouting), ground/sky ridge
visibility, reset behavior, no explosion on healthy removal and bounded history.

`python3 tools/validate.py combat_effects --timeout 20`: 65 headless checks pass
through the actual match loop, including real death/removal, geometry and positional
playback, duplicate/stale suppression, current-sight loss, expiry and hard caps.
Temporarily bypassing the current-visibility guard fails nine checks; restoring it
passes. `audio_library` (1,546), `gdextension_boundary` (70) and `match_flow` (80)
also pass. The existing audio cleanup test now measures its same 150 ms mixer wait
with a monotonic wall clock; a long setup frame could exhaust a SceneTree timer
immediately. No assertions or thresholds were removed/widened.

Rendered validation: Apple M1 Pro, OpenGL 4.1 on Metal Compatibility, 1280x720,
`test_combat_effects.gd` with RTS_COMBAT_SCREENSHOT and isolated user data. It passes
62 checks (extra smoke checkpoint, fewer active effects after advancing its age).
Inspected `validation/artifacts/combat_effects/rendered/combat.png` and
`combat_smoke.png`; engine log is beside them. This is a small rendered checkpoint,
not an FPS/scaling benchmark or a hand-played match.

Remaining: engine/aircraft loops, per-weapon sound selection/editing, volume UI,
player-facing engine attribution, art/mix tuning and hand play. No persistent
wrecks, craters or full-flight missile/shell trail system is claimed.

## Launcher build parallelism recovery (2026-11-08)

`tools/run_game` used `cmake --build build -j`: with Unix Makefiles, the missing
job count means unlimited jobs. During the reported apparent stop at
`zip_reader.cpp.o`, process inspection found 1,917 compiler processes on the
10-core/32 GB Mac (about 44 GiB aggregate RSS, including shared/driver memory).
The last printed filename was not a ZIP code failure; the build was overloaded.
The confirmed launcher/build process group was interrupted and its compilers exited.

The launcher now builds only `rts_gdextension`, using eight jobs by default or a
positive `CMAKE_BUILD_PARALLEL_LEVEL` override. Invalid/zero values fail before
configuration. `--no-build` remains available. The interrupted Release build was
recovered with eight jobs, and `./tools/run_game --headless --quit-after 2` passed
end-to-end (extension handshake and heightmap load). Temporary command stubs also
asserted default eight jobs, override three, no-build bypass and zero rejection;
`sh -n tools/run_game` passes. No gameplay code changed for this fix.

## Keyboard and Mac trackpad zoom (2026-11-08)

The battlefield now accepts +, = and numpad + for zoom-in; - and numpad - zoom
out. Held keys repeat. Ctrl/Command/Alt combinations are left alone. Unhandled
input preserves text entry in focused controls. F1 lists keyboard and pinch controls.
macOS `InputEventMagnifyGesture` was previously ignored; it now drives the same
cursor-anchored camera path as wheel input with `distance / factor`, so spread
zooms in, pinch zooms out and small incremental deltas accumulate continuously.
Nonfinite, nonpositive and neutral factors are ignored. Existing 24–26,000 m
camera limits and easing remain. No native gameplay state or selection changes.
Two-finger scrolling/panning is not repurposed as pinch.

`camera_zoom_input` passes 37 checks through Viewport GUI/unhandled dispatch:
all aliases, key release/repeat, modified shortcut avoidance, magnification and
round trips, small deltas, invalid input, clamps, terrain cursor focus, rendered
camera-distance state, unchanged selection and native tick, focused text entry
and F1 descriptions. Its input is explicitly viewport-local: headless macOS has
a tiny OS window scaled to a logical desktop viewport, so injecting logical
coordinates as OS pixels distorts them. The headless OS cursor is stationary;
keyboard anchors are checked against the actual viewport cursor, gesture anchors
against the supplied local point. Reversing the reciprocal factor fails five
checks; restoring it passes. The existing 25 m terrain-anchor criterion is
reused. The test setup was corrected to use viewport-local coordinates rather
than assuming synthetic mouse motion moves the physical headless OS cursor.

`hotkey_sheet` passes 41 checks; `strategic_zoom_transition` passes its nine
scenario assertions, including unchanged wheel behavior and cursor anchoring.
Release build passes. Physical MacBook pinch delivery has not been hand-tested;
these checks inject Godot gesture events, not hardware trackpad input. No new
rendered screenshot or performance claim.

Camera follow-up native verification: CTest discovers and passes four programs;
runner output reports 20 unit, 21 portable-snapshot and 290 integration checks.


Landing presentation follow-up (2026-11-08): the delivery F-15C now targets the
main runway's -55 m local-X centre, supplied from AIRFIELD_PAVING, rather than
the airfield/taxiway centre. A tangent-continuous curved entry leads to straight
final, smooth descent, touchdown 220 m before centre and rollout to rest. The
nose follows the path with pitch and a shallow bank. Native delivery timing and
unit creation are unchanged; this is a delivery visual, not flight physics for
all combat aircraft.

Validation: Release build passed; landing_approach passes 16 assertions sampled
at 1,001 points for each runway direction (heading change <1 degree per sample,
continuous position/descent, final alignment, touchdown bounds, stop and actual
mesh placement/orientation). Restoring the old centre offset fails two assertions.
reinforcement_delivery_smoke passes headless and rendered, with the final view
focused on the main runway. Artifacts: validation/artifacts/
reinforcement_delivery_smoke/20261109T004455.882803Z/. Four CTest programs pass;
direct runner summaries: 20 unit, 21 portable snapshot, 290 integration. No new
performance benchmark; physical hand-play of the landing remains to be done.


Construction skeleton follow-up (2026-11-08): unfinished structures now use the
same shared geometry builder as the placement preview, with a translucent cyan
construction material. Outpost, radar, airfield (including full paving footprint,
hangar and tower), and floodlight retain their placement dimensions/transforms
throughout construction. Native queue targets and rules are unchanged.
Release build passed. construction_preview passes 37 checks; temporarily restoring
the generic scaffold fails all 12 geometry comparisons, then restoring the fix
passes. Final report: validation/artifacts/construction_preview/20261109T010534.506670Z/report.json.
No rendered artifact or performance benchmark was produced; hand visual check remains.
CTest discovers four programs: three pass, integration fails the existing aircraft
approach test's "ground rollout stays within the long runway" assertion. Runner
summaries: 20 unit passed; 21 portable snapshot passed; 290 integration passed,
one failed. The broad skirmish_presentation run completed 188 checks without
assertion failures but its report is FAIL due to engine certificate/shutdown errors.
The focused contract passes outside the sandbox with no engine errors.


## Real fighter landing correction (2026-11-08)

User play invalidated the previous landing-complete claim: the change above
covered the decorative ground-delivery F-15C only. The real paid runway fighter
still used `Simulation::update_ferry_flights`, snapped within 150 m to the
installation origin, stopped there, and then sank from cruise altitude in
`main.gd`. The old ferry scenario ended at map ingress and could not catch this.

Reference inspected read-only: `/Users/praxis/Games/Supreme Commander Forged
Alliance/gamedata/units.scd`, entries `units/UEA0102/UEA0102_unit.bp` and
`units/UEA0303/UEA0303_unit.bp`. Their Air sections specify Winged, MinAirspeed,
TurnSpeed and BankFactor (UEA0102: 10, 1.5, 2; UEA0303: 8, 1.9, 5). These
establish forward flight, turn response and bank as separate concerns; they are
not metre-scale constants to copy into this game. No SupCom assets/code copied.

The real fighter now enters west of the map and 4.5 km north/south of its own
airfield. Native `AirfieldApproach` guides a continuous turn, with zero curvature
at its ends, onto a straight final 1.8 km beyond the runway centre. Descent/flare
follow forward distance; touchdown is 220 m before centre, followed by ground
braking to rest. The authoritative centre is site X minus 55 m, matching the
45 x 700 m runway rather than the taxiway at X plus 12 m. The single runway is
retained. Aircraft keep heading after stopping; reset clears approach state.
The old generic movement cannot move a managed approach a second time. Returning
sorties use the circuit too and reserve additional fuel/time for it.

A batched presentation query, `get_aircraft_approach_poses(ids)`, returns altitude
AGL, pitch and bank, or (-1,0,0) for each unmanaged/unknown ID. Godot interpolates
these with native tick poses instead of lowering a stopped aircraft over wall
time. Other units continue to use their existing presentation path.

Validation: Release `cmake --build build --parallel 8`; all four CTest programs
pass (20 unit, 21 portable-snapshot, 291 integration tests). A new native test
tracks the real fighter from both runway directions on every tick: bounded
steps/turns/bank, forward speed while airborne, centreline descent, runway rollout,
parked heading and reset. The expanded `airfield_fighter_ferry` runs all the way
to rest: 21 headless checks / 27 rendered checks. Restoring the old altitude
renderer fails descent agreement and the no-vertical-drop-after-stop assertions;
restoring the corrected renderer passes. Rendered diagnostics: maximum 6.020 m
step and 0.258 degrees turn per 50 ms tick, minimum flying step 3.544 m.

Final rendered report and six checkpoints (including banked turn, final,
threshold and parking):
`validation/artifacts/airfield_fighter_ferry/20261109T011021.921016Z/`.
Boundary 73, unit motion 58, match flow 80, and ground-delivery smoke regressions
pass. No scale/FPS benchmark claim. Interactive hand-play is still needed;
this fixes ferry/return landing guidance, not all combat flight or takeoff rules.


## Aircraft overlays and separate radar/intel coverage (2026-11-08)

Aircraft strategic icons now anchor to the interpolated model pose plus 5 m,
with the icon another 22 screen pixels above its leader anchor. Their attack,
view and intel hexes remain horizontal at flight height, independent of banking.
Ground hexes follow terrain, including edge samples for long ranges; terrain
edits invalidate stationary geometry. Aircraft reuse shared flat meshes and
stationary ground rings keep their meshes.

The previous universal 10/15/22 m rings are gone. `get_unit_range_profile` reads
native weapon range (zero for unarmed units), UnitData sight and new authored
`intel_range`, cached when the presentation wrapper is created. Examples:
Elite MBT 115/150/450 m; fighter 120/160/1,200 m; scout 60/650/1,300 m.
Equal weapon/sight radii use alternating red/blue segments on the same boundary;
zero ranges are hidden. The colors remain red attack, blue sight, purple intel.

The user explicitly chose a separate gameplay intel/radar range. Unit data now
has optional finite `intel_range` (0–10,000 m; default 0), copied into native
sensors. A separate terrain-aware coverage map refreshes with visual coverage
(every 0.25 s); its sky layer detects aircraft over ridges, like the visual sky
layer. This is game-scaled sensor tuning, not a real-world radar simulation.

Radar-only enemies become anonymous occupied 100 m cells. The presentation gets
only x/z pairs, with no IDs, type, health, count or precise tracking; nearby
screen markers merge. Purple question-mark hexes appear without revealing models,
fog, target locks or visual sight memory. Contacts vanish when coverage is lost
and visually identified contacts are not drawn twice. AI uses the same anonymous
positions for navigation and still needs visual sight to identify/fire.

Validation: Release build and all four CTest programs pass: 20 unit, 21 portable
snapshot, 294 integration tests. New native tests assert detection without sight,
identity or firing; coarse coordinates, faction isolation, range exit, terrain
occlusion/earthworks, sensor loss/reset, optional/default/invalid content values.
`unit_range_overlays` passes 39 headless / 40 rendered checks: native radii, generated geometry, actual icon
projection, flight-plane placement, terrain conformity, reuse, unarmed/equal
ranges, privacy and unchanged simulation. Restoring ground icon anchoring fails
two checks. `airfield_fighter_ferry` passes 29 rendered checks, including native
fighter flight-height icon and all three range envelopes. Boundary 75, unit-root
18, skirmish presentation 151 and match flow 80 checks pass. Match flow now
reaches first contact at 914 s/defeat at 917 s in its idle-player fixture.

Rendered aircraft checkpoints:
`validation/artifacts/airfield_fighter_ferry/20261109T012702.024404Z/`.
Ground range spacing and a radar-only contact:
`validation/artifacts/unit_range_overlays/rendered/radar_ranges.png`.
Native 1,000-moving-unit intel comparison is in PERFORMANCE.md (1.171 ms off,
1.515 ms on average); no rendering/FPS claim. Interactive tuning of sensor
balance and dense-battle overlay readability remains open.


## Game speed and zoom-key remap (2026-11-09)

This supersedes the earlier +/- zoom bindings. Plus, equals and numpad plus now
increase non-multiplayer game speed by 25 percentage points; minus/numpad minus
decrease it. The seven settings are 25, 50, 75, 100, 125, 150 and 175%, clamped
at either end. New games start at 100%. Slash/numpad divide cycles camera-distance
presets 24, 120, 420, 1,500, 4,500, 12,000, 26,000 m, then wraps to 24 m. From
an arbitrary wheel/pinch zoom, slash picks the next wider preset. These keys use
one step per press and ignore echo; modified shortcuts and focused text entry
are preserved. Wheel and magnify gestures keep their existing behavior.

`Simulation::set_game_speed` owns pacing. The bridge exposes set/get speed and
multiplayer-session state; GDScript does not alter the native tick size. Incoming
wall delta is capped at the existing 250 ms safety limit, multiplied by speed,
then added to the accumulator. Fractional time is retained, so four 250 ms frames
correctly produce five native ticks at 25% and 35 at 175%. Native movement, AI,
production, combat, fuel and delivery timers all continue using 50 ms ticks.
Camera/UI timing and audio playback use ordinary wall time, not Engine.time_scale.

Native successful TCP listen/connect latches multiplayer mode until session
reset. Its effective speed is always 100% and its setter refuses local changes.
The Godot control path also guards a non-offline MultiplayerPeer and restores
normal pacing before advancing it. HUD displays the current percentage and the
multiplayer lock. F1 has a Game speed section; the orders card moved down to make
room for the speed line within the top-left identity card.

Release `cmake --build build --parallel 8` passes. CTest discovers and passes four
programs: runner totals 20 unit, 21 portable-snapshot and 296 integration tests.
New native cases assert all seven tick rates on short and long frames, movement
hash equivalence to ordinary fixed ticks, invalid values, reset and a real native
TCP listener forcing normal speed. `tools/validate.py game_speed` passes 27 checks
(28 rendered): real input dispatch, aliases, bounds, repeat/modifier handling,
exact pacing in the actual game loop, a real loopback Godot ENet multiplayer
listener, HUD lock, text entry and lifecycle reset. Restoring the old +/- zoom
route fails ten checks; restoring the remap passes. Camera input 48, hotkey sheet
41, boundary 78 and existing strategic zoom nine checks pass. The first lifecycle
test was corrected to use stop/start: bridge start is deliberately idempotent.

Rendered speed/HUD checkpoint:
`validation/artifacts/game_speed/rendered/game_speed_175.png`.
Headless report: `validation/artifacts/game_speed/20261109T125634.890865Z/`.
No new scale/FPS benchmark; this is a bounded scheduling/input change. Manual
play feel and physical keyboard/trackpad checks remain open.


## Slice 4 implementation closeout (2026-11-09)

This entry supersedes the earlier Slice 4 engine/per-weapon/settings/credits TODOs.
The slice's implementation is complete; human listening and full-match feel are
still acceptance tasks, not claims supported by automation.

- `audio/audio_palette.gd` chooses cannon, artillery, missile, rifle and burst
  cues for armed unit types. Two CC0 firearm derivatives are trimmed, converted
  to mono PCM16/24 kHz, normalized and faded. `tools/prepare_audio.py` reproduces
  both WAVs and the manifest byte-for-byte. All 382 source files remain unchanged;
  the manifest now covers 384 audio assets with provenance/hashes/edit records.
- `audio/engine_audio.gd` polls the already presented unit poses and native tick/
  airborne flags at up to 10 Hz. Only moving ground units or airborne aircraft
  that the player can currently see qualify. The nearest eight inside 1.8 km
  retain stable positional voices; sight loss/removal stops them immediately.
  Idle ground units, distant units and completed/reset matches are silent.
  Separate ground/air OGG loops are duplicated in memory, with fade-in and
  speed-based pitch. Game-speed controls do not change audio playback rate.
- Four UI, 12 Combat and eight Engine voices route through separate buses to
  Master. Defaults are 80/80/80/60 percent; Master has a -1 dBFS limiter. Initial
  voice gains are UI -18 dB, Combat -16 dB, ground engine -26 dB and aircraft
  -20 dB, compensating for the latter's quieter steady source level.
- F2 opens Audio & Credits before/during/after a match; the briefing also has a
  button and the HUD/F1 advertise it. Sliders apply immediately, zero mutes and
  `user://audio.cfg` persists preferences. Restore Defaults and Esc/F2/Close work
  with keyboard focus. The match keeps running, while tactical hotkeys are
  blocked. Credits expose Nayckron/qubodup, source and CC BY 3.0 links and runtime
  gain/pitch changes, plus the CC0 sources. Aircraft/heavy-weapon sounds remain
  stylized layers, not authentic recordings.
- Visual review caught an uninitialized magnifier camera at world origin before
  the first battlefield hover. The real match now seeds it from the starting
  camera focus. It still holds/dims over HUD/off-map and shares world visibility.

Validation on Apple M1 Pro, macOS arm64, Release:

```
cmake --build build --parallel 8
ctest --test-dir build -V
python3 tools/validate.py contracts --timeout 60
python3 tools/validate.py slice4_audio --timeout 45
python3 tools/validate.py airfield_fighter_ferry --rendered --require-screenshots --timeout 90
python3 tools/prepare_audio.py
python3 tools/check_audio_mix.py validation/artifacts/slice4_audio/listening/audio_mix.wav
```

Build passes. CTest discovers/passes four programs; direct runner summaries are
20 unit, 21 portable-snapshot and 296 integration tests. The final Godot sweep
passes all 37 contracts, including audio library 1,554, new Slice 4 audio 30,
combat effects 65 (66 after the final flash assertion), magnifier 14, briefing 15,
full match flow 80, boundary 78,
camera 48, game speed 27 and hotkey sheet 41. The initial sweep saw a stale range
assertion during separate workspace range-envelope changes; its focused rerun
and final complete sweep pass. Those unrelated edits were preserved.

The audio contract runs real moving/hidden vehicles and a paid paradrop cargo
flight through the actual match loop. It asserts positional looping, distinct
weapon streams, current sight, stop/end cleanup, eight nearest voices, persisted
mute/volume, real slider input, modal hotkey blocking, credits and unchanged
native time. Removing the engine visibility filters fails the moving hidden-enemy
check; restoration passes. Intentional negative-control evidence is retained at
`validation/artifacts/slice4_audio/20261110T004735.252001Z/`.

Rendered audio contract passes 31 checks. All eight main UI surfaces were captured
and inspected at 1280x720 and 1920x1080 under macOS OpenGL Compatibility, including
briefing, real unit name/selection, seeded magnifier, Strategic Command, F1,
after-action and audio/credits. Paths:
`validation/artifacts/slice4_ui/{1280x720,1920x1080}/` and
`validation/artifacts/slice4_audio/rendered/audio_settings.png`.
The actual fighter ferry/landing scenario passes 29 rendered assertions, with
screenshots in `validation/artifacts/airfield_fighter_ferry/20261110T005904.345215Z/`.
Combat flash/smoke evidence is in `validation/artifacts/combat_effects/slice4_closeout/`.
Final visual review caught a lingering opaque explosion flash during the smoke
phase. Expired flashes now explicitly stop drawing as well as reaching zero
alpha. The rendered contract passes 63 checks with fresh-frame captures; the
new state check runs headless too, retaining every original shot/hit sight-loss
fixture (66 final headless checks). Forcing flash visibility back on fails that
check; restoring the fix passes. No baselines/tolerances were changed.

`tools/capture_audio_mix.gd` records four UI cues, five weapon families, complete
engine cycles and overlapping destruction through the actual buses/limiter.
It marks cues with captured audio frame counts, checks for capture-buffer drops
and records actual final engine gains. `audio_mix.wav`, cue timings and measured
`levels.json` live in `validation/artifacts/slice4_audio/listening/`. The final
23.50-second PCM capture passes 16 checks: all segments have signal, no dropped
meter frames, no clipped samples, peak -17.90 dBFS (below the -1 dBFS ceiling).
Steady engine segment RMS is approximately -47.25/-47.21 dBFS at a 300 m listener
separation. This is a measured synthetic audition fixture, not human listening
or a dense-battle loudness guarantee. A bounded allocation microbenchmark is in
PERFORMANCE.md; no rendering/FPS claim is made.

Known remaining acceptance: human listening/full-match hand play and feedback
on fuel, balance and pacing; physical keyboard/trackpad delivery; Windows/Linux
font and rendered appearance. No Slice 5 is defined or automatically started.


## Feature and bug follow-up — 2026-11-09

The user approved water inside Materials and a hold-key-sites victory. Broken
Strait now configures three native Material/Fuel objectives and wins after a
continuous two-site hold for 120 seconds; command-post destruction still takes
precedence. Native tests cover interruption, retake, loss and post survival.
The Godot fixture passes 14 checks including the HUD and end panel. Scenario
validation rejects malformed hold rules.

Civilian dressing now uses four sourced Poly Pizza models with restrained forms:
Kenney residential block and office tower (CC0), Lavender Harmony weathered
warehouse (CC BY 3.0), and Quaternius water tower (CC0). The warehouse's required
credit appears in F2. [BUILDING_ASSETS.md](BUILDING_ASSETS.md) records model pages,
licenses and SHA-256. The renderer scales each imported model uniformly, grounds
its bounds, and rejects slopes over 12 m across the footprint. The rendered
`civilian_building_models` scenario passes, with neighborhood and four close
screenshots in `validation/artifacts/civilian_building_models/20261110T024520.064664Z/`.
The user-suggested Sketchfab architecture gallery was inspected; the relevant
recent downloadable industrial study was CC BY and 99.3k triangles, so it was
not added to a repeated RTS scenery layer. Basic utility boxes remain.

The previously queued UI hover, build descriptions, queued engineer builds,
NATO/resource markers and command-post work pass their focused Godot contracts.
Ground-unit hull separation now runs without the temporary bypass and passes
crowded-spawn state assertions. Single-player no longer captures an unused
network snapshot each tick; combat event ownership is checked first and live
observers are cached for no-fog combat. Crowded separation work is bounded.
The equal-budget faction benchmark exposed the resulting Elite advantage; a
health tune from 500 to 460 brought all three matchups inside its unchanged 12%
gate (see "Ground-unit separation": since the 2026-11-10 rework crowded units drive
aside instead of sliding and the health is back at 500). Release `cmake --build build --parallel 8` and all four CTest programs pass:
20 unit, 21 snapshot, 304 integration, and `BALANCE_OK`. The 4,000-unit active
combat average is 9.001 ms/tick; native-only details and limits are in
[PERFORMANCE.md](PERFORMANCE.md).

The first full Godot sweep found a stale HUD objective string and a legacy
scenario's draw-only expectation. The HUD now names both victory paths and
the legacy fixture checks that the displayed result matches native state;
focused reruns pass (`match_flow` 80, native route 19). The final full sweep
passes all 42 Godot contracts. Human battle-onset play is still pending; no
FPS claim is made.

AI paradrop follow-up (2026-11-09): on a decision tick, an AI with a visible
forward enemy contact and transport rules enabled looks for a walkable,
currently watched point 350 m to its side. It chooses its faction's existing
airdroppable recon unit, checks the ordinary paradrop refusal/cost rules and
issues the same recorded command as the player. Accepted orders have a 90-second
cooldown and use the existing interceptable cargo aircraft and drop pipeline.
The native scenario asserts no blind or unpaid order, a visible flank target,
one inbound scout and no decision-tick spam. Removing the call makes its
assertion fail.
Release build and all four CTest programs pass (20 unit, 21 snapshot, 305
integration, balance); Godot `match_flow` passes 80 checks and
`native_skirmish_match_to_result` passes 19. No rendered artifact or new
performance measurement was made; tactical quality awaits hand play.

Resource-map verification (2026-11-09): Broken Strait's strategic overlay draws
four Materials sites and four known civilian fuel terminals at all camera zooms.
Two enemy military depots are marked only after discovery; their ownership uses
last-seen intel. The `strategic_map_symbols` contract passes 299 checks.
The separate native 2D skirmish now draws the two resource nodes returned by
`skirmish_state`: METAL as Materials and the legacy ENERGY node as Fuel. Markers
follow node coordinates and remain marked when depleted. Its Godot route contract
passes 23 checks, including type, count and location; omitting the draw call
fails four checks. A 1280x720 Compatibility-renderer screenshot was inspected at
`validation/artifacts/resource_map/rendered/native_resource_markers.png`.
Release build and all four CTest programs pass. Water has no independent resource
node or marker because it is part of Materials; municipal water towers are scenery.

## Command post (2026-11-09)

The player's command post is selectable. A left click on the compound (units
nearby take the click first) selects it on its own: it never enters
`selected_ids`, so move, attack and stop orders cannot reach it. Box selection
leaves it out. A bottom-left panel (`ui/components/command_post_panel.gd`)
shows its name, native integrity (1,500), role, the forces and fuel shipment in
flight with times, and two actions: Strategic Command (G) and Request Fuel
Shipment (J, disabled while one is inbound). A right-click with the post
selected is refused with a notice. Previously a right-click with nothing
selected fell back to `commander_ids[0]` (the post's native entity), which could
drive the post's entity away from its drawn compound; that fallback is gone for
move and attack. The enemy post can be hovered only once discovered (integrity
only while in sight) and is never selectable.

The model (`structures/command_post_model.gd`) is a 56 x 42 m forward command
post from primitives and the in-repo airfield concrete texture: graded pad,
HESCO perimeter (one MultiMesh) with a south gate, two-storey prefab HQ with
window bands, faction roof marking and stripe, satellite dish, two antenna
masts with warning lights, vehicle bay with a parked truck, containerised
generator and fuel bladder in a sandbag ring (one MultiMesh), canvas tent,
emissive floodlights and a flag. At most 60 mesh instances and 12 shared
materials (asserted); only the accent material is per faction. It sits level on the
highest ground under its footprint with a skirt down to the lowest. The Field
Engineer now starts outside the south gate instead of 4 m from the centre.

Evidence: `python3 tools/validate.py command_post` passes 69 checks (click
selection, HUD content, airfield-backed fuel and force inbound rows, both
actions, refused orders leave the native post still, box selection, enemy
hover/no-select, model parts, footprint, grounding, draw cost, faction colour).
With the fixes removed it fails 31 checks. `match_flow` (80), `skirmish_presentation`
(204), `audio_library` and `hud_bridge` pass. Rendered macOS captures before and
after are in `validation/artifacts/command_post/` (from
`capture_command_post.gd`). Not hand-played.

## APP-6 map symbols and resource markers (2026-11-09)

At strategic zoom (camera 600 m and above) `StrategicIconOverlay` draws each
shown unit as an APP-6 / MIL-STD-2525 symbol from `nato_map_symbol.gd`, vector
drawn at about 26 x 17 px. Frames follow affiliation and dimension: friendly
land rectangle, hostile land and hostile sea diamond, friendly sea circle,
friendly air dome and hostile air pointed arch (both open at the bottom).
Fills are the standard light blue and red with black frames and icons. The
function comes from `strategic_icon` in the unit data: armour (track oval),
self-propelled artillery (dot over a track oval), air defence (base dome with a
missile, since both AA vehicles are SAMs), surface-to-surface missile,
reconnaissance (slash), engineer, fixed-wing fighter F, recon R and cargo C, and
rotary-wing attack (bow tie with A). The patrol boat uses a surface-combatant
hull. No echelon or mobility modifiers beyond those.

Data fixes in `data/unit_faction_stats.json`: `strategic_icon` is now the APP-6
function key (`armour`, `sp_artillery`, `air_defense`, `missile`, `recon`,
`engineer`, `fighter`, `recon_aircraft`, `cargo_aircraft`, `attack_helicopter`,
`naval_surface`). Build-catalog `nato_symbol` corrections: Elite Artillery
`artillery` to `self_propelled_artillery`; Harrierwing (a Z-10 attack helicopter)
`fighter` to `attack_helicopter`; scout car and bike `light_armor` to `recon`;
recon planes `fighter` to `recon_aircraft`; the three airlifters `fighter` to
`transport_aircraft`. The Missile Platform keeps `rocket_artillery` in the
catalog (the UI icon registry has no surface-to-surface missile entry).

Resource sites get screen-space markers at every zoom: a square badge with
ingots for Materials and a round badge with a fuel drop for fuel, labelled
MATERIALS, FUEL or FUEL MIL, filled by the owner the player last saw (blue
friendly, red hostile, amber unclaimed). Materials sites and civilian fuel
terminals were known from the start in the 2026-11-09 baseline. The
2026-11-10 scouting change below hides all resource sites until discovered.
Markers read `known_owner`, never the true owner.

Evidence: `python3 tools/validate.py strategic_map_symbols` passes 299 checks
through the real match scene (what `_draw` put on screen). With a fix removed it
fails: no resource draw call (3 checks), ignoring discovery (4), true owner
instead of `known_owner` (2), frames ignoring affiliation/dimension (25), old
unit data values (25). `strategic_zoom_transition`, `skirmish_presentation`
(204), `unit_range_overlays`, `hud_bridge`, `build_catalog`, `visual_registry`,
`visual_presentation_policy`, `unit_visual_root`, `match_flow` and
`test_ui_icon_registry.gd` (19) pass. `skirmish_presentation` intermittently
reports a leaked UI audio clip at exit (`confirmation_001.ogg`), unrelated.
Rendered macOS captures are in `validation/artifacts/agent_b_nato_resources/`.
The patrol boat's sea frame is asserted by data only; it could not be spawned
on the land test fixture. Not hand-played.

## Build menu hover and queued engineer builds (2026-11-09, Agent A)

Presentation only; no native code changed. `queue_faction_structure` still
decides every structure order.

- **Hover ticks.** `audio/ui_audio.gd` has a fifth, dedicated `HoverVoice` on the
  UI bus (Kenney Interface `tick_002.ogg`, already in the repo, -28 dB against
  -18 dB for order cues; `manifest.json` `hover`). Every `MandateButton`
  (so build catalog entries, Strategic Command ORDER/PARADROP/CLOSE/logistics
  buttons, end panel, audio settings) and the `MandateTabs` strip tick once per
  pointer entry, never while disabled, never when `ui_audio.enabled` is false,
  and follow the UI bus volume/mute. A sweep restarts the one hover voice and
  cannot cut off an order cue. The HUD's ORDERS panel is a drawn key reference,
  not buttons, so it has no hover. The startup overlay's plain `Button`s do not
  tick yet.
- **Descriptions.** `ui/build_descriptions.json` holds short factual text keyed
  by unit type key (via `unit_catalog.gd`, from the native catalog's numeric
  type) and by structure (FORWARD_OUTPOST, RADAR_MAST, AIRFIELD, FLOODLIGHT);
  `ui/build_descriptions.gd` looks them up. The build catalog shows the hovered
  structure's text on a three-line detail line under its buttons (hint text
  otherwise) and in the button tooltip; Strategic Command shows
  `NAME // description` under its cards and in the card tooltip.
- **Queued builds.** In placement, Shift+left click queues the site and keeps
  placing; the first goes to the engineer (`pending_build_order`), later ones
  wait in `queued_build_orders`. When the engineer reaches a site it hands it to
  the native queue and drives to the next; a site it cannot reach is skipped.
  Each waiting site has a translucent cyan footprint numbered with its place
  (`QueuedBuildMarkers/QueuedBuild_N`). Escape/right-click end placement and
  keep the queue; a plain click replaces the queue. Handing a site over no
  longer requires the engineer to still be selected (previously a reselect
  silently dropped the order). There is no cancel-queue key yet besides a plain
  click.

Evidence: `build_menu_hover` (new, 81 checks, real pointer motion pushed into
the viewport) and `queued_builds` (new, 24 checks, real `_unhandled_input`
Shift+clicks, native acceptance in order) pass. With the hover connection
removed from `MandateButton`, `build_menu_hover` fails 3 checks; with the
description hover connections removed, it fails 3; with the Shift branch
disabled, `queued_builds` fails. `audio_library` now expects four cue voices
plus the hover voice. Not hand-played or listened to.

## User-requested transport and gameplay fixes (2026-11-10)

Broken Strait now has authored road and rail corridors for each faction. Ground
forces can be ordered by AIRLIFT, ROAD or RAIL in Strategic Command. Each order
is a native recorded BUILD command, pays Materials once, observes the same
24-inbound and live-unit caps, and unloads at its own terminal. Road travel is
25 m/s; rail travel is 45 m/s; build time and an eight-second departure stagger
are included in ETA. The authored road also lowers ordinary ground navigation
cost. The rail has ballast, spaced ties and two steel rails. Trucks and trains
travel visibly along the route using native progress; the vehicles are
presentation-only silhouettes rather than attackable logistics entities.
Enemy corridors remain hidden until their terminal is scouted, then persist.
Road/rail routes are fixed in Broken Strait; engineer rail placement, route
damage and AI use of road/rail remain future extensions.

Clicking the player's command post opens Strategic Command on the first click,
including when another unit is selected. The Kestrel fighter's native weapon
now engages ground as well as air; an assertion checks actual projectile
damage in both domains. Ferried fighters finish the curved approach and runway
rollout, then taxi at 8 m/s along the runway exit/taxiway to distinct apron
slots. They refuel at rollout so a low-fuel return survives the taxi. The
aircraft remain native entities with continuous positions and no parking
teleport.

Materials and all Fuel locations are hidden until scouted. Their world models
and map markers persist after sight leaves, with the last seen owner. The
player's initially hidden civilian sites no longer leak through the map.
The fog overlay was lightened to `fog_strength` 0.24, then tuned for issue #5 to 0.33 with a near-black colour (0.02/0.03/0.05); the default map brightness is 0.93. An F8 debug slider (FOG OF WAR) adjusts the darkness live, and a 3 px orange line (`edge_color`, `edge_width_px` in `fog_of_war.gdshader`) outlines the boundary of every area not under fog, and the UI tokens
now use green for Fuel and orange for Power. Numbered construction markers
use a 3.6 m Label3D instead of the previous 28.8 m text. The road mesh winding
was corrected so existing engineer-built roads and new corridors render above
the terrain.

Focused contracts: `command_post` 71, `strategic_map_symbols` 302,
`queued_builds` 28 and `ground_transport` 16 checks pass. A rendered
`ground_transport_routes` scenario asserts corridor and convoy presence and
captures both views under `validation/artifacts/ground_transport_routes/`.
The native integration runner includes road/rail delivery, road traversal,
air/ground fighter damage and distinct fighter parking assertions. The final
Release build and all four CTest programs pass. The broader Godot sweep is
not green: `skirmish_presentation` has resource/capture assertion failures;
`airfield_fighter_ferry` exceeded its 120 s deadline; `ground_transport` and
rendered `ground_transport_routes` report resources still in use at exit;
`vehicle_movement_smoke` fails hull-alignment and formation-arrival assertions.
The stale fighter hover description was corrected and `build_menu_hover` now
passes 81/81. This package remains open pending those failures.

## Radio callouts and command voice alerts (2026-11-10, issue #3)

Design and rules: [adr/0001-radio-callouts.md](adr/0001-radio-callouts.md).

Three alerts work end to end: `air.hostile_inbound`, `objective.fuel_depot_secured`
and `objective.fuel_depot_lost`. **No voice clips exist yet**, so every alert is
text with a short attention tone. Generating speech needs an ElevenLabs key.

- Native: `Simulation::record_radio_event` raises a report for one side. A depot
  capture tells the captor and the previous holder once each. Hostile aircraft in
  a side's own sky sight raise one report; another needs the sky clear of them for
  30 s (`kAirThreatClearTicks`). Reports are outside the state hash, the match
  record and the AI. Bridge: `get_radio_events(after_sequence, faction_id)`.
- Catalog: `godot/project/audio/radio/radio_catalog.json`. Eight further alerts
  are listed as parked with the system each one needs.
- `audio/radio/radio_announcer.gd` plays one line at a time on the new Voice bus,
  ordered by severity; critical cuts in. Repeats fold together, the queue holds
  six, and stale reports are dropped. `radio_location.gd` makes bearing, compass
  point, clock direction, range and grid words.
- UI: the line appears top-centre with category, severity and a location detail;
  clicking it moves the camera there. F3 opens the radio log (last eight). F2 has
  a Voice slider, spoken alerts on/off, alert text on/off and a switch per
  category, saved in `user://audio.cfg`.
- `tools/generate_radio_voice.py`: dry run by default (30 lines, 2,484 characters
  today). `--generate` renders missing lines to `audio/radio/audition/`, keeping
  clean masters in `audio/radio/masters/`; `--approve-all` moves them into the
  game and lists them in `voice_manifest.json`. Cached by text and settings hash,
  with a character budget, selection by event or voice set, and retry on rate
  limits. The key is read from `ELEVENLABS_API_KEY` or the ignored `.env`.

Evidence (Apple M1 Pro, macOS arm64, Release):

- Native: six `radio_*` tests in `tests/test_radio_events.cpp`. Removing the raid
  memory, the sight check or the previous-holder check fails 1, 1 and 4 of them.
- Godot: `python3 tools/validate.py radio_callouts` passes 86 checks headless and
  rendered, including a real capture, a real loss and an enemy paradrop flight
  seen by the player's units. Removing dedupe, preemption or the main-loop read
  fails 4, 4 and 5 checks. Screenshot:
  `validation/artifacts/radio_callouts/rendered/radio_feed.png`.
- Tool: `python3 -m unittest discover -s tools/tests` runs 10 generator tests with
  a fake transport (no network).

- Regression: CTest passes 4 programs (318 integration tests). The contract sweep
  passes except `skirmish_presentation`, which was already failing (HANDOFF
  "Known failing contracts"); `build_menu_hover` failed one hover-tick check in
  the sweep and passed twice alone. Three `test_validation.py` tool tests error on
  a missing `resolution` argument, unrelated to this work. Cost: PERFORMANCE.md.

Known gaps: no recorded speech and no human listening; ElevenLabs commercial
rights not yet checked; voice ids in the catalog are empty; the catalog cannot be
replaced by a mod without code; replays do not replay radio; the AI side's
reports are produced but nothing presents them.
