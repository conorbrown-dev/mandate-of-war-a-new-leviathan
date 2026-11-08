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

**Slice 2: forces arrive (mostly done)**
5. The engineer builds structures and earthworks only: done.
6. Strategic Command (G) orders forces from the unit data: done for ground and
   air. No sea, rail or paratroop cards.
7. The AI receives forces the same way: done, with a caveat. The AI has no
   airfield and cannot build one yet, so it is given a landing zone beside its
   command post. That is a free advantage until it builds its own.
8. Paratroop drops onto scouted ground: **not started**.

**Slice 3: decisions with teeth (partly done)**
9. Fuel: capturing depots and shipping fuel in work. Ground vehicles burn no
   fuel (only aircraft and earthworks do) and the AI neither orders nor spends
   fuel. Open.
10. Intercepting shipments, and Materials sites capturing on arrival: **not
    started**. Shipments come by air only and cannot be intercepted.

**Slice 4: making it feel good (mostly open)**
11. Sound: **not started**. There are no audio files or players.
12. Combat effects: **not started**. Units just vanish when destroyed. Map
    markers exist only for remembered enemy positions.
13. Briefing, objective panel, unit names: partial. There is an objective
    banner. The briefing is only text in the scenario file, and a unit with no
    name shows as "COMBAT UNIT".

## Suggested next steps

1. Let the AI build its own airfield, then remove its free landing zone
   (`_apply_force_arrival_rules` in `main.gd`).
2. Paratroop drops (item 8): a transport flies in from off the map to a drop
   point the player has scouted (use the vision map), and the troops land
   spread out. They must pay and obey the unit cap like any delivery. Start from
   `Simulation::order_delivery` and `deliver_inbound_forces`.
3. Re-measure the AI timings in `STATUS.md` "The battle". They predate realistic
   speeds and forces by transport. A match run on 2026-11-08 showed the first wave
   at 33 s, first contact at about 12.4 minutes and 17 waves, against 7 waves and
   about 22 minutes before. That shift has not been investigated.
4. Fuel burn and the AI's use of fuel (item 9), then interception (item 10).
5. Slice 4. The game has not been played by hand against the AI yet, so judge
   whether the fighting is fun before spending on polish.

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

- No combat effects or sound.
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
