# Current Status

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
renders the zone, inbound transport placeholder, and status feedback.

The verified smoke scenario is documented in
`docs/refactor/CONTROL_POINT_BATTLE_LOOP.md`. Claims here are limited to the
assertion-backed scenario and native integration test; no strategic territory
or objective-aware AI completion is implied.
