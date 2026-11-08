extends SceneTree
## Scripted gameplay demo for recording. Drives a real Broken Strait match and
## its camera through a few shots, with a caption for each.
##   RTS_AUTO_START_SKIRMISH=1 godot --path godot/project --resolution 1920x1080 \
##     --fixed-fps 30 --write-movie demo.avi --script res://demo_reel.gd
## DEMO_STILLS=<folder> saves one still per shot instead of needing a movie.

const BUILD_POSITION := Vector2(-12500.0, 0.0)

var view: Node
var extension: Object
var caption: Label
var stills := ""
var shot_index := 0


func _initialize() -> void:
	stills = OS.get_environment("DEMO_STILLS")
	_run.call_deferred()


func _caption(text: String) -> void:
	caption.text = text
	caption.modulate.a = 0.0
	create_tween().tween_property(caption, "modulate:a", 1.0, 0.6)


# Runs frames while easing the camera; extra ticks fast-forward the match.
func _hold(seconds: float, target_from: Vector3, target_to: Vector3, distance_from: float, distance_to: float,
		yaw_from: float, yaw_to: float, extra_ticks := 0, follow := Callable()) -> void:
	var frames := int(seconds * 30.0)
	for frame in range(frames):
		var t := float(frame) / float(maxi(frames - 1, 1))
		var eased := t * t * (3.0 - 2.0 * t)
		var target := target_from.lerp(target_to, eased)
		if follow.is_valid():
			target = follow.call()
		view.camera_target = target
		view.target_camera_distance = lerpf(distance_from, distance_to, eased)
		view.tactical_camera_yaw = lerpf(yaw_from, yaw_to, eased)
		_hide_range_rings()
		for _tick in range(extra_ticks):
			extension.call("update_simulation", 50.0)
		if extra_ticks > 0:
			view.call("_sync_new_entities")
			view.call("_sync_unit_transforms")
		await process_frame
		if OS.get_environment("DEMO_TRACE") == "1" and frame % 30 == 0:
			print("DEMO trace shot=%d target=%s camera=%s" % [shot_index, view.camera_target, view.camera.global_position])
		if not stills.is_empty() and frame == frames * 2 / 3:
			await RenderingServer.frame_post_draw
			root.get_viewport().get_texture().get_image().save_png(stills.path_join("shot_%02d.png" % shot_index))
	shot_index += 1


func _ground(point: Vector2, lift := 0.0) -> Vector3:
	return Vector3(point.x, float(view.call("_terrain_height_at", point.x, point.y)) + lift, point.y)


func _centroid(ids: Array) -> Vector3:
	var sum := Vector2.ZERO
	var count := 0
	for id in ids:
		var position: Array = extension.call("get_unit_position", id)
		if position.size() >= 2 and not view.removed_entity_ids.has(int(id)):
			sum += Vector2(float(position[0]), float(position[1]))
			count += 1
	if count == 0:
		return view.camera_target
	return _ground(sum / float(count))


func _spawn_group(unit_type: int, faction: int, center: Vector2, columns: int, rows: int, spacing: float) -> Array:
	var ids := []
	for row in range(rows):
		for column in range(columns):
			var point := center + Vector2((column - (columns - 1) * 0.5) * spacing, (row - (rows - 1) * 0.5) * spacing)
			var id := int(extension.call("create_unit_with_type", point.x, point.y, unit_type, faction))
			if id > 0:
				ids.append(id)
			else:
				print("DEMO could not place unit at %s land=%s" % [point, extension.call("is_land_position", point.x, point.y)])
	view.call("_sync_new_entities")
	return ids


