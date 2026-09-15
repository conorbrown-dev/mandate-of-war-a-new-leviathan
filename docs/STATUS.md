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
