extends SceneTree
## The command post through the real game: a click selects the player's post,
## the HUD shows its name, integrity, role and inbound logistics, its two
## actions work, no order moves it, box selection leaves it out, and the
## enemy's post is hover-only once discovered. Also the model's structure,
## size, grounding and draw cost.

const CommandPostModel := preload("res://structures/command_post_model.gd")

var checks := 0
var failures := 0


func check(condition: bool, reason: String) -> void:
	checks += 1
	if not condition:
		push_error(reason)
		failures += 1


func _initialize() -> void:
	_run.call_deferred()


func _mouse(view: Node, button: MouseButton, at: Vector2, pressed: bool, ctrl := false) -> void:
	var event := InputEventMouseButton.new()
	event.button_index = button
	event.pressed = pressed
	event.ctrl_pressed = ctrl
	event.position = at
	event.global_position = at
	view._unhandled_input(event)


func _click(view: Node, at: Vector2) -> void:
	_mouse(view, MOUSE_BUTTON_LEFT, at, true)
	_mouse(view, MOUSE_BUTTON_LEFT, at, false)


func _drag(view: Node, from: Vector2, to: Vector2) -> void:
	_mouse(view, MOUSE_BUTTON_LEFT, from, true)
	var motion := InputEventMouseMotion.new()
	motion.position = to
	view._unhandled_input(motion)
	_mouse(view, MOUSE_BUTTON_LEFT, to, false)


func _hover(view: Node, at: Vector2) -> void:
	view._update_hover_context(at)


func _aim(view: Node, world: Vector2) -> void:
	view.camera_target = Vector3(world.x, view._terrain_height_at(world.x, world.y), world.y)
	view.camera_distance = 420.0
	view.target_camera_distance = 420.0
	view.tactical_camera_yaw = 0.0
	view._update_camera(1.0)


func _screen(view: Node, post: Node3D, local: Vector3) -> Vector2:
	return view.camera.unproject_position(post.global_position + local)


func _post_position(view: Node, faction: int) -> Vector2:
	var position: PackedFloat32Array = view.extension.call("get_unit_position", int(view.commander_ids.get(faction, -1)))
	return Vector2(position[0], position[1]) if position.size() >= 2 else Vector2.INF


func _ticks(view: Node, count: int) -> void:
	for _tick in range(count):
		view.extension.call("update_simulation", 50.0)


func _mesh_parts(node: Node, into: Array) -> Array:
	for child in node.get_children():
		if child is GeometryInstance3D and child.visible:
			into.append(child)
		if child.name != "SelectionRing":
			_mesh_parts(child, into)
	return into


func _run() -> void:
	var view: Node = load("res://main.tscn").instantiate()
	root.add_child(view)
	await process_frame
	await process_frame
	view._on_start_skirmish_pressed()
	await process_frame
	view.extension.call("skirmish_set_ai_enabled", false)
	var post: Node3D = view.command_post_views.get(0)
	var enemy_post: Node3D = view.command_post_views.get(1)
	check(post != null and enemy_post != null, "both command post views exist")
	_test_model(view, post, enemy_post)
	_test_selection(view, post)
	_test_actions(view)
	_test_orders(view, post)
	_test_box_select(view, post)
	_test_enemy_post(view, enemy_post)
	print("COMMAND_POST checks=%d failures=%d" % [checks, failures])
	view.queue_free()
	await process_frame
	quit(1 if failures > 0 else 0)


