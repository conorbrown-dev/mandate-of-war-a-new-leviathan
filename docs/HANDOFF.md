# Handoff

Written 2026-11-08 so a new session, on this or another machine, can carry on
without the conversation that produced the work. Read this, then
`docs/STATUS.md` (how each system works and where it falls short). The code is
on branch `stabilise-gameplay-and-content`.

## What is being built

A fun-to-play skirmish on Broken Strait: seize the enemy command post in a
10 to 15 minute match, with forces that arrive by transport rather than being
built on the field. The plan below was proposed in order, and the user approved
Slice 1 and then Slice 2.

## The plan and where it stands

**Slice 1: a real battle that can end (done)**
1. Both sides on one landmass about 7 km apart: done.
2. Faster ground units: done. Speeds are now realistic and factions rebalanced.
3. AI objectives and waves: done.
4. Win and lose with an end panel: done.

**Slice 2: forces arrive (done 2026-11-08)**
5. The engineer builds structures and earthworks only: done.
6. Strategic Command (G) orders forces from the unit data: done for ground and
   air. No sea, rail or paratroop cards.
7. The AI receives forces the same way: done. Its Field Engineer builds its own
   airfield (`AIManager::build_airfield`); the free landing zone is gone.
8. Paratroop drops onto scouted ground: done for the player (PARADROP in
   Strategic Command) and the AI. The AI drops a scout near a currently visible
   forward contact, subject to the same cost, sight and transport rules and a
   90-second decision interval. Cargo aircraft are drawn and interceptable.

**Slice 3: decisions with teeth (done 2026-11-08, not hand-played)**
9. Fuel: done. Ground vehicles burn fuel from their side's store per km driven
   (`fuel_per_km` in the unit data, on in Broken Strait) and halt when it runs
   out. The AI orders shipments when low, keeps their price in hand, and goes
   for depots before Materials sites when short.