func _run() -> void:
	view = load("res://main.tscn").instantiate()
	root.add_child(view)
	var layer := CanvasLayer.new()
	layer.layer = 50
	caption = Label.new()
	caption.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	caption.add_theme_font_size_override("font_size", 34)
	caption.add_theme_color_override("font_color", Color(0.95, 0.97, 0.92))
	caption.add_theme_color_override("font_outline_color", Color(0.0, 0.0, 0.0, 0.85))
	caption.add_theme_constant_override("outline_size", 10)
	caption.set_anchors_and_offsets_preset(Control.PRESET_CENTER_BOTTOM)
	caption.offset_top = -170.0
	caption.offset_bottom = -120.0
	caption.offset_left = -900.0
	caption.offset_right = 900.0
	layer.add_child(caption)
	# A title card covers the load; the movie records from the first frame.
	var card := ColorRect.new()
	card.color = Color(0.02, 0.03, 0.03)
	card.set_anchors_preset(Control.PRESET_FULL_RECT)
	var title := Label.new()
	title.text = "MANDATE OF WAR\n\na near-future real-time strategy game\nwork in progress"
	title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	title.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	title.add_theme_font_size_override("font_size", 40)
	title.add_theme_color_override("font_color", Color(0.85, 0.9, 0.82))
	title.set_anchors_preset(Control.PRESET_FULL_RECT)
	card.add_child(title)
	layer.add_child(card)
	root.add_child(layer)
	var loading_frames := 0
	for _frame in range(900):
		await process_frame
		loading_frames += 1
		if view.match_started and view.terrain_vegetation != null and not view.terrain_vegetation.building:
			break
	print("DEMO loading_frames=%d" % loading_frames)
	create_tween().tween_property(card, "modulate:a", 0.0, 1.2)
	extension = view.extension
	# The fog layer darkens everything not yet scouted; the demo shows the map.
	if view.fog_view != null:
		view.fog_view.visible = false
	view.call("_clear_selection")
	view.call("_update_hud")
	var base := _ground(BUILD_POSITION)

	# 1. The theatre from above, closing on the player's landing ground.
	_caption("MANDATE OF WAR  //  BROKEN STRAIT")
	await _hold(7.0, _ground(Vector2(-6000.0, 0.0)), base, 9000.0, 1400.0, 0.5, 0.0)

	# 2. Across the forest.
	_caption("Forests of real tree models, every one of them drawn")
	var forest := Vector2(-13300.0, 3000.0)
	await _hold(8.0, _ground(forest + Vector2(-500.0, 0.0)), _ground(forest + Vector2(500.0, 300.0)), 260.0, 420.0, -0.6, 0.6)

	# 3. The field engineer lays out an airfield (fast-forwarded).
	var engineer := int(view.call("_player_engineer_id"))
	view.call("_set_selected", engineer, true)
	var airfield_queued := bool(view.call("_queue_commander_structure", 2, BUILD_POSITION))
	view.call("_clear_selection")
	view.call("_update_hud")
	_caption("The field engineer levels ground and builds an airfield  (fast-forward)")
	await _hold(6.0, base, base, 700.0, 520.0, 0.2, -0.5, 10)
	var installed: Array = extension.call("territory_get_installation_info", BUILD_POSITION.x, BUILD_POSITION.y)
	var guard := 0
	while installed.size() < 6 and guard < 120:
		await _hold(0.5, base, base, 520.0, 520.0, -0.5, -0.5, 10)
		installed = extension.call("territory_get_installation_info", BUILD_POSITION.x, BUILD_POSITION.y)
		guard += 1
	if view.get_node_or_null("Built_Airfield") == null:
		view.call("_spawn_completed_structure_view", 2, BUILD_POSITION)
	print("DEMO airfield queued=%s complete=%s" % [airfield_queued, installed.size() >= 6])

	# 4. The finished airfield.
	view.call("_clear_selection")
	view.call("_update_hud")
	_caption("Airfield complete: the gateway for reinforcements")
	await _hold(6.0, base, base, 420.0, 300.0, -0.5, 0.7)

	# 4b. Low along the runway, as a pilot on final approach sees it.
	_caption("Runway 36, taxiway and apron, laid on the levelled ground")
	await _hold(6.0, base + Vector3(-55.0, 0.0, 330.0), base + Vector3(-40.0, 0.0, 60.0), 150.0, 120.0, 0.15, -0.35)

	# 4c. A fighter ferried in from a strategic airfield. Its long flight in
	# from beyond the map edge is skipped inside one frame.
	var fighter := _order_fighter(engineer)
	print("DEMO fighter=%d" % fighter)
	if fighter > 0:
		var model := view.prototype_visual_views.get(fighter) as Node3D
		_caption("An F-15C ferried in from a strategic airfield")
		var follow := func() -> Vector3: return model.global_position if is_instance_valid(model) else view.camera_target
		await _hold(4.0, base, base, 60.0, 90.0, 2.4, 1.6, 0, follow)
		await _hold(4.0, base, base, 90.0, 260.0, 1.6, 0.9, 0, follow)

	# 5. Armour moves out in formation.
	var tanks := _spawn_group(0, 0, Vector2(-11700.0, 450.0), 4, 3, 30.0)
	print("DEMO tanks=%d" % tanks.size())
	view.call("_clear_selection")
	var objective := Vector2(-10300.0, 700.0)
	extension.call("issue_move_commands", PackedInt32Array(tanks), 0, objective.x, objective.y, 26.0)
	_caption("Elite Precision armour advances in formation")
	await _hold(10.0, base, base, 95.0, 160.0, 0.9, 0.3, 3, func(): return _centroid(tanks))

	# 7. Back out to the strategic map.
	_caption("Zoom out at any time to command the whole theatre")
	var here: Vector3 = view.camera_target
	await _hold(7.0, here, _ground(Vector2(-9000.0, 0.0)), 520.0, 8500.0, -0.3, 0.0, 1)
	caption.text = ""
	await _hold(1.5, view.camera_target, view.camera_target, 8500.0, 8500.0, 0.0, 0.0)
	print("DEMO done tanks_left=%d " % [_alive(tanks)])
	quit(0)


# Orders a fighter through the engineer, then runs the simulation until it has
# crossed the coast and is a few kilometres short of the airfield.
func _order_fighter(engineer: int) -> int:
	var before: Array = view.player_entity_ids.duplicate()
	view.call("_set_selected", engineer, true)
	view.call("_queue_commander_unit", 9)
	view.call("_clear_selection")
	view.call("_update_hud")
	var fighter := 0
	for _tick in range(3000):
		extension.call("update_simulation", 50.0)
		view.call("_sync_new_entities")
		if fighter == 0:
			for id in view.player_entity_ids:
				if not before.has(id) and int(view.entity_unit_types.get(id, -1)) == 9:
					fighter = int(id)
		if fighter > 0:
			var position: Array = extension.call("get_unit_position", fighter)
			if position.size() >= 2 and float(position[0]) > BUILD_POSITION.x - 3500.0:
				break
	view.call("_sync_unit_transforms")
	return fighter


# The always-on range hexes clutter close shots of a demo.
func _hide_range_rings() -> void:
	for model in view.prototype_visual_views.values():
		if model == null:
			continue
		for ring_name in ["VisibilityHex", "RadarHex", "AttackHex"]:
			var ring := (model as Node).get_node_or_null(ring_name) as Node3D
			if ring != null:
				ring.visible = false


func _alive(ids: Array) -> int:
	var count := 0
	for id in ids:
		if not view.removed_entity_ids.has(int(id)):
			count += 1
	return count
