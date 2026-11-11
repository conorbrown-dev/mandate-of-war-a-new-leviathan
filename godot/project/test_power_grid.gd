extends SceneTree
## Contract for the power grid on Broken Strait: the scenario's stations and
## lines reach the simulation, the right model stands on each kind of line, the
## enemy's grid stays hidden until scouted, and cutting, repairing and
## capturing a line change the player's Power state and what is drawn.

const PLAYER := 0
const ENEMY := 1
const PLAYER_FEED := 102
const ENEMY_FEED := 107
const SITE_FEED := 109
const STRAIT_TRUNK := 103

var checks := 0
var failures := 0


func check(condition: bool, reason: String) -> void:
	checks += 1
	if not condition:
		failures += 1
		push_error(reason)


func _initialize() -> void:
	_run.call_deferred()


func _tick(native: Object, count: int) -> void:
	for _tick_index in range(count):
		native.call("update_simulation", 50.0)


func _line_state(native: Object, id: int) -> Dictionary:
	for line: Dictionary in native.call("power_grid_state", PLAYER).get("lines", []):
		if int(line.id) == id:
			return line
	return {}


func _power(native: Object, post: int) -> Array:
	var resources: Array = native.call("economy_get_tactical_resources", post)
	return [float(resources[4]), float(resources[5]), float(resources[6])]