func _test_model(view: Node, post: Node3D, enemy_post: Node3D) -> void:
	for part in ["Pad", "HescoPerimeter", "Headquarters", "OperationsRoom", "RoofMarking", "AccentStripe", "SatelliteDish", "AntennaMast0", "AntennaMast1", "VehicleBayRoof", "TruckCab", "Generator", "FuelBladder", "SandbagRing", "OperationsTent", "FloodlightHead", "Flag", "GateBarrier", "SelectionRing"]:
		check(post.get_node_or_null(part) != null, "the command post has its %s" % part)
	var wall := post.get_node_or_null("HescoPerimeter") as MultiMeshInstance3D
	check(wall != null and wall.multimesh.instance_count >= 50, "the HESCO perimeter is one MultiMesh of many cells")
	# Size: the parts fill the gameplay footprint and stay inside it.
	var footprint: Vector2 = post.get_meta("footprint", Vector2.ZERO)
	check(footprint == CommandPostModel.FOOTPRINT and footprint.x >= 40.0 and footprint.y >= 30.0, "the post declares its footprint")
	var parts := _mesh_parts(post, [])
	var bounds := AABB()
	var first := true
	var materials := {}
	for part: GeometryInstance3D in parts:
		var box: AABB = part.global_transform * part.get_aabb()
		if part is MultiMeshInstance3D:
			box = part.global_transform * (part as MultiMeshInstance3D).multimesh.get_aabb()
		bounds = box if first else bounds.merge(box)
		first = false
		if part.material_override != null:
			materials[part.material_override] = true
	var local_size := bounds.size
	check(local_size.x <= footprint.x + 0.5 and local_size.z <= footprint.y + 0.5, "the post stays inside its %.0f x %.0f m footprint (%.1f x %.1f)" % [footprint.x, footprint.y, local_size.x, local_size.z])
	check(local_size.x >= footprint.x - 1.0 and local_size.z >= footprint.y - 1.0, "the post fills its footprint")
	check(bounds.end.y - post.global_position.y >= 20.0, "the masts rise at least 20 m, readable from the strategic camera")
	# Cheap to render: a bounded number of draw items sharing few materials.
	check(parts.size() <= 60, "the post draws with at most 60 mesh instances (%d)" % parts.size())
	check(materials.size() <= 12, "the post uses at most 12 materials (%d)" % materials.size())
	var shared := 0
	for part: GeometryInstance3D in _mesh_parts(enemy_post, []):
		shared += 1 if materials.has(part.material_override) else 0
	check(shared >= parts.size() - 12, "both posts share their non-faction materials")
	# Faction colour cue.
	var marking := post.get_node("RoofMarking") as MeshInstance3D
	var enemy_marking := enemy_post.get_node("RoofMarking") as MeshInstance3D
	check((marking.material_override as StandardMaterial3D).albedo_color.is_equal_approx(view.PLAYER_FACTION_COLOR), "the player's post carries the player colour")
	check((enemy_marking.material_override as StandardMaterial3D).albedo_color.is_equal_approx(view.AI_FACTION_COLOR), "the enemy's post carries the enemy colour")
	# Grounded: pad top above every terrain sample under it, skirt below every one.
	var pad := post.get_node("Pad") as MeshInstance3D
	var pad_box: AABB = pad.global_transform * pad.get_aabb()
	var grounded := true
	for sample_x in range(-2, 3):
		for sample_z in range(-2, 3):
			var at := Vector2(post.global_position.x + sample_x * footprint.x * 0.25, post.global_position.z + sample_z * footprint.y * 0.25)
			var height: float = view._terrain_height_at(at.x, at.y)
			grounded = grounded and pad_box.end.y >= height and pad_box.position.y <= height
	check(grounded, "the pad sits on the terrain with no gap and nothing buried")
	# The engineer starts outside the compound.
	var engineer: Vector2 = view._native_unit_position(view._player_engineer_id())
	var local := engineer - Vector2(post.global_position.x, post.global_position.z)
	check(absf(local.x) > footprint.x * 0.5 or absf(local.y) > footprint.y * 0.5, "the Field Engineer starts outside the command post")


