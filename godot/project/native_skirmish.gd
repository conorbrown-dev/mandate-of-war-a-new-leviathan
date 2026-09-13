extends Node2D

# Native Goal 08 player route.  This intentionally talks only to the bounded
# Skirmish controller, so the presentation, terminal result, and replay share
# one authoritative simulation rather than the legacy free-form demo world.
const SCENARIO := "res://scenarios/two_landmass_skirmish.json"
const WORLD_HALF := 160.0
const MAP_MARGIN := 32.0

var extension: Object
var state: Dictionary = {}
var selected_ids := PackedInt32Array()
var paused := false
var load_error := ""
var result_message := ""
var status_message := "SELECT ELITE UNITS. RIGHT-CLICK TO MOVE. CTRL+RIGHT-CLICK TO ATTACK."

var title: Label
var telemetry: Label
var help: Label
var result_panel: PanelContainer
var result_label: Label
var screenshot_saved := false

func _ready() -> void:
	set_process(true)
	if not ClassDB.class_exists("RtsExtension"):
		load_error = "Native extension unavailable"
		queue_redraw()
		return
	extension = ClassDB.instantiate("RtsExtension")
	load_error = String(extension.call("skirmish_load", SCENARIO))
	if load_error.is_empty():
		state = extension.call("skirmish_state")
	_build_hud()
	if load_error.is_empty() and OS.get_environment("RTS_NATIVE_FAST_FORWARD_RESULT") == "1":
		for _tick in range(20000):
			if int(state.get("result", -1)) >= 0:
				break
			extension.call("skirmish_update", 50.0)
			state = extension.call("skirmish_state")
		_complete_match()
	queue_redraw()

func _build_hud() -> void:
	title = Label.new()
	title.position = Vector2(32, 12)
	title.add_theme_font_size_override("font_size", 22)
	title.text = "BROKEN STRAIT // DETERMINISTIC SKIRMISH"
	add_child(title)
	telemetry = Label.new()
	telemetry.position = Vector2(32, 42)
	telemetry.add_theme_font_size_override("font_size", 14)
	add_child(telemetry)
	help = Label.new()
	help.position = Vector2(32, get_viewport_rect().size.y - 72.0)
	help.size = Vector2(maxf(320.0, get_viewport_rect().size.x - 64.0), 56.0)
	help.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	add_child(help)
	result_panel = PanelContainer.new()
	result_panel.visible = false
	result_panel.position = (get_viewport_rect().size - Vector2(440.0, 200.0)) * 0.5
	result_panel.size = Vector2(440, 200)
	var box := VBoxContainer.new()
	box.add_theme_constant_override("separation", 12)
	result_panel.add_child(box)
	result_label = Label.new()
	result_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	result_label.add_theme_font_size_override("font_size", 20)
	box.add_child(result_label)
	var rematch := Button.new()
	rematch.text = "REMATCH"
	rematch.pressed.connect(_rematch)
	box.add_child(rematch)
	var exit := Button.new()
	exit.text = "EXIT TO SETUP"
	exit.pressed.connect(func(): get_tree().change_scene_to_file("res://main.tscn"))
	box.add_child(exit)
	add_child(result_panel)

func _process(delta: float) -> void:
	if extension == null or not load_error.is_empty():
		return
	if not paused and int(state.get("result", -1)) < 0:
		extension.call("skirmish_update", minf(delta * 1000.0, 250.0))
		state = extension.call("skirmish_state")
		if int(state.get("result", -1)) >= 0:
			_complete_match()
	_update_hud()
	queue_redraw()
	if not screenshot_saved and OS.get_environment("RTS_CAPTURE_NATIVE_SCREENSHOT") == "1":
		screenshot_saved = true
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png("/tmp/g08-native-skirmish.png")