func _run() -> void:
	var view: Node3D = load("res://main.tscn").instantiate()
	root.add_child(view)
	await process_frame
	view._on_start_skirmish_pressed()
	view.set_process(false)
	await process_frame
	var native: Object = view.extension
	native.call("skirmish_set_ai_enabled", false)
	var grid: Node3D = view.power_grid_view
	check(grid != null and grid.active, "Broken Strait has a power grid")
	if grid == null or not grid.active:
		print("POWER_GRID checks=%d failures=%d" % [checks, failures])
		quit(1)
		return
	_tick(native, 2)
	view._sync_power_grid()
	var post := int(view.commander_ids.get(PLAYER, -1))
	var state: Dictionary = native.call("power_grid_state", PLAYER)
	var stations := 0
	for node: Dictionary in state.get("nodes", []):
		stations += 1 if int(node.kind) == 0 else 0
	check(stations == 3 and (state.get("lines", []) as Array).size() == 10, "the scenario's three stations and ten lines reach the simulation")
	var power := _power(native, post)
	check(is_equal_approx(power[0], 320.0) and is_equal_approx(power[1], 40.0),
		"the player's post draws 40 MW from the two stations joined to it (320 MW)")
	check(String(view._hud_power(native.call("economy_get_tactical_resources", post))) == "320 MW / 40 MW", "the HUD reports the grid's supply")

	# Which model stands where: steel for stations, cities and the strait; wood for posts and small works.
	var wood_mesh: Mesh = grid._mesh("wooden_power_pole")
	var steel_mesh: Mesh = grid._mesh("steel_pylon")
	var station_mesh: Mesh = grid._mesh("power_station")
	check(wood_mesh != null and steel_mesh != null and station_mesh != null, "the three power models load")
	check(wood_mesh != steel_mesh and wood_mesh != null and steel_mesh != null and
		steel_mesh.get_aabb().size.y > wood_mesh.get_aabb().size.y * 3.0, "a steel pylon is a much larger model than a wooden pole")
	var steel_rule := true
	var wood_rule := true
	var crosses_water := false
	for line: Dictionary in grid.lines.values():
		var touches_station: bool = int(grid.nodes[int(line.from)].kind) == 0 or int(grid.nodes[int(line.to)].kind) == 0
		var over_water := false
		for point: Vector2 in line.supports:
			over_water = over_water or view._terrain_height_at(point.x, point.y) < 0.0
		if int(line.id) == STRAIT_TRUNK:
			crosses_water = over_water
		if (touches_station or over_water) and int(line.kind) != 1:
			steel_rule = false
		var to_post: bool = int(grid.nodes[int(line.from)].kind) == 2 or int(grid.nodes[int(line.to)].kind) == 2
		if to_post and int(line.kind) != 0:
			wood_rule = false
	check(steel_rule, "every line that leaves a station or crosses water is steel")
	check(crosses_water, "the strait trunk really stands in the water")
	check(wood_rule, "the lines to the command posts are wooden")
	var feed: Dictionary = grid.lines[PLAYER_FEED]
	var road: Dictionary = native.call("transport_route", PLAYER, 1)
	var road_entry: Vector2 = road.get("entry", Vector2.ZERO)
	var road_terminal: Vector2 = road.get("terminal", Vector2.ZERO)
	var beside_road := 0
	for point: Vector2 in feed.supports:
		var nearest := Geometry2D.get_closest_point_to_segment(point, road_entry, road_terminal)
		if point.distance_to(nearest) <= 40.0:
			beside_road += 1
	check(beside_road >= 40, "the player's wooden line runs alongside the supply road (%d poles beside it)" % beside_road)

	# What is drawn at the start: the player's own supply, nothing of the enemy's.
	var feed_view: Node3D = feed.view
	check(feed_view != null and feed_view.get_node_or_null("Supports") != null, "the player's own line is drawn from the start")
	var supports := feed_view.get_node("Supports") as MultiMeshInstance3D if feed_view != null else null
	check(supports != null and supports.multimesh.mesh == wood_mesh and
		supports.multimesh.instance_count == (feed.supports as Array).size() and supports.multimesh.instance_count > 40,
		"it stands on wooden poles, one per span")
	var intact_wire_vertices := int(feed_view.get_meta("wire_vertices", 0)) if feed_view != null else 0
	check(intact_wire_vertices > 0 and feed_view.get_node_or_null("Conductors") != null, "conductors hang between the poles")
	var harbor: Dictionary = grid.nodes[1]
	check(harbor.view != null and (harbor.view as Node3D).get_node_or_null("Model") != null and
		((harbor.view as Node3D).get_node("Model") as MeshInstance3D).mesh == station_mesh, "the station that feeds the player is drawn with the station model")
	var trunk_view: Node3D = grid.lines[101].view
	check(trunk_view != null and (trunk_view.get_node("Supports") as MultiMeshInstance3D).multimesh.mesh == steel_mesh,
		"the trunk from the station stands on steel pylons")
	check(grid.lines[ENEMY_FEED].view == null and grid.nodes[2].view == null and not bool(grid.lines[ENEMY_FEED].discovered),
		"the enemy's line and station are not drawn before they are scouted")
	var overlay: Control = view.strategic_icon_overlay
	var everywhere := Rect2(-100000, -100000, 200000, 200000)
	view.camera_distance = 26000.0
	view.target_camera_distance = 26000.0
	view._update_camera(1.0)
	var markers: Array = overlay.power_markers(view.camera, everywhere)
	check(markers.size() == 1 and int(markers[0].id) == 1 and String(markers[0].affiliation) == "unclaimed",
		"the map marks only the known station, as civilian")
	var hover: Dictionary = grid.hover_context(Vector2(-12125, 2000), 20.0)
	check(String(hover.get("title", "")) == "FRIENDLY // WOODEN POWER LINE", "hovering the line names it and its holder")

	# Scouting shows the enemy's line, with its holder.
	var scout := int(native.call("create_unit_with_type", -17975.0, -9000.0, 13, PLAYER))
	_tick(native, 20)
	view._sync_vision()
	check(scout > 0 and bool(grid.lines[ENEMY_FEED].discovered) and grid.lines[ENEMY_FEED].view != null and
		int(grid.lines[ENEMY_FEED].known_owner) == ENEMY, "a scout reveals the enemy's power line and who holds it")
	native.call("destroy_unit", scout)

	# The enemy cuts the player's line.
	var raider := int(native.call("create_unit_with_type", -12300.0, 2000.0, 3, ENEMY))
	check(raider > 0 and String(native.call("power_order", raider, ENEMY, 3, PLAYER_FEED)).is_empty(), "an enemy unit is sent to cut the player's line")
	check(not String(native.call("power_order", raider, PLAYER, 3, PLAYER_FEED)).is_empty(), "a side cannot order another side's unit")
	_tick(native, 1)
	check(bool(_line_state(native, PLAYER_FEED).intact), "the line is not cut by the order alone")
	for _attempt in range(1200):
		if not bool(_line_state(native, PLAYER_FEED).intact):
			break
		_tick(native, 1)
	_tick(native, 1)
	check(not bool(_line_state(native, PLAYER_FEED).intact), "the line is cut when the enemy reaches it")
	power = _power(native, post)
	check(is_zero_approx(power[0]) and power[2] < 0.0, "the player loses all power income and falls into deficit")
	var bursts_before := int(grid.spark_bursts)
	view._sync_power_grid()
	feed_view = feed.view
	var sparks := feed.sparks as CPUParticles3D
	check(not bool(feed.known_intact) and feed_view != null and not bool(feed_view.get_meta("intact", true)), "the player is told at once and the line is drawn broken")
	check(sparks != null and sparks.emitting and int(grid.spark_bursts) == bursts_before + 1 and
		sparks.mesh != null and sparks.color_ramp != null and sparks.one_shot, "sparks burst from the break")
	var cut: Vector2 = feed.known_cut
	check(sparks != null and Vector2(sparks.position.x, sparks.position.z).distance_to(cut) < 1.0 and
		grid.distance_to_line(feed, cut) < 1.0, "the sparks are at the break, on the line")
	check(int(feed_view.get_meta("wire_vertices", 0)) < intact_wire_vertices, "the broken line has lost conductors")
	check(String(view.fuel_supply_status) == "POWER LINE CUT // ENGINEER MUST REPAIR IT", "the HUD reports the cut")
	native.call("destroy_unit", raider)
	_tick(native, 2)

	# Only the engineer repairs it, for Materials.
	var engineer := int(view._player_engineer_id())
	var tank := int(native.call("create_unit_with_type", -12300.0, 1500.0, 0, PLAYER))
	check(String(native.call("power_order", tank, PLAYER, 4, PLAYER_FEED)) == "ENGINEER REQUIRED TO REPAIR POWER LINES", "a tank is refused the repair")
	var materials_before := float(native.call("economy_get_tactical_resources", post)[0])
	var cost := float(_line_state(native, PLAYER_FEED).repair_cost)
	view.material_order_mode = 1
	var result: Dictionary = grid.order_at(cut, 0.0, 1, tank, engineer)
	check(bool(result.handled) and String(result.message).begins_with("REPAIRING POWER LINE"), "claim mode on a dead line sends the engineer to repair it")
	_tick(native, 2)
	check(is_equal_approx(float(native.call("economy_get_tactical_resources", post)[0]), materials_before), "nothing is paid until the engineer arrives")
	for _attempt in range(12000):
		if bool(_line_state(native, PLAYER_FEED).repairing):
			break
		_tick(native, 1)
	check(bool(_line_state(native, PLAYER_FEED).repairing), "the repair starts when the engineer reaches the line")
	check(cost > 0.0 and is_equal_approx(float(native.call("economy_get_tactical_resources", post)[0]), materials_before - cost),
		"the repair costs %.0f Materials" % cost)
	view._sync_power_grid()
	check(feed.label != null and String((feed.label as Label3D).text).begins_with("REPAIRING"), "the break shows the repair under way")
	_tick(native, int(float(_line_state(native, PLAYER_FEED).repair_seconds) * 20.0) + 5)
	view._sync_power_grid()
	power = _power(native, post)
	check(bool(_line_state(native, PLAYER_FEED).intact) and is_equal_approx(power[0], 320.0) and power[2] > 0.0, "the repaired line restores the player's power income")
	check(bool(feed.known_intact) and feed.sparks == null and int((feed.view as Node3D).get_meta("wire_vertices", 0)) == intact_wire_vertices,
		"the line is drawn whole again, without sparks")

	# Any ground unit captures a line.
	check(int(grid.lines[SITE_FEED].owner) == -1 and grid.lines[SITE_FEED].view == null, "the works feeder starts civilian and unseen")
	var captor := int(native.call("create_unit_with_type", -12800.0, 4175.0, 0, PLAYER))
	_tick(native, 20)
	view._sync_vision()
	check(captor > 0 and bool(grid.lines[SITE_FEED].discovered) and grid.lines[SITE_FEED].view != null and
		((grid.lines[SITE_FEED].view as Node3D).get_node("Supports") as MultiMeshInstance3D).multimesh.mesh == wood_mesh,
		"a civilian line is drawn, on wooden poles, once a unit sees it")
	tank = captor
	result = grid.order_at(Vector2(-12800, 4170), 0.0, 1, tank, engineer)
	check(bool(result.handled) and String(result.message) == "CAPTURING POWER LINE", "claim mode on a live line sends the selected unit to capture it")
	for _attempt in range(6000):
		if int(_line_state(native, SITE_FEED).owner) == PLAYER:
			break
		_tick(native, 1)
	view._sync_power_grid()
	check(int(_line_state(native, SITE_FEED).owner) == PLAYER and int(grid.lines[SITE_FEED].known_owner) == PLAYER,
		"the line changes hands when the unit arrives")
	check(String(native.call("power_order", tank, PLAYER, 2, SITE_FEED)) == "POWER LINE ALREADY HELD", "a held line cannot be captured again")
	view.queue_free()
	await process_frame
	print("POWER_GRID checks=%d failures=%d" % [checks, failures])
	quit(1 if failures else 0)
