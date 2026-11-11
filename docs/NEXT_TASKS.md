# Next tasks

Issue #10's overlap guard and red ghost/refusal feedback are implemented.
Hand-check the preview and sound over a queued and completed structure in a
match. The focused Godot contract passes 34/34 checks.
Airfield ghost alignment and command-post exclusion now pass focused native and
Godot checks. Hand-check the updated airfield boundary at several zoom levels.

Follow the slice plan in [HANDOFF.md](HANDOFF.md).

Power grid (issue #4, 2026-11-10) is implemented and assertion-backed: stations,
steel and wooden lines, capture, cutting and engineer repair, with 12 native
tests and a 42-check Godot contract. Next, in the order the user chooses: give
a power deficit an effect, teach the AI to cut, take and repair lines under the
player's limits, let engineers lay new lines, and let weapons damage lines.
Hand-play: cut and repair the supply-road line, and judge the pylons, wires and
sparks at tactical and strategic zoom.

Resource-map verification (2026-11-09) is complete. The 2026-11-10 scouting
change now hides every Materials/Fuel site until discovered, then remembers
its marker and last-seen owner. The separate
native skirmish now marks its two METAL/ENERGY nodes as Materials/Fuel. Contracts
pass (302 strategic after the scouting change, 23 native) and a rendered native
screenshot was inspected.
Water is part of Materials, so no independent water marker is planned.

The 2026-11-10 transport/bug package is implemented and assertion-backed:
fixed road/rail corridors carry paid ground deliveries to distinct terminals;
command-post click, fighter domains, separate apron slots, construction text,
resource memory, Fuel/Power colors and lighter fog have focused checks. Next
transport extensions are engineer-built rail links, destructible/interdictable
convoys and AI route choice. These are not part of the fixed-corridor first pass.
Hand-play a match to judge route visibility, truck/train silhouettes, fighter
taxi flow and fog brightness at tactical and strategic zoom.

2026-11-09 feature/bug pass: new requested code is implemented and focused
scenarios pass. The final 42-scenario Godot contract sweep passes. Hand-play a match to
judge civilian scenery density, key-site hold pacing, water-tower appearance,
and battle-onset frame pacing. Water remains part of Materials; the placed towers
are scenery. If towers should perform a gameplay action, define that action
before changing the economy. Four civilian model licenses and provenance are in
[BUILDING_ASSETS.md](BUILDING_ASSETS.md); the CC BY warehouse credit is also in F2.

1. Slice 4 implementation is complete. Human acceptance: play Broken Strait
   against the AI and listen to the mix in context; tune fuel, balance and
   feedback from concrete play evidence. F2 exposes Master/UI/Combat/Engines
   levels and sound credits. Automated full-match victory/defeat checks pass.
2. Review v2 UI/font fallbacks on Windows and Linux, plus physical MacBook
   pinch, slash cycling and +/- speed. macOS 1280x720/1920x1080 screens have been
   inspected, including briefing, HUD, magnifier, F1 and audio/credits.
3. Agree the next work package. HANDOFF.md defines no Slice 5. Extra
   airdroppable content, persistent wreckage and projectile trails are possible
   follow-ups, not automatically authorized milestones. AI paradrops are done.

Slice 3 is implemented; do not reopen it without failing evidence. AI paradrops
now pass a native behavior scenario; hand-play their tactics. A second Elite
airdroppable unit remains a content decision.

Audio provenance: [AUDIO_SOURCES.md](AUDIO_SOURCES.md). All 382 originals are
retained; two edited firearm cues are reproducible with tools/prepare_audio.py.
Distinct weapon cues and engine loops are connected. F2 includes engine attribution.
The listening reel and measured levels are under
validation/artifacts/slice4_audio/listening/. Aircraft/heavy-weapon effects are
stylized, not authentic recordings; human listening remains pending.

Launch with `./tools/run_game` (eight build jobs by default). For less parallelism,
use `CMAKE_BUILD_PARALLEL_LEVEL=4 ./tools/run_game`; `--no-build` reuses binaries.

Hand-check MacBook pinch zoom, slash zoom cycling and +/- game speed in a real
match. Speed uses one 25% step per press (25–175%) and is locked to 100% for
multiplayer. Routing, clamps, cursor anchoring, text entry and pacing are covered.


Landing follow-up verified: curved delivery approach and main-runway touchdown
pass automated path assertions and rendered delivery scenario. Hand-play the
approach at tactical zoom; full combat-aircraft flight dynamics remain separate.


Construction skeleton follow-up (2026-11-08): shared placement/construction meshes
are implemented and pass 37 focused checks. Hand-check their translucent cyan
appearance in play. The historical native aircraft rollout failure was resolved by the later real
fighter correction below; current CTest passes all four programs.


Real fighter follow-up: the previous delivery-visual landing claim was invalidated
by user play. Native fighter ferry/return guidance and drawn altitude are now
corrected and verified through rollout (see STATUS.md and the 20261109T011021.921016Z
ferry artifacts). Hand-play an ordered fighter from ingress to the large runway,
then a low-fuel return. Keep the single runway; no X-airfield change was needed.


Overlay/radar follow-up implemented and assertion-backed: aircraft icon/range
planes follow flight altitude; all supported rings use native gameplay metres.
Hand-play radar/contact balance and dense-battle readability. The user approved
separate intel sensing; preserve anonymous contacts and visual-only targeting.
See STATUS.md for behavior and PERFORMANCE.md for native-only timing evidence.

Overlay implementation checks are complete (39 headless / 40 rendered); remaining
work is hand-play tuning, not missing aircraft anchoring or placeholder radii.


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
Main integration: Release build, radio_voice_source (4), radio_callouts (88)
and generator tests (11) pass. Human listening acceptance is confirmed.