func _test_selection(view: Node, post: Node3D) -> void:
	_aim(view, Vector2(post.global_position.x, post.global_position.z))
	view._clear_selection()
	# The headquarters building, away from the engineer at the south gate.
	var at := _screen(view, post, Vector3(CommandPostModel.HQ_CENTER.x, 3.0, CommandPostModel.HQ_CENTER.y))
	_click(view, at)
	view._update_hud()
	check(view.command_post_selected, "clicking the command post selects it")
	check(view.strategic_command_view.visible, "clicking the command post opens Strategic Command by default")
	check(view.selected_ids.is_empty(), "the post is not added to the unit selection")
	check((post.get_node("SelectionRing") as Node3D).visible, "the selected post shows its selection frame")
	var panel: Control = view.command_post_panel
	check(panel != null and panel.visible, "the HUD shows the command post panel")
	var text: String = panel.describe() if panel != null else ""
	check(text.contains("FORWARD COMMAND POST"), "the panel names the post: %s" % text)
	check(text.contains("1500 / 1500"), "the panel shows the post's native integrity")
	check(text.contains("HEADQUARTERS") and text.contains("LOSS ENDS THE MATCH"), "the panel states the post's role")
	check(text.contains("NONE IN FLIGHT"), "the panel shows nothing inbound at the start")
	view._close_strategic_command()
	# Clicking the engineer moves the selection to it; the post is released.
	var engineer: Vector2 = view._native_unit_position(view._player_engineer_id())
	_click(view, view.camera.unproject_position(Vector3(engineer.x, view._terrain_height_at(engineer.x, engineer.y), engineer.y)))
	view._update_hud()
	check(not view.command_post_selected and view.selected_ids.has(view._player_engineer_id()), "selecting a unit releases the post")
	check(not panel.visible and not (post.get_node("SelectionRing") as Node3D).visible, "the post panel and frame hide again")
	view._clear_selection()
	_click(view, at)
	view._update_hud()
	check(view.command_post_selected, "the post can be selected again")
	check(view.strategic_command_view.visible, "selecting the post again reopens Strategic Command")
	view._close_strategic_command()


## Fuel and forces both land at an airfield, so the player builds one first.
func _build_airfield(view: Node) -> bool:
	var spawn: Array = view.scenario_definition.player.spawn
	var origin := Vector2(float(spawn[0]), float(spawn[1]))
	for radius in range(1, 12):
		for step in range(16):
			var site := origin + Vector2.RIGHT.rotated(TAU * step / 16.0) * radius * 250.0
			if bool(view.extension.call("validate_structure_placement", 2, site.x, site.y)) and view._queue_commander_structure(2, site, false):
				for _tick in range(20 * 120):
					view.extension.call("update_simulation", 50.0)
					if bool(view.extension.call("delivery_state", 0).get("has_site", false)):
						return true
				return false
	return false


func _test_actions(view: Node) -> void:
	var panel: Control = view.command_post_panel
	check(_build_airfield(view), "the player's engineer builds an airfield for deliveries")
	var ground_type := -1
	for entry in view.extension.call("get_build_catalog", 0):
		if ground_type < 0 and bool(entry.get("delivered", false)) and bool(entry.get("available", false)) and not bool(entry.get("is_aircraft", false)) and not bool(entry.get("is_structure", false)):
			ground_type = int(entry.type)
	check(ground_type >= 0 and view._order_unit_by_transport(ground_type), "a ground force is ordered by transport")
	view.extension.call("update_simulation", 50.0)
	view._update_hud()
	check(not panel.describe().contains("NONE IN FLIGHT") and panel.describe().contains(" S"), "the panel lists the inbound force with its time: %s" % panel.describe())
	var materials_before: float = float(view.extension.call("get_hud_state", 0, int(view.commander_ids[0]), -1, 0.0, 0.0, false).get("tactical_resources", [0.0])[0])
	panel.fuel_button.pressed.emit()
	var supply: Dictionary = view.extension.call("fuel_supply_state", 0)
	check(supply.has("shipment_remaining_ms"), "REQUEST FUEL orders a native fuel shipment")
	var materials_after: float = float(view.extension.call("get_hud_state", 0, int(view.commander_ids[0]), -1, 0.0, 0.0, false).get("tactical_resources", [0.0])[0])
	check(materials_after < materials_before, "the shipment is paid for in Materials")
	view._update_hud()
	check(panel.describe().contains("FUEL SHIPMENT"), "the panel lists the inbound fuel shipment: %s" % panel.describe())
	check(panel.fuel_button.disabled, "a second shipment cannot be requested while one is inbound")
	check(not view.strategic_command_view.visible, "Strategic Command starts closed")
	panel.strategic_button.pressed.emit()
	check(view.strategic_command_view.visible, "STRATEGIC CMD opens Strategic Command")
	view._close_strategic_command()
	check(view.command_post_selected, "the post stays selected after Strategic Command closes")


