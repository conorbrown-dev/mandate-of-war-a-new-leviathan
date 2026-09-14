# Control-Point Battle Loop

`control_point_skirmish_smoke` is a tactical vertical slice launched through
the existing `main.tscn` controller. It starts twelve player Industrial MBTs,
one opposing ground defender, and one visible objective, Alpha, on Broken
Strait's west landmass.

## Rules

The native `rts::ControlPointBattle` owns all point state. Godot receives
snapshots only; it does not calculate capture, ownership, contesting, or match
results.

- Eligible units are living ground units with `GroundSteering`, grouped by
  native faction inside the point radius.
- Both factions in a point leave its progress unchanged and mark it
  `CONTESTED`.
- An uncontested eligible unit moves signed capture progress toward its faction.
  Progress ranges from `-1` (enemy) to `+1` (player); a defender therefore
  reverses unfinished player progress rather than merely pausing it.
- At `+1`, Alpha is friendly. It must remain occupied by an uncontested player
  ground unit for the configured hold duration to produce `VICTORY`.
- A defeat can occur only after the player has held all configured points and
  subsequently loses them to an uncontested enemy hold. An opening defender
  cannot end a match before the arriving player force has an opportunity to
  contest it.
- Restarting simulation calls `ControlPointBattle::reset()`, clearing points,
  progress, hold time, and result.

## Tunables

| Value | Current setting | Authority |
| --- | ---: | --- |
| Capture rate | `0.20` progress/unit/second | `ControlPointBattle::kCaptureRatePerUnitPerSecond` in `src/simulation/control_point_battle.hpp` |
| Objective radius | `55 m` | `_start_control_point_skirmish()` in `godot/project/main.gd` |
| Win hold | `3000 ms` | `_start_control_point_skirmish()` in `godot/project/main.gd` |
| Player force | 12 Industrial MBTs | `_start_control_point_skirmish()` in `godot/project/main.gd` |
| Defender | 1 opposing ground engineering vehicle | `_start_control_point_skirmish()` in `godot/project/main.gd` |

The scenario uses the established native movement tuning; vehicle acceleration,
braking, turn behavior, arrival radius, and spacing remain in
`src/simulation/movement_tuning.hpp` and faction unit prototype data.

## Run

Interactive scenario:

```bash
RTS_CONTROL_POINT_SKIRMISH=1 ./Godot_v4.7.2-stable_linux.x86_64 --path godot/project
```

Automated smoke scenario with deterministic assertions:

```bash
PYTHONDONTWRITEBYTECODE=1 python3 tools/validate.py control_point_skirmish_smoke --rendered --require-screenshots --timeout 120
```

The scenario deliberately frames Alpha and the route from the player vehicles
to the defender. Drag-select the blue vehicles, right-click Alpha, and use the
existing attack input against the defender to open the capture window.

## Deferred

This slice has one point and a static defender. It does not add objective-aware
AI, multi-point balancing, capture-specific collision avoidance, a campaign
territory layer, or a new unit roster.
