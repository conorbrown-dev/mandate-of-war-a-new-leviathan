# Current state

Read [HANDOFF.md](HANDOFF.md), then [STATUS.md](STATUS.md) for authoritative scope.
Slice 3 is implemented and assertion-covered, but has not been hand-played.
Slice 4 implementation is complete (2026-11-09): distinct weapon cues, bounded
moving-vehicle/aircraft loops, persistent F2 audio controls and sound credits,
combat feedback, briefing/native names and a magnifier seeded at match start.
The 2026-11-10 user bug package adds native road/rail ground deliveries on
Broken Strait's fixed corridors, immediate Strategic Command from the command
post, dual-domain fighter attacks, distinct taxi parking, scouting-gated
resource memory, lighter fog, swapped Fuel/Power colors, smaller queued-build
labels and visible road meshes. Transport, combat, parking and scouting have
state assertions; a rendered road/rail scenario has inspected screenshots.
Convoys are presently visual representations of timed native deliveries, not
attackable entities. See HANDOFF.md and STATUS.md for exact validation.
macOS desktop screenshots and automated match flow are verified. Human listening,
full-match hand play, physical MacBook input and Windows/Linux visual review remain.
The historical entries below record earlier intermediate states; the final
Slice 4 closeout in STATUS.md supersedes their audio TODOs.

2026-11-08 validation: Release build; four CTest programs pass; direct runners:
20 unit, 21 portable-snapshot, 290 integration checks. Focused Godot contracts:
slice4_briefing 15/15, gdextension_boundary 70/70. The name regression control
fails five checks when the fix is removed. No new visual/performance claims.

Sound sources researched 2026-11-08: see [AUDIO_SOURCES.md](AUDIO_SOURCES.md).
382 licensed source clips have now been acquired and imported with provenance and
checksums. Strategic Command UI playback is connected. `audio_library` passes 1,546
assertions (regression control fails with the open hook removed); Release build and
four CTest programs pass. Combat now uses native visible events for shot/hit/destruction feedback. Engine
loops, listening/mixing and final engine credits remain open.

Combat feedback validation: 65 headless Godot checks; 62 rendered checks with
inspected flash/smoke screenshots in `validation/artifacts/combat_effects/rendered/`.
Native tests cover event order, visibility at emission, ground/sky terrain sight,
viewer-private cursors, bounded history, reset and gameplay hash equivalence.
Bypassing the current-sight guard fails nine presentation checks. Release build,
four CTest programs and audio/boundary/match-flow regressions pass. No scale/FPS claim.

Launcher fix (2026-11-08): `tools/run_game` now defaults to eight build jobs,
respects a positive `CMAKE_BUILD_PARALLEL_LEVEL`, and builds only the game extension.
The prior bare `-j` launched 1,917 compiler processes and thrashed memory on a
10-core/32 GB Mac. That build was interrupted before rebuilding with eight jobs.
Argument checks cover default/override/no-build/invalid values.

Recovery build and the repaired launcher's headless startup both passed; extension
handshake and heightmap load verified. No gameplay changes in the launcher fix.

Current controls (2026-11-09): +/- (also = and numpad aliases) adjusts native
game speed in 25% steps, clamped to 25–175%, offline only. Slash/numpad divide
cycles seven cursor-anchored zoom presets; wheel/pinch remain unchanged. F1 and
HUD reflect the remap. Game-speed checks 27 headless / 28 rendered, camera input
48, F1 sheet 41 and strategic zoom nine pass. Physical trackpad hand check remains.


Camera follow-up Release build and four CTest programs pass (runner counts
20 unit, 21 portable-snapshot, 290 integration).


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

Final overlay verification: 39 headless / 40 rendered assertions, including hidden
enemy range rings and stationary terrain-sampler invalidation; unit-root 18 passes.


Game-speed follow-up (2026-11-09): native scheduling now supports 25–175% in 25%
keyboard steps, default 100%, locked to normal in multiplayer. +/- controls speed;
slash cycles zoom presets. Wheel/pinch stay smooth. Native fixed ticks remain
50 ms; fractional scaled time survives long frames. HUD and F1 show the controls.
Release build and four CTest programs pass (20/21/296 runner totals); speed
27 headless / 28 rendered, camera 48, hotkeys 41, boundary 78, strategic zoom nine
checks pass. Evidence: validation/artifacts/game_speed/rendered/game_speed_175.png.
See STATUS.md for the native/Godot multiplayer guards and negative-control proof.

