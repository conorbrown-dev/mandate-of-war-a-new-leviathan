extends SceneTree

# Aircraft stay visible when zoomed out: the main view draws them to half of
# full zoom, enlarged to keep their on-screen size, at a fixed drawn altitude
# with strategic recon highest. Strategic icons sit above the
# unit as drawn. Presentation only; nothing here changes the simulation.

const KESTREL_FIGHTER := 9
const LONGSIGHT_RECON := 14
const HIGHWATCH_RECON := 20
const ELITE_MBT := 0

var checks := 0
var failures := 0


func check(condition: bool, message: String) -> void:
	checks += 1
	if not condition:
		failures += 1
		push_error(message)


func _initialize() -> void:
	_run.call_deferred()


func _zoom(view: Node3D, distance: float) -> void:
	view.camera_distance = distance
	view.target_camera_distance = distance
	view._update_camera(1.0)
	view._sync_unit_transforms()


func _run() -> void:
	var view: Node3D = load("res://main.tscn").instantiate()
	root.add_child(view)
	await process_frame
	view._on_start_skirmish_pressed()
	view.set_process(false)
	var extension: Object = view.extension
	var ids := {}
	for unit_type in [KESTREL_FIGHTER, LONGSIGHT_RECON, HIGHWATCH_RECON, ELITE_MBT]:
		ids[unit_type] = int(extension.call("create_unit_with_type", -12500.0 + unit_type * 40.0, 1000.0, unit_type, 0))
		check(ids[unit_type] > 0, "unit %d spawns" % unit_type)
	view._sync_new_entities()
	extension.call("update_simulation", 50.0)
	for unit_type in [KESTREL_FIGHTER, LONGSIGHT_RECON, HIGHWATCH_RECON]:
		check(view._is_aircraft_type(unit_type), "unit %d is presented as an aircraft" % unit_type)
	check(not view._is_aircraft_type(ELITE_MBT), "a tank is not presented as an aircraft")

	var half_zoom: float = view.CAMERA_MAX_DISTANCE * 0.5
	view.camera_target = view.prototype_visual_views[ids[HIGHWATCH_RECON]].global_position
	_zoom(view, half_zoom * 0.98)
	var viewport_height := float(view.get_viewport().get_visible_rect().size.y)
	var meters_per_pixel: float = 2.0 * view.camera_distance * tan(deg_to_rad(view.camera.fov) * 0.5) / viewport_height
	for unit_type in [KESTREL_FIGHTER, LONGSIGHT_RECON, HIGHWATCH_RECON]:
		var wrapper = view.prototype_visual_views[ids[unit_type]]
		check(wrapper.model_drawn_by(view.camera), "unit %d is still drawn at half of full zoom" % unit_type)
		check(wrapper.model_layer() == UnitVisualRoot.AIRCRAFT_LAYER, "unit %d draws on the aircraft layer" % unit_type)
		check(wrapper.presentation_scale() > 20.0, "unit %d is enlarged when zoomed out (x%.1f)" % [unit_type, wrapper.presentation_scale()])
		check(wrapper.drawn_radius() * 2.0 / meters_per_pixel >= 20.0, "unit %d spans at least 20 px at half zoom (%.0f px)" % [unit_type, wrapper.drawn_radius() * 2.0 / meters_per_pixel])
		var placement: Dictionary = view.strategic_icon_overlay.marker_placement(ids[unit_type], view.camera)
		check(placement.radius_pixels > 5.0 and placement.icon_center.y < placement.anchor.y - placement.radius_pixels - 20.0, "unit %d's icon sits above it as drawn" % unit_type)
	check(not view.prototype_visual_views[ids[ELITE_MBT]].model_drawn_by(view.camera), "ground units still hand over to their icons when zoomed out")
	check(is_equal_approx(view.prototype_visual_views[ids[ELITE_MBT]].presentation_scale(), 1.0), "ground units are never enlarged")

	_zoom(view, half_zoom * 1.05)
	check(not view.prototype_visual_views[ids[KESTREL_FIGHTER]].model_drawn_by(view.camera), "beyond half of full zoom aircraft hand over to their icons")

	_zoom(view, 200.0)
	check(is_equal_approx(view.prototype_visual_views[ids[KESTREL_FIGHTER]].presentation_scale(), 1.0), "close in, aircraft are their authored size")
	check(view.prototype_visual_views[ids[KESTREL_FIGHTER]].model_drawn_by(view.camera), "close in, aircraft are drawn")

	# Drawn altitude is fixed: zooming changes how big an aircraft is drawn, never
	# how high it flies.
	var heights := {}
	for distance in [600.0, 2000.0, half_zoom * 0.98]:
		_zoom(view, distance)
		for unit_type in [KESTREL_FIGHTER, LONGSIGHT_RECON, HIGHWATCH_RECON]:
			var height: float = view._presented_cruise_height(unit_type)
			check(not heights.has(unit_type) or is_equal_approx(heights[unit_type], height), "unit %d keeps its altitude at %.0f m camera distance" % [unit_type, distance])
			heights[unit_type] = height
	print("AIRCRAFT_PRESENTATION drawn altitude: fighter %.0f, recon %.0f, strategic recon %.0f" % [heights[KESTREL_FIGHTER], heights[LONGSIGHT_RECON], heights[HIGHWATCH_RECON]])
	check(heights[HIGHWATCH_RECON] > heights[LONGSIGHT_RECON] and heights[LONGSIGHT_RECON] > heights[KESTREL_FIGHTER], "strategic recon flies above recon, and recon above fighters")
	check(heights[HIGHWATCH_RECON] < half_zoom * view.CAMERA_HEIGHT_PER_DISTANCE * 0.5, "at half zoom strategic recon is well below the camera")
	var drawn_at := []
	for distance in [2000.0, half_zoom * 0.98]:
		_zoom(view, distance)
		view.aircraft_heights[ids[HIGHWATCH_RECON]] = 1.0  # at cruise
		drawn_at.append(view._presented_flight_height(ids[HIGHWATCH_RECON], true, -12500.0, 1000.0, view._terrain_height_at(-12500.0, 1000.0), 0.0))
	check(is_equal_approx(drawn_at[0], drawn_at[1]), "a strategic recon plane in flight stays at the same height while the camera zooms")
	# An aircraft is picked where it is drawn, high above its ground position:
	# a click or a selection box there selects it.
	_zoom(view, 6000.0)
	var plane: Node3D = view.prototype_visual_views[ids[HIGHWATCH_RECON]]
	var ground: Vector3 = view._entity_world_position(ids[HIGHWATCH_RECON])
	plane.global_position = ground + Vector3.UP * view._presented_cruise_height(HIGHWATCH_RECON)
	view._update_camera(1.0)
	var plane_on_screen: Vector2 = view.camera.unproject_position(plane.global_position)
	check(plane_on_screen.distance_to(view.camera.unproject_position(ground)) > 40.0, "the drawn plane is well clear of its ground point on screen")
	view._clear_selection()
	view._select_nearest(plane_on_screen)
	check(view.selected_ids.has(ids[HIGHWATCH_RECON]), "clicking a plane where it is drawn selects it")
	view._clear_selection()
	view._select_in_rectangle(Rect2(plane_on_screen - Vector2(15, 15), Vector2(30, 30)))
	check(view.selected_ids.has(ids[HIGHWATCH_RECON]), "a selection box around a plane where it is drawn selects it")
	view._clear_selection()
	view.queue_free()
	await process_frame
	print("AIRCRAFT_PRESENTATION checks=%d failures=%d" % [checks, failures])
	quit(1 if failures else 0)
