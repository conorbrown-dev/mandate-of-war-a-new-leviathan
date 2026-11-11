extends SceneTree
## Queued Field Engineer builds, through the real game and its input handler.
## - Shift+click queues several placements and keeps the placement mode armed.
## - Every queued placement shows a numbered footprint marker.
## - Escape ends placement but keeps the queue; selecting other units keeps it.
## - The engineer drives to each site in order and hands each to the native
##   queue_faction_structure, which alone decides acceptance.
## - A plain click replaces the queue.

var checks := 0
var failures := 0
const OUTPOST := 100


func check(condition: bool, reason: String) -> void:
	checks += 1
	if not condition:
		push_error(reason)
		failures += 1


func _initialize() -> void:
	_run.call_deferred()


func _click(view: Node, at: Vector2, shift: bool) -> void:
	for pressed in [true, false]:
		var event := InputEventMouseButton.new()
		event.button_index = MOUSE_BUTTON_LEFT
		event.pressed = pressed
		event.shift_pressed = shift
		event.position = at
		event.global_position = at
		view._unhandled_input(event)


func _key(view: Node, keycode: Key) -> void:
	var event := InputEventKey.new()
	event.keycode = keycode
	event.pressed = true
	view._unhandled_input(event)


# Screen points whose ground the native rules accept for an outpost, far
# enough apart that their footprints do not overlap.
func _valid_sites(view: Node, wanted: int) -> Array[Vector2]:
	var engineer: Vector2 = view._native_unit_position(view._player_engineer_id())
	var candidates: Array = []
	var viewport_size: Vector2 = view.get_viewport().get_visible_rect().size
	for row in range(12):
		for column in range(1, 16):
			var screen := Vector2(viewport_size.x * column / 16.0, viewport_size.y * (0.2 + row * 0.05))
			if view._blueprint_at_screen(screen) >= 0:
				continue
			var ground: Vector2 = view._screen_to_world(screen)
			var site: Dictionary = view.extension.call("get_structure_site", OUTPOST - 100, ground.x, ground.y)
			if bool(site.get("buildable", false)) and not bool(site.get("earthworks", true)):
				candidates.append([ground.distance_to(engineer), screen, ground])
	# The nearest sites keep the engineer's drive short.
	candidates.sort_custom(func(a, b): return a[0] < b[0])
	var screens: Array[Vector2] = []
	var grounds: Array[Vector2] = []
	for candidate in candidates:
		var clear: bool = candidate[0] > 350.0
		for other in grounds:
			# Native site claims include a 125 m navigation-cell margin around
			# each 60 m outpost; 270 m separates even diagonal placements.
			clear = clear and other.distance_to(candidate[2]) > 270.0
		if clear:
			screens.append(candidate[1])
			grounds.append(candidate[2])
			if screens.size() == wanted:
				break
	return screens