2026-11-09 UI design system v2: one theme, type system and panel look across the
briefing, tactical HUD, build catalog, Strategic Command, F1 sheet, after-action
report and magnifier (see [ui-design-system.md](ui-design-system.md) and the
STATUS.md entry). Eleven UI contracts and the showcase pass; rendered macOS
screenshots only. Windows/Linux font fallback and hand play are unchecked.


Slice 4 closeout evidence (2026-11-09): Release build and all four CTest programs
pass (20/21/296 direct runner totals). All 37 Godot contracts pass; focused audio
30 headless/31 rendered, library 1,554, magnifier 14, briefing 15, combat 66 headless/63 rendered and
complete match flow 80. Real fighter ferry passes 29 rendered assertions.
`validation/artifacts/slice4_ui/` holds inspected 1280x720 and 1920x1080 screens.
The measured listening reel is `validation/artifacts/slice4_audio/listening/audio_mix.wav`:
23.50 seconds, peak -17.90 dBFS, zero clipped samples; 16 audio measurements pass.
Removing engine visibility filters fails its hidden-enemy regression control.
The engine allocator keeps eight voices for 1,000 candidates; see PERFORMANCE.md
for the limited microbenchmark scope. Human listening and match feel are open;
all Slice 4 code tasks are implemented. See the final STATUS.md entry for commands,
provenance and limitations. No commit was made.


2026-11-09 feature/bug follow-up: hover audio/descriptions, engineer build queues,
NATO/resource markers and selectable authored command posts are implemented and
covered by focused Godot contracts. Broken Strait now offers continuous two-of-three
key-site hold for 120 seconds alongside command-post victory; 14 focused Godot
checks and two native state tests pass. Drinking water is part of Materials.
Poly Pizza residential, office, warehouse and municipal water-tower models replace
most civilian placeholder boxes; source/license details are in BUILDING_ASSETS.md.
The rendered civilian scenario passes and captures the neighborhood and four close
views under validation/artifacts/civilian_building_models/20261110T024520.064664Z/.
Ground separation is active and deterministic; Elite MBT health is now 460 to
keep the unchanged equal-budget balance gate passing. Native combat mean tick
was 9.001 ms for 4,000 active units after the slowdown fixes, versus 33.197 ms
before; see PERFORMANCE.md for workload limits. Release build and all four CTest
programs pass (20/21/304 direct native tests plus balance). The complete Godot
contract sweep found two outdated expectations and their focused reruns pass;
the final complete sweep passes all 42 contracts. Human play and rendered
battle-onset frame pacing remain unverified.

2026-11-09 AI paradrop follow-up: the AI uses the existing recorded command to
drop its scout bike near a visible forward contact, with the player's sight,
cost, queue and cargo rules and a 90-second decision cooldown. A native state
scenario proves no blind or unpaid order, an accepted inbound drop and cooldown; removing
the new decision fails it. Release build and four CTest programs pass
(20/21/305 direct native tests plus balance). Godot `match_flow` 80 and native
skirmish route 19 pass. No new visual or performance claim; hand-play tactical
quality remains open.

2026-11-09 resource-map check: the main Broken Strait map marks four Materials
sites and four known civilian fuel terminals; two enemy military depots become
marked on discovery, without leaking hidden ownership. `strategic_map_symbols`
passes 299 checks. The separate native 2D skirmish was missing its two resource
markers; it now draws native METAL/ENERGY nodes as Materials/Fuel. The route
contract passes 23 checks; the negative control fails four when the draw call
is removed. A rendered 1280x720 screenshot is in
`validation/artifacts/resource_map/rendered/native_resource_markers.png`.
Release build and four CTest programs pass. Water remains included in Materials;
water towers are scenery rather than a separate resource.


2026-11-10 voice playback fix: generated voice WAVs can exist in the manifest
without Godot import metadata. The announcer now loads WAV source files directly
with AudioStreamWAV.load_from_file, preserving PCM and bypassing the missing
import. Other formats retain normal ResourceLoader loading. No EQ or treatment
is added. `radio_voice_source` creates an unimported WAV and manifest and checks
speech playback, no fallback blip, unchanged PCM, and zero Voice-bus effects.
The regression fails against the old announcer and passes with the fix.
Release build passes. CTest discovers/passes four programs (20 unit, 21 snapshot,
318 integration, BALANCE_OK). Focused radio_voice_source scenario passes four
checks outside the sandbox; Godot reports two ObjectDB leaks at shutdown.
Human playback acceptance: user confirmed speech works in this worktree.
The shipped pack contains ten Elite Command clips; other faction voices remain
text-only. Generator output preserves original PCM without processing.