func _test_orders(view: Node, post: Node3D) -> void:
	var start: Vector2 = _post_position(view, 0)
	check(start.distance_to(Vector2(post.global_position.x, post.global_position.z)) < 1.0, "the native post stands at its drawn compound")
	var target: Vector2 = view.camera.get_viewport().get_visible_rect().size * Vector2(0.75, 0.3)
	_mouse(view, MOUSE_BUTTON_RIGHT, target, true)
	_mouse(view, MOUSE_BUTTON_RIGHT, target, true, true)
	view._issue_stop_order()
	_ticks(view, 200)
	check(_post_position(view, 0).distance_to(start) < 0.5, "move and attack orders with the post selected do not move it")
	view._update_hud()
	check(String(view.command_post_notice).contains("FIXED INSTALLATION") and view.command_post_panel.describe().contains("FIXED INSTALLATION"), "the refusal is explained in the post panel")
	check(view.selected_ids.is_empty() and view.command_post_selected, "the refused order does not put the post into the unit selection")
	view._clear_selection()
	_mouse(view, MOUSE_BUTTON_RIGHT, target, true)
	_ticks(view, 200)
	check(_post_position(view, 0).distance_to(start) < 0.5, "a right-click with nothing selected does not move the post")
	check(view.selected_ids.is_empty(), "a right-click with nothing selected selects nothing")


func _test_box_select(view: Node, post: Node3D) -> void:
	view._clear_selection()
	var a := _screen(view, post, Vector3(-CommandPostModel.FOOTPRINT.x * 0.5, 0.0, -CommandPostModel.FOOTPRINT.y * 0.5))
	var b := _screen(view, post, Vector3(CommandPostModel.FOOTPRINT.x * 0.5, 0.0, CommandPostModel.FOOTPRINT.y * 0.5 + 25.0))
	var corner_a := Vector2(minf(a.x, b.x), minf(a.y, b.y)) - Vector2(20, 20)
	var corner_b := Vector2(maxf(a.x, b.x), maxf(a.y, b.y)) + Vector2(20, 20)
	_drag(view, corner_a, corner_b)
	check(view.selected_ids.has(view._player_engineer_id()), "a box over the compound selects the engineer beside it")
	check(not view.command_post_selected, "box selection never takes the command post")


func _test_enemy_post(view: Node, enemy_post: Node3D) -> void:
	view._clear_selection()
	var spot := Vector2(enemy_post.global_position.x, enemy_post.global_position.z)
	_aim(view, spot)
	var at := _screen(view, enemy_post, Vector3(CommandPostModel.HQ_CENTER.x, 3.0, CommandPostModel.HQ_CENTER.y))
	_hover(view, at)
	check(not String(view.hover_context.get("title", "")).contains("COMMAND POST"), "the undiscovered enemy post cannot be hovered")
	_click(view, at)
	check(not view.command_post_selected and view.selected_ids.is_empty(), "the undiscovered enemy post cannot be selected")
	var scout: int = view.extension.call("create_unit_with_type", spot.x + 60.0, spot.y + 60.0, 0, 0)
	check(scout > 0, "a scout is placed beside the enemy post")
	_ticks(view, 10)
	view._sync_vision()
	check(view.enemy_post_discovered and enemy_post.visible, "the scout discovers the enemy post")
	_hover(view, Vector2.ZERO)
	_hover(view, at)
	var title := String(view.hover_context.get("title", ""))
	check(title == "HOSTILE // ENEMY COMMAND POST", "the discovered enemy post can be hovered: '%s'" % title)
	check(String(view.hover_context.get("detail", "")).contains("INTEGRITY 1500 / 1500"), "hovering shows its integrity while in sight")
	_click(view, at)
	check(not view.command_post_selected and view.selected_ids.is_empty(), "the discovered enemy post is never selectable")
	check(not bool(view.command_post_panel.visible), "the enemy post never opens the command post panel")