10. Interception and site capture: done. A fuel shipment flies in on a real,
    unarmed cargo aircraft from the west map edge and is lost if shot down.
    Materials sites change hands when a unit arrives, not on the order.
    Paratroop drops fly in on the same cargo aircraft and can be shot down too.
    Weapons engage air or ground as their real systems do (see STATUS.md "Air
    and ground targets").

**Slice 4: making it feel good (implementation complete 2026-11-09; human play/listening acceptance open)**
11. Sound: implemented. 382 original licensed clips plus two reproducible edited
    firearm cues carry source/license/checksum records. UI has four voices; combat
    has 12 positional voices with distinct cannon, artillery, missile, rifle and
    burst cues. Moving vehicles and airborne aircraft use separate loops, capped
    at eight nearest audible units within 1.8 km. Hidden enemies stay silent;
    sight loss/removal, stopping, distance and match end release engine voices.
    F2 opens persistent Master/UI/Combat/Engines controls and player-accessible
    sound credits before, during or after a match. Zero mutes; Esc closes.
    Engine attribution includes Nayckron/qubodup, source, CC BY 3.0 and runtime
    pitch/gain changes. Aircraft/heavy-weapon layers are stylized substitutes.
12. Combat effects: implemented first pass. Native shot, hit and death events
    drive muzzle flashes/firing streaks, impacts, explosions and fading smoke.
    Visibility-filtered history and current-sight checks protect fog of war.
    Persistent wreckage and full-flight projectile trails remain optional later
    refinements, not unfinished Slice 4 requirements.
13. Briefing, objective panel, unit names: implemented and inspected on macOS
    at 1280x720 and 1920x1080. Startup explains airfield, Strategic Command,
    scouting and fuel; HUD/hover names come from native prototypes. The shared
    v2 UI covers briefing, HUD, reinforcement menu, controls and after-action.
14. Magnified view: implemented, sharing World3D/fog/visibility. Defaults to 8x,
    [ / ] step to 16x/32x, M toggles. Holds/dims over HUD or off-map. It now seeds
    its first view from the starting camera focus, avoiding an empty world-origin
    view before the first battlefield hover. Geometry, hold/dim, toggles and real
    match initialization are asserted; desktop screenshots inspected.

Validation commands and evidence for the Slice 4 closeout are in STATUS.md.
The repeatable listening reel measures levels/clipping; it is not a claim of
human audition. A complete match has automated victory/defeat coverage. Human
match feel, physical input and listening acceptance remain to be reviewed.

Controls updated 2026-11-09: + / = / numpad + increases game speed; - /
numpad - decreases it, in 25% steps between 25% and 175%. Multiplayer stays at
100%. Slash (or numpad divide) cycles camera presets from 24 m through full-map
26,000 m and wraps. These are one step per press, with no key-repeat racing.
Wheel and MacBook pinch retain smooth cursor-anchored zoom. F1 lists the controls
and the top-left HUD shows speed. Native speed 27 headless / 28 rendered checks,
camera input 48, F1 sheet 41 and strategic zoom nine pass. Physical MacBook
trackpad delivery and hand-play feel remain open.


## Feature requests and bugs (from the user, 2026-11-09)

Implementation and automated verification as of 2026-11-09:

- Done (Agent A): UI hover cues and build/Strategic Command descriptions; the
  `build_menu_hover` contract passes 81 checks. Done: Shift+click queues engineer
  structure orders with numbered markers; `queued_builds` passes 24 checks.
  Both fail with their fix removed. See STATUS.md "Build menu hover and queued
  engineer builds". Open: startup-overlay buttons do not tick; no cancel-queue key.
- Map-scale unit icons are APP-6 symbols (affiliation frames, land/air/sea,
  function icons per unit; STATUS.md "APP-6 map symbols and resource markers")
  and Materials/fuel sites have screen-space markers at every zoom;
  `strategic_map_symbols` passes 299 checks. The command post can be selected
  by click (not box), shows name, integrity, role and inbound forces/fuel, and
  offers Strategic Command (G) and Request Fuel Shipment (J); it takes no
  orders. The enemy post is hover-only once discovered. It is now a 56 x 42 m
  primitive-built forward command post (STATUS.md "Command post");
  `command_post` passes 69 checks. All four native CTest programs pass.
- The user decided that drinkable water belongs in Materials. Water towers are
  now municipal scenery and navigation obstacles, with no separate resource bar.
  A functional tower upgrade has not been assigned. The user chose **hold key
  sites** as the additional victory condition: Broken Strait ends when either
  command post falls or a side holds two of three named Material/Fuel sites for
  120 consecutive seconds. Native tests verify interruption, retake, defeat and
  post-loss precedence; `key_site_victory` passes 14 Godot checks.
- Civilian residential blocks, office towers, a weathered warehouse and water
  towers now use imported models from Poly Pizza. Model pages, license terms,
  checksums and the CC BY warehouse credit are in [BUILDING_ASSETS.md](BUILDING_ASSETS.md).
  The user also suggested Sketchfab; its current downloadable architecture
  gallery was reviewed, but no model was adopted because the relevant recent
  industrial study was 99.3k triangles for repeated scenery. The rendered
  `civilian_building_models` scenario asserts all four model types and captures
  the neighborhood plus close views. Plain utility boxes remain in the mix.
- Units can occupy exactly the same position: fixed (Agent E). Ground hull
  separation now resolves overlapping vehicles deterministically, including
  crowded spawns, same-point orders and tight formations; its native tests pass
  and fail with the fix removed. See "Ground-unit separation" in STATUS.md. The new collision behavior
  changed equal-budget faction outcomes, so Elite MBT health was first tuned from
  500 to 460. On 2026-11-10 crowded vehicles were changed to drive out of the
  way (the user's choice) instead of sliding, which needs no such compensation:
  Elite MBT health is back at 500 and the unchanged 12% balance gate passes for
  all three pairings (+0.063, -0.073, +0.086). The balance harness now leaves a
  unit that is making room alone.
- The native 4,000-unit active-combat fixture improved from 33.197 to 9.001 ms
  mean tick after single-player snapshot gating, combat observer caching and
  bounded crowded-cell work. See [PERFORMANCE.md](PERFORMANCE.md). This does
  not establish rendered frame rate or remove possible first-contact shader
  hitches. Human play at battle onset remains an acceptance check. Evidence from
  2026-11-10 (rendered, headless run on an M1 Pro): with the AI on, a fresh match
  showed 250 to 320 ms frames about 20 to 30 s in (frames 2442, 3135 and 3830 of
  a forest build), when the enemy's forces began to land; with the AI off the same
  window had only two slow frames. So the first-contact hitch is real and is
  presentation-side (a new unit's model and effects loading for the first time),
  not native. Fix idea: preload the unit models and combat effect materials and
  shaders during the loading screen.
- AI paratroop follow-up: completed. The AI now issues recorded scout-bike
  paradrop commands to watched flank ground near a forward contact, using the
  same Materials cost, visibility check and cargo pipeline as the player. A
  native scenario asserts no blind or unpaid order, one accepted inbound drop
  and a cooldown; removing the AI decision fails the new assertion. Release build,
  all four CTest programs (20 unit, 21 snapshot, 305 integration plus balance),
  `match_flow` (80 checks) and `native_skirmish_match_to_result` (19) pass.
- Resource-map verification (2026-11-09 baseline, superseded by the scouting
  change below): Broken Strait marked all four Materials sites and
  four known civilian fuel terminals at every zoom. Its two enemy military fuel
  depots appear only after scouting, without revealing their owner early;
  `strategic_map_symbols` passes 299 checks. The separate native skirmish had
  two resource nodes but no drawn markers; this is fixed. Its Materials and Fuel
  badges follow the native nodes, and the route contract now passes 23 checks.
  Removing that draw call fails four checks. Water is included in Materials;
  water towers are scenery, not separate resource sites. A rendered native-map
  screenshot is in `validation/artifacts/resource_map/rendered/`.

The complete Release build passes. CTest discovers/passes four programs; current
direct runner summaries are 20 unit, 21 portable-snapshot, 305 integration, plus
`BALANCE_OK`. The earlier Godot contract sweep found two stale expectations
(the HUD dropped command-post wording when showing site progress, and the legacy
native route now wins rather than draws). Both were corrected and their focused
contracts pass: `match_flow` 80/80, `native_skirmish_match_to_result` 19/19.
That complete Godot contract sweep passed all 42 scenarios (2026-11-09).
After the AI paradrop change, the relevant `match_flow` and native skirmish
contracts were rerun and passed; the full sweep was not repeated.

## User-requested MoW transport and bug package (2026-11-10)

- ROAD and RAIL now work as ground-force delivery choices in Strategic Command.
  Broken Strait has one authored corridor of each type per faction. A recorded
  native order pays Materials, observes inbound/unit caps, travels at 25 m/s
  by road or 45 m/s by rail after build time, and unloads at its own terminal
  without an airfield. Existing roads and the authored road corridor lower
  ground traversal cost. A corrected mesh winding makes roads visible; rail
  ballast, ties and two rails, terminal labels, and moving truck/train
  silhouettes show the routes. Enemy corridors are hidden until scouted and
  then remembered. Native road/rail and Godot `ground_transport` contracts
  assert ordering, costs, progress, arrival, and two distinct modes. The
  rendered `ground_transport_routes` scenario has inspected corridor/convoy
  screenshots. Routes are fixed infrastructure; visual convoys cannot yet be
  attacked or blocked, engineer rail placement is absent, and the AI still
  defaults to air delivery.
- Clicking the player's command post now opens Strategic Command immediately.
  Fighters can damage both ground and airborne targets. Ferried fighters
  roll out on the main runway and taxi continuously at 8 m/s to distinct
  apron slots, with no parking jump. Native combat/landing tests and focused
  `command_post` checks cover these behaviors.
- All Materials and Fuel sites, including civilian sites, now stay hidden
  until scouted; their models, markers and last-seen owners persist afterward.
  The strategic map contract includes both discovery and memory checks.
  Fog now tints unwatched terrain by 24% rather than 62%. Fuel uses green and
  Power orange. Queued-build labels were reduced from 28.8 m to 3.6 m, and
  the `queued_builds` fixture now uses clear sites and waits for native jobs.
  Screenshots were inspected on macOS; physical play remains open.

Validation for this package: Release build passes. Focused Godot contracts
`command_post` 71/71, `strategic_map_symbols` 302/302, `queued_builds` 28/28,
`ground_transport` 16/16, and rendered `ground_transport_routes` 7/7 pass.
Release build and four CTest programs pass, but the full Godot sweep is not
green. See STATUS.md for the failing scenarios; keep this package open until
they are resolved or triaged with evidence.

## Radio callouts (issue #3, 2026-11-10)

Slices A to C of the issue are implemented: native radio reports, a JSON alert
catalog, the announcer with subtitles and an F3 log, and the offline ElevenLabs
generator. Three alerts are live (hostile aircraft, fuel depot secured, fuel depot
lost). Ten Elite Command clips now ship; other factions remain text-only.
The user confirmed playback on macOS. WAVs load directly without an import pass
and preserve the original audio without processing. See STATUS.md "Radio callouts"
and `docs/adr/0001-radio-callouts.md`. To add speech: put voice ids in the catalog
(or `ELEVENLABS_VOICE_<SET>`), set `ELEVENLABS_API_KEY`, run
`tools/generate_radio_voice.py --generate`, listen, then `--approve-all`. Check
the plan's commercial rights first and add the voices to the F2 credits. Slices D
(more alerts) and E (mod loading, replay) are open.

## Suggested next steps

1. Play a match by hand against the AI. Every slice so far is proven only by
   tests; the fuel numbers in particular (`fuel_per_km`, 2,000 per shipment for
   300 Materials, the AI's 1,500 low-fuel mark) are first guesses.
2. Ground balance was re-tuned after SAMs stopped fighting tanks (Mass Assault
   Vehicle damage 59.5, HP 102; all pairings inside tolerance). Check it in play.
3. Decide whether Elite Precision needs a second airdroppable unit (content
   call for the user). AI paradrops are implemented and assertion-covered;
   their tactical value still needs hand play.
4. Slice 4 implementation is complete. Listen to the recorded mix and play a
   match to tune feedback; F2 adjusts channel levels. Windows/Linux UI and
   physical MacBook input checks remain. Do not invent a Slice 5 without an
   agreed work package; this handoff currently defines only four slices.

## Standing rules

Design (the user stated these, and they override convenience):
- Research happens outside the match. Never offer in-match research.
- Forces arrive by air, road, rail, paratroop or sea. Do not build units on the
  battlefield.
- Fuel is shipped in or captured from civilians and enemy depots. Nothing on the
  map produces it.
- A match never starts with knowledge of the enemy, and terrain blocks sight.
- The AI has every limitation a player has: same fog, same costs, same command
  API, no reads of hidden state, no resource or vision bonuses as difficulty.
- AI difficulty comes from play styles (turtle, rush or swarm, tech). Later it may
  be trained on real matches and on a player's habits, never by cheating. Keep
  style data in JSON and keep matches recordable.

Working:
- Do not commit or push unless asked. Preserve the user's uncommitted changes.
- GitHub CI is not a priority. Do not raise it as an open item.
- The simulation is native C++ and authoritative. Godot only presents.
- A gameplay change is not done until a scenario or test asserts its behavior.
  Check that a new check fails when the fix is removed. Never widen a tolerance
  or skip an assertion to pass.
- For gameplay work, report: Implementation, Validation scenarios run, PASS/FAIL,
  State assertions, Visual artifacts, Performance evidence, Known limitations,
  Files changed.

## Setting up on a new machine

macOS (Apple silicon, set up 2026-11-08; headless validation only so far):
- `brew install cmake`. Godot 4.7.2's macOS build goes in
  `~/mow-tools/godot/Godot.app` (validate.py finds it there).
- `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j10`.
  The build copies `librts_gdextension.dylib` and `librts_simulation.dylib` into
  `godot/project/bin`; `rts.gdextension` has `macos` entries for them.
- Once: `Godot.app/Contents/MacOS/Godot --headless --path godot/project --import`.
- `ctest --test-dir build --output-on-failure`, then `python3 tools/validate.py all`.
  validate.py points `HOME` at the artifact folder so `user://` stays isolated.
- Play with `./tools/run_game`. It builds only `rts_gdextension`, defaulting to
  eight parallel jobs; set `CMAKE_BUILD_PARALLEL_LEVEL=4` for a smaller build, or
  pass `--no-build` to use existing binaries. Never use bare `-j` with Make: it
  means unlimited jobs and caused about 1,900 compiler processes on this Mac.
- Rendered combat screenshots now pass on the Mac's Compatibility renderer;
  full-match hand play and representative GPU frame rates remain unverified.

Windows (the verified platform):
- Install Visual Studio Build Tools with the C++ workload, CMake, Python 3 and
  Godot 4.7.2. Put Godot's Windows build in `~/mow-tools/godot/`, or set
  `GODOT_BIN` to `Godot_v4.7.2-stable_win64_console.exe`.
- Build: `tools/build_windows.ps1` (a plain shell needs `vcvars64.bat` first, then
  `cmake --build build\windows`). The build copies the extension DLL into the
  Godot project.
- C++ tests: `ctest --test-dir build/windows --output-on-failure` (4 programs,
  including `faction_balance`).
- Godot checks: `python tools/validate.py contracts`, then
  `python tools/validate.py <scenario>` or `all`. Without a GPU, screenshots are
  skipped.
- Play: `Play vs AI.cmd`, or `tools/play_vs_ai.ps1 -Style swarm -Difficulty hard`.
  F1 shows the hotkeys.
- A GPU is needed to bake tree impostors and to judge visuals or frame rate. WSL
  reaches only a software renderer.

Rebuilding the forest (only when its source changes):
`godot --headless --path godot/project --script res://tools/build_near_trees.gd`,
then, with a GPU and no `--headless`,
`--script res://tools/bake_tree_impostors.gd`.

## Gotchas learned

- After any Godot run, `git restore godot/project/.editor_native`. Godot rewrites
  it and the `.import` files.
- `.uid` files are tracked. Commit the one a new script creates.
- Windows has a 1 MiB thread stack, so keep large buffers on the heap, and use
  `read_environment`, not `std::getenv`.
- `Simulation::unit_count` is a cache refreshed in the economy phase. Let a tick
  pass before reading it in a test.
- `issue_build_commands` is applied on the next tick. `queue_faction_unit` takes
  effect and answers at once, so use it when the UI needs to know if an order was
  accepted.
- `queue_free()` in a frame leaves old nodes until the frame ends. Await a frame
  before counting children.
- Headless Godot uses a dummy renderer that keeps no MultiMesh data, so checks of
  instance buffers need a GPU or a meta value.
- Unit models from donor packs may be several vehicles; `isolate` in
  `visuals/visual_definitions.json` cuts one out. `test_unit_motion.gd` checks
  every visual faces forward.
- Only the Broken Strait scenario turns the transport rule on
  (`"forces_by_transport": true`). Native tests run with it off.

## Known limitations

- Slice 4 audio/effects/UI are implemented; human listening and full-match
  hand play remain. Persistent wreckage/trails and authentic aircraft/heavy-weapon
  recordings are optional later refinements.
- Wheeled vehicles cannot reach a point inside their turning circle (an engineer
  cannot reach a spot 14 m directly to its side).
- If an airfield is destroyed, forces already in the air wait for a landing site
  instead of being refunded.
- The old single-package `reinforcement_delivery_*` native calls remain and only
  anchor the parked F-15C.
- Close-up trees are EZ-Tree models; the leaf textures come from its npm package
  because GitHub's media host was unreachable when they were fetched.
- The F-14's nose was checked by eye only.
- Not played by hand against the AI.

## Not in the repository

These lived on the previous machine and are not needed to carry on:
the downloaded Poly Haven tree sources (the forest now comes from EZ-Tree), the
scratch scripts used to run validation, ffmpeg, the demo videos under
`validation/artifacts/`, and a Supreme Commander: Forged Alliance install that
was read once for unit speed and turn-rate reference values.


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


Real fighter landing correction (2026-11-08): user play invalidated the earlier
visual-only landing fix. The actual native ferry now enters 4.5 km north/south,
curves gradually onto a 1.8 km final, descends in forward flight onto the main
runway at site X-55 m, then rolls to rest. Godot reads native altitude/pitch/bank
instead of dropping a stopped plane vertically. Local SupCom fighter blueprints
were inspected for forward-airspeed/turn/bank principles; details in STATUS.md.
The real-fighter scenario now runs through landing (21 headless / 27 rendered
checks); restoring the old altitude renderer fails two checks. Release build and
four CTest programs pass (20/21/291 runner totals); boundary 73, unit motion 58,
match flow 80 and ground-delivery smoke pass. Final visual evidence:
validation/artifacts/airfield_fighter_ferry/20261109T011021.921016Z/.
Interactive play review remains; no scale/FPS claim or general combat/takeoff rewrite.


Overlay/radar follow-up (2026-11-08): aircraft icons and horizontal hex envelopes
follow the drawn flight pose. Rings now use native weapon/view/intel distances
rather than universal 10/15/22 m placeholders; example MBT 115/150/450 m. The user
approved distinct gameplay intel coverage: terrain-aware anonymous 100 m contact
cells, shared with AI, without model revelation, target identity, fog or firing
permission. Full behavior/content/privacy details are in STATUS.md.
Release build and four CTest programs pass (20/21/294 runner totals); overlay,
ferry, boundary, skirmish and match-flow regressions pass. Rendered evidence:
validation/artifacts/airfield_fighter_ferry/20261109T012702.024404Z/ and
validation/artifacts/unit_range_overlays/rendered/radar_ranges.png.
PERFORMANCE.md records the isolated 1,000-moving-unit native intel comparison.
Hand-play sensor balance and dense overlay readability; no FPS claim.


Game-speed follow-up (2026-11-09): native scheduling now supports 25–175% in 25%
keyboard steps, default 100%, locked to normal in multiplayer. +/- controls speed;
slash cycles zoom presets. Wheel/pinch stay smooth. Native fixed ticks remain
50 ms; fractional scaled time survives long frames. HUD and F1 show the controls.
Release build and four CTest programs pass (20/21/296 runner totals); speed
27 headless / 28 rendered, camera 48, hotkeys 41, boundary 78, strategic zoom nine
checks pass. Evidence: validation/artifacts/game_speed/rendered/game_speed_175.png.
See STATUS.md for the native/Godot multiplayer guards and negative-control proof.

## Known failing contracts (2026-11-10)

- `airfield_fighter_ferry`: fails `air.fighter_no_teleport_pivot_or_hover` and
  `air.fighter_parks_on_main_runway` and exceeds the 120 s default deadline
  (about 118 s without the deadline hit, ~3,300 ticks). Cause: ferried
  fighters now roll out and then taxi to an apron slot (the taxi change in
  `Simulation`), but the scenario still expects the fighter to stop on the
  runway centre with no heading change. The scenario needs to be updated for
  the taxi path by whoever owns that change; its tolerances were not loosened.
- `skirmish_presentation`: expects 8 resource sites at start; resource sites
  are now hidden until scouted (other session's scouting change).