func _update_hud() -> void:
	if telemetry == null:
		return
	var aircraft := []
	var naval_stranded := 0
	var visible_enemy := 0
	var intel_current := 0
	var intel_stale := 0
	for record in state.get("intelligence", []):
		if bool(record.get("currently_observed", false)):
			intel_current += 1
		if bool(record.get("stale", false)):
			intel_stale += 1
	for unit in state.get("units", []):
		if int(unit.get("faction", -1)) == 1:
			visible_enemy += 1
		if String(unit.get("kind", "")) == "air":
			var return_cost := ""
			if is_finite(float(unit.get("return_energy", 0.0))):
				return_cost = " COST %.0fE/%.0fM" % [float(unit.get("return_energy", 0.0)), float(unit.get("return_material", 0.0))]
			aircraft.append("%.0f/%.0f%s%s" % [float(unit.get("fuel", 0.0)), float(unit.get("max_fuel", 0.0)), " RETURN" if bool(unit.get("safe_return", false)) else "", return_cost])
		if String(unit.get("kind", "")) == "sea" and bool(unit.get("stranded", false)):
			naval_stranded += 1
	var logistics := "AIR %s  //  RECOVERY Q %d/%d  //  NAVAL STRANDED %d  //  INTEL %d CURRENT / %d STALE / %d VISIBLE" % [
		", ".join(aircraft) if not aircraft.is_empty() else "NONE", int(state.get("takeoff_queue", 0)), int(state.get("landing_queue", 0)), naval_stranded, intel_current, intel_stale, visible_enemy]
	telemetry.text = "MATERIAL %.0f  +%.1f    ENERGY %.0f  +%.1f    RESEARCH %.0f  +%.1f    TICK %d    %s\n%s" % [
		float(state.get("material", 0.0)), float(state.get("material_income", 0.0)),
		float(state.get("energy", 0.0)), float(state.get("energy_income", 0.0)),
		float(state.get("research", 0.0)), float(state.get("research_income", 0.0)),
		int(state.get("tick", 0)), "PAUSED" if paused else "LIVE", logistics]
	help.text = "%s\n[P] pause  [X] stop  [R] return/recover  [B] queue MBT  [T] research  [H] harvest  [K] patrol  [D] defend  [Right-click] move  [Ctrl+Right-click] attack" % status_message

func _map_rect() -> Rect2:
	var viewport := get_viewport_rect().size
	return Rect2(MAP_MARGIN, 82.0, viewport.x - MAP_MARGIN * 2.0, maxf(120.0, viewport.y - 160.0))

func _world_to_screen(x: float, y: float) -> Vector2:
	var rect := _map_rect()
	return rect.position + Vector2((x + WORLD_HALF) / (WORLD_HALF * 2.0) * rect.size.x, (y + WORLD_HALF) / (WORLD_HALF * 2.0) * rect.size.y)

func _screen_to_world(point: Vector2) -> Vector2:
	var rect := _map_rect()
	return Vector2((point.x - rect.position.x) / rect.size.x * WORLD_HALF * 2.0 - WORLD_HALF, (point.y - rect.position.y) / rect.size.y * WORLD_HALF * 2.0 - WORLD_HALF)

func _draw() -> void:
	var rect := _map_rect()
	draw_rect(rect, Color("#07131d"), true)
	draw_rect(rect, Color("#2e6478"), false, 2.0)
	# Two authored landmasses and the narrow causeway are deliberately visible.
	for land in [Rect2(-160, -150, 110, 300), Rect2(50, -150, 110, 300), Rect2(-50, -8, 100, 16)]:
		var top_left := _world_to_screen(land.position.x, land.position.y)
		var bottom_right := _world_to_screen(land.end.x, land.end.y)
		draw_rect(Rect2(top_left, bottom_right - top_left), Color("#254b2b"), true)
	for projectile in state.get("projectiles", []):
		var point := _world_to_screen(float(projectile.get("x", 0.0)), float(projectile.get("y", 0.0)))
		draw_circle(point, 2.5, Color("#ffd166"))
	for unit in state.get("units", []):
		var id := int(unit.get("id", 0))
		var faction := int(unit.get("faction", -1))
		var point := _world_to_screen(float(unit.get("x", 0.0)), float(unit.get("y", 0.0)))
		var color := Color("#4ed9ff") if faction == 0 else Color("#ff6d65")
		if String(unit.get("kind", "ground")) == "air":
			draw_colored_polygon(PackedVector2Array([point + Vector2(0, -7), point + Vector2(-6, 6), point + Vector2(6, 6)]), color)
		elif String(unit.get("kind", "ground")) == "sea":
			draw_rect(Rect2(point - Vector2(6, 3), Vector2(12, 6)), color, true)
		else:
			draw_circle(point, 5.0 if bool(unit.get("base", false)) else 3.5, color)
		if selected_ids.has(id):
			draw_arc(point, 8.0, 0.0, TAU, 16, Color.WHITE, 1.5)
		var health := float(unit.get("health", 0.0)) / maxf(1.0, float(unit.get("max_health", 1.0)))
		draw_rect(Rect2(point + Vector2(-5, 7), Vector2(10, 2)), Color("#291318"), true)
		draw_rect(Rect2(point + Vector2(-5, 7), Vector2(10 * health, 2)), Color("#7ad99b"), true)
	if not load_error.is_empty():
		draw_string(ThemeDB.fallback_font, rect.get_center(), load_error, HORIZONTAL_ALIGNMENT_CENTER, -1, 18, Color.RED)

