extends Control
var match_view: Node3D
const UiTypographyScript = preload("res://ui_typography.gd")
var ui_font: Font

func _ready() -> void:
	ui_font = UiTypographyScript.monospace_font()

func _draw() -> void:
	if match_view == null or not match_view.match_started or match_view.startup_overlay.visible:
		return
	var camera: Camera3D = match_view.camera
	for unit in match_view.match_state.get("units", []):
		var world := Vector3(unit.x, 2, unit.y)
		if camera.is_position_behind(world):
			continue
		var at := camera.unproject_position(world)
		draw_rect(Rect2(at + Vector2(-10, -6), Vector2(20, 3)), Color(0.15, 0.15, 0.15))
		draw_rect(Rect2(at + Vector2(-10, -6), Vector2(20 * clampf(float(unit.health)/maxf(float(unit.max_health), 1), 0, 1), 3)), Color.GREEN if int(unit.faction)==0 else Color.RED)
	for shot in match_view.match_state.get("projectiles", []):
		var world := Vector3(shot.x, 1, shot.y)
		if not camera.is_position_behind(world):
			draw_circle(camera.unproject_position(world), 2, Color.YELLOW if int(shot.faction) == 0 else Color.ORANGE_RED)
	for id in match_view.last_observed:
		if match_view.ai_entity_ids.has(id):
			continue
		var memory: Dictionary = match_view.last_observed[id]
		var at := camera.unproject_position(Vector3(memory.x, 1, memory.y))
		draw_circle(at, 6, Color(0.6, 0.6, 0.6, 0.6), false, 1)
		draw_string(ui_font if ui_font != null else ThemeDB.fallback_font, at+Vector2(8, 0), "LAST SEEN %.0FS" % ((float(match_view.match_state.tick)-float(memory.tick))/20), HORIZONTAL_ALIGNMENT_LEFT, -1, 13, Color.GRAY)
