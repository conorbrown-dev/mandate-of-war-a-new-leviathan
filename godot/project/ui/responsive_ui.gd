class_name ResponsiveUi
extends RefCounted

## Shared logical layout constants for the 1920x1080 HUD baseline. Keep
## screen-relative controls anchored; use these values only for local padding,
## compact panel limits, and custom-drawn HUD geometry.

const REFERENCE_SIZE := Vector2(1920.0, 1080.0)
const MIN_USABLE_SIZE := Vector2(1280.0, 720.0)
const HUD_EDGE := 14.0
const PANEL_MAX_WIDTH := 560.0


static func layout_scale(viewport_size: Vector2) -> float:
	return clampf(minf(viewport_size.x / REFERENCE_SIZE.x, viewport_size.y / REFERENCE_SIZE.y), 0.62, 1.0)


static func logical_size(viewport_size: Vector2) -> Vector2:
	return viewport_size / layout_scale(viewport_size)


static func centered_origin(viewport_size: Vector2, width: float) -> Vector2:
	var logical := logical_size(viewport_size)
	return Vector2((logical.x - width) * 0.5, HUD_EDGE)