func _unhandled_input(event: InputEvent) -> void:
	if extension == null or not load_error.is_empty() or int(state.get("result", -1)) >= 0:
		return
	if event is InputEventKey and event.pressed and not event.echo:
		match event.keycode:
			KEY_P:
				paused = not paused
				status_message = "MATCH PAUSED" if paused else "MATCH RESUMED"
			KEY_X: _submit(2)
			KEY_R: _submit(5)
			KEY_B: _submit_world(3, Vector2(-92.0, 0.0), 0, PackedInt32Array([int(state.get("base", 0))]))
			KEY_T: _submit_world(8, Vector2.ZERO, 0, PackedInt32Array([int(state.get("base", 0))]))
			KEY_H: _harvest_nearest_resource()
			KEY_K: _submit_world(7, Vector2(0.0, 0.0))
			KEY_D: _submit_world(6, Vector2(0.0, 0.0))
	if event is InputEventMouseButton and event.pressed and _map_rect().has_point(event.position):
		if event.button_index == MOUSE_BUTTON_LEFT:
			_select_at(event.position)
		elif event.button_index == MOUSE_BUTTON_RIGHT:
			if event.ctrl_pressed:
				_submit(1, event.position, _enemy_at(event.position))
			else:
				_submit(0, event.position)

func _select_at(point: Vector2) -> void:
	var nearest := 18.0
	var selected := 0
	for unit in state.get("units", []):
		if int(unit.get("faction", -1)) != 0:
			continue
		var distance := point.distance_to(_world_to_screen(float(unit.get("x", 0.0)), float(unit.get("y", 0.0))))
		if distance < nearest:
			nearest = distance
			selected = int(unit.get("id", 0))
	selected_ids = PackedInt32Array([selected]) if selected > 0 else PackedInt32Array()
	status_message = "UNIT %d SELECTED" % selected if selected > 0 else "NO ELITE UNIT SELECTED"

func _enemy_at(point: Vector2) -> int:
	var nearest := 20.0
	var target := 0
	for unit in state.get("units", []):
		if int(unit.get("faction", -1)) == 0:
			continue
		var distance := point.distance_to(_world_to_screen(float(unit.get("x", 0.0)), float(unit.get("y", 0.0))))
		if distance < nearest:
			nearest = distance
			target = int(unit.get("id", 0))
	return target

func _submit(type: int, screen_target := Vector2.ZERO, extra := 0) -> void:
	if selected_ids.is_empty():
		status_message = "SELECT AN ELITE UNIT FIRST"
		return
	var world_target := _screen_to_world(screen_target) if screen_target != Vector2.ZERO else Vector2.ZERO
	var accepted := int(extension.call("skirmish_command", type, selected_ids, world_target.x, world_target.y, extra))
	status_message = "COMMAND ACCEPTED" if accepted > 0 else "COMMAND REJECTED BY AUTHORITATIVE RULES"

func _complete_match() -> void:
	var result := int(state.get("result", 2))
	result_message = "VICTORY" if result == 0 else "DEFEAT" if result == 1 else "DRAW"
	var replay_error := String(extension.call("skirmish_save_replay", "user://matches/latest-g08.replay"))
	var summary: Dictionary = extension.call("skirmish_stats_summary")
	result_label.text = "%s\nTICK %d\nHISTORY %d MATCH(ES) // %d WIN(S)\n%s" % [result_message, int(state.get("tick", 0)), int(summary.get("total_matches", 0)), int(summary.get("total_wins", 0)), "REPLAY SAVED" if replay_error.is_empty() else "REPLAY SAVE FAILED: %s" % replay_error]
	result_panel.visible = true
	status_message = "MATCH COMPLETE"

func _harvest_nearest_resource() -> void:
	if selected_ids.is_empty() or state.get("resources", []).is_empty():
		status_message = "SELECT AN ELITE UNIT AND ENSURE A RESOURCE IS AVAILABLE"
		return
	var resource: Dictionary = state.get("resources", [])[0]
	_submit_world(4, Vector2(float(resource.get("x", 0.0)), float(resource.get("y", 0.0))))

func _submit_world(type: int, world_target: Vector2, extra := 0, ids := PackedInt32Array()) -> void:
	var command_ids := ids if not ids.is_empty() else selected_ids
	if command_ids.is_empty():
		status_message = "SELECT AN ELITE UNIT FIRST"
		return
	var accepted := int(extension.call("skirmish_command", type, command_ids, world_target.x, world_target.y, extra))
	status_message = "COMMAND ACCEPTED" if accepted > 0 else "COMMAND REJECTED BY AUTHORITATIVE RULES"

func _rematch() -> void:
	load_error = String(extension.call("skirmish_load", SCENARIO))
	state = extension.call("skirmish_state") if load_error.is_empty() else {}
	selected_ids = PackedInt32Array()
	paused = false
	result_panel.visible = false
	status_message = "REMATCH READY"