func _run() -> void:
	var view: Node = load("res://main.tscn").instantiate()
	root.add_child(view)
	await process_frame
	await process_frame
	view._on_start_skirmish_pressed()
	var engineer_id: int = view._player_engineer_id()
	view._set_selected(engineer_id, true)
	view.target_camera_distance = 900.0
	view.camera_distance = 900.0
	for _frame in range(4):
		await process_frame
	var commander_id := int(view.commander_ids.get(0, -1))
	var funds: Array = view.extension.call("economy_get_tactical_resources", commander_id)
	check(float(funds[0]) >= 900.0, "the start has Materials for three outposts (%.0f)" % float(funds[0]))
	var sites := _valid_sites(view, 3)
	check(sites.size() == 3, "three separate buildable outpost sites are on screen")
	if sites.size() < 3:
		_finish(view)
		return

	view._begin_build_placement(OUTPOST)
	for site in sites:
		_click(view, site, true)
	check(view.build_mode_type == OUTPOST, "Shift+click keeps the placement mode armed for the next site")
	check(not view.pending_build_order.is_empty() and view.queued_build_orders.size() == 2, "the first placement goes to the engineer and two wait in the queue")
	if view.pending_build_order.is_empty() or view.queued_build_orders.size() < 2:
		_finish(view)
		return
	var expected: Array[Vector2] = [view.pending_build_order.target, view.queued_build_orders[0].target, view.queued_build_orders[1].target]
	for index in range(3):
		var ground: Vector2 = view._screen_to_world(sites[index])
		check(expected[index].distance_to(ground) < 1.0, "placement %d keeps its clicked site and its order" % (index + 1))
	view.build_ghost = view._make_build_ghost(OUTPOST)
	view.add_child(view.build_ghost)
	check(view._resolve_build_target(OUTPOST, expected[0]) == Vector2.INF, "pending engineer site makes the ghost invalid")
	check(view._resolve_build_target(OUTPOST, expected[1]) == Vector2.INF, "waiting queued site makes the ghost invalid")
	view._set_build_ghost_valid(false)
	check(view.build_ghost_material.get_shader_parameter("tint") == Color("#ff334d", 0.50), "occupied ghost uses the red invalid tint")
	var voice_before: int = view.ui_audio._next_voice
	_click(view, sites[1], true)
	check(view.ui_audio._next_voice == (voice_before + 1) % view.ui_audio.VOICE_LIMIT and view.queued_build_orders.size() == 2,
		"clicking a red queued site plays refusal and keeps the queue")
	voice_before = view.ui_audio._next_voice
	_click(view, sites[1], false)
	check(view.ui_audio._next_voice == (voice_before + 1) % view.ui_audio.VOICE_LIMIT and view.queued_build_orders.size() == 2 and view.build_mode_type == OUTPOST,
		"plain click on a red site plays refusal without clearing the queue or mode")
	var markers: Node3D = view.queued_build_markers
	check(markers != null and markers.get_child_count() == 3, "every queued placement shows a marker")
	for index in range(3):
		var marker := markers.get_node_or_null("QueuedBuild_%d" % (index + 1)) as Node3D if markers != null else null
		var label := marker.get_node_or_null("Order") as Label3D if marker != null else null
		check(marker != null and Vector2(marker.position.x, marker.position.z).distance_to(expected[index]) < 0.5 and label != null and label.text.begins_with(str(index + 1)), "marker %d stands on its site and shows its place in the queue" % (index + 1))
		check(label != null and label.font_size * label.pixel_size <= 4.0 and label.position.y <= 10.0,
			"queued build label stays a compact ground marker")

	_key(view, KEY_ESCAPE)
	check(view.build_mode_type == -1 and view.build_ghost == null, "Escape ends placement")
	check(not view.pending_build_order.is_empty() and view.queued_build_orders.size() == 2 and markers.get_child_count() == 3, "Escape keeps the orders already queued")
	view._clear_selection()
	check(not view.selected_ids.has(engineer_id), "the player selects something else while the engineer drives")

	# Drive the native simulation; the engineer hands each site over in order.
	var handed: Array[Vector2] = []
	var native_accepted := 0
	var queue_lengths: Array[int] = []
	for _tick in range(12000):
		var before: int = view.pending_completed_structures.size()
		view.extension.call("update_simulation", 50.0)
		view._process_pending_build_order()
		if view.pending_completed_structures.size() > before:
			var record: Dictionary = view.pending_completed_structures.back()
			handed.append(record.target)
			native_accepted += 1
			queue_lengths.append(view.queued_build_orders.size())
		if view.pending_build_order.is_empty() and view.queued_build_orders.is_empty():
			break
	check(handed.size() == 3, "the engineer reached all three sites and the native queue accepted each (%d)" % handed.size())
	for index in range(mini(handed.size(), 3)):
		check(handed[index].distance_to(expected[index]) < 0.5, "site %d was handed over in the order it was queued" % (index + 1))
	check(queue_lengths == [1, 0, 0], "each site waits for the one before it (%s)" % [queue_lengths])
	for _tick in range(1800):
		var jobs: Array = view.extension.call("get_hud_state", 0, commander_id, -1, 0.0, 0.0, false).get("production_queue", [])
		if jobs.is_empty():
			break
		view.extension.call("update_simulation", 50.0)
	view._update_hud()
	var native_jobs := 0
	for job in view.extension.call("get_hud_state", 0, commander_id, -1, 0.0, 0.0, false).get("production_queue", []):
		native_jobs += 1 if String(job.get("name", "")).to_upper().contains("OUTPOST") else 0
	check(native_jobs + _built_outposts(view) >= 3, "the native production line holds or has finished three outposts")
	var completed_site: Dictionary = view.extension.call("get_structure_site", OUTPOST - 100, expected[0].x, expected[0].y)
	check(bool(completed_site.get("claimed", false)) and view._resolve_build_target(OUTPOST, expected[0]) == Vector2.INF,
		"native structure site stays red after handoff and construction")
	check(markers.get_child_count() == 0, "markers clear once every site is handed over")

	# A plain click replaces whatever is queued.
	view._set_selected(engineer_id, true)
	view._begin_build_placement(OUTPOST)
	var replacement_sites := _valid_sites(view, 3)
	check(replacement_sites.size() == 3, "fresh unbuilt outpost sites remain available")
	if replacement_sites.size() < 3:
		_finish(view)
		return
	_click(view, replacement_sites[0], true)
	_click(view, replacement_sites[1], true)
	check(view.queued_build_orders.size() == 1, "a new Shift queue starts")
	_click(view, replacement_sites[2], false)
	check(view.queued_build_orders.is_empty() and not view.pending_build_order.is_empty() and (view.pending_build_order.target as Vector2).distance_to(view._screen_to_world(replacement_sites[2])) < 1.0, "a plain click replaces the queue with its own site")
	check(view.build_mode_type == -1 and markers.get_child_count() == 1, "a plain click ends placement and leaves one marker")
	_finish(view)


func _built_outposts(view: Node) -> int:
	var count := 0
	for structure in view.completed_structure_views:
		if is_instance_valid(structure) and int(structure.get_meta("structure_type", -1)) == 0:
			count += 1
	return count


func _finish(view: Node) -> void:
	view.queue_free()
	await process_frame
	print("QUEUED_BUILDS checks=%d failures=%d" % [checks, failures])
	quit(1 if failures else 0)
