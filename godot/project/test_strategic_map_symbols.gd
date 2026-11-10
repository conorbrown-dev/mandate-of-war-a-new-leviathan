extends SceneTree

## Map-scale APP-6 unit symbols and resource-site markers, asserted through the
## real match scene: what StrategicIconOverlay._draw put on screen.

const UnitCatalog := preload("res://unit_catalog.gd")
const NatoMapSymbol := preload("res://nato_map_symbol.gd")
const UiIconRegistryScript := preload("res://ui/icons/ui_icon_registry.gd")

# The APP-6 function each unit must show, by unit name (pins the data fix-ups).
const EXPECTED_FUNCTION := {
	"Elite MBT": "armour", "Swarm Tank": "armour", "Assault Vehicle": "armour", "Industrial MBT": "armour",
	"Elite Artillery": "sp_artillery",
	"Elite AA Vehicle": "air_defense", "Mass AA Vehicle": "air_defense",
	"Missile Platform": "missile",
	"Engineering Unit": "engineer",
	"Pathfinder Scout Car": "recon", "Outrider Scout Bike": "recon",
	"Kestrel T1 Fighter": "fighter",
	"Harrierwing T1 VTOL": "attack_helicopter",
	"Longsight Recon Plane": "recon_aircraft", "Farwatch Recon Plane": "recon_aircraft",
	"Skybridge Airlifter": "cargo_aircraft", "Packhorse Airlifter": "cargo_aircraft", "Hauler Airlifter": "cargo_aircraft",
	"Breakwater Patrol Boat": "naval_surface",
}
# Build-catalog symbol (UI icon registry id) that agrees with each map function.
const CATALOG_SYMBOL := {
	"armour": ["medium_armor", "heavy_armor", "light_armor"],
	"sp_artillery": ["self_propelled_artillery"],
	"air_defense": ["air_defense", "short_range_air_defense", "long_range_sam"],
	"missile": ["rocket_artillery"],
	"engineer": ["combat_engineer", "engineer"],
	"recon": ["recon", "scout"],
	"fighter": ["fighter", "interceptor", "multirole_fighter"],
	"attack_helicopter": ["attack_helicopter"],
	"recon_aircraft": ["recon_aircraft"],
	"cargo_aircraft": ["transport_aircraft"],
	"naval_surface": ["patrol_boat"],
}
const FRAME := {
	"land": {"friendly": "rectangle", "hostile": "diamond"},
	"air": {"friendly": "air_dome", "hostile": "air_arch"},
	"sea": {"friendly": "circle", "hostile": "diamond"},
}

var checks := 0
var failures := 0


func check(condition: bool, message: String) -> void:
	checks += 1
	if not condition:
		failures += 1
		push_error("VALIDATION FAIL: " + message)


func _initialize() -> void:
	_run.call_deferred()


func _redraw(view: Node3D) -> void:
	view.strategic_icon_overlay.queue_redraw()
	await process_frame
	await process_frame


func _look_at(view: Node3D, x: float, z: float, distance: float) -> void:
	view.camera_distance = distance
	view.target_camera_distance = distance
	view.camera_target = Vector3(x, view._terrain_height_at(x, z), z)
	view._update_camera(1.0)


func _run() -> void:
	var view: Node3D = load("res://main.tscn").instantiate()
	root.add_child(view)
	await process_frame
	view._on_start_skirmish_pressed()
	view.set_process(false)
	var extension: Object = view.extension
	var overlay: Control = view.strategic_icon_overlay
	var everywhere := Rect2(-1.0e7, -1.0e7, 2.0e7, 2.0e7)

	# --- Resource markers: absent until scouted, then remembered. ---
	_look_at(view, -13000.0, -3000.0, 26000.0)
	var markers: Array = overlay.resource_markers(view.camera, everywhere)
	var by_id := {}
	for marker: Dictionary in markers:
		by_id[int(marker.id)] = marker
	for site: Dictionary in view.material_site_state.values():
		var visible := bool(extension.call("can_see", 0, site.position.x, site.position.y))
		check(bool(site.discovered) == visible and by_id.has(int(site.id)) == visible,
			"Materials site %d is marked only when scouted" % int(site.id))
	var civilian := 0
	for site: Dictionary in view.fuel_depot_state.values():
		if String(site.kind) == "civilian":
			civilian += 1
		var visible := bool(extension.call("can_see", 0, site.position.x, site.position.y))
		check(bool(site.discovered) == visible and by_id.has(int(site.id)) == visible,
			"fuel depot %d is marked only when scouted" % int(site.id))
	check(civilian == 4 and view.fuel_depot_state.size() == 6, "fixture has four civilian terminals and two military depots")
	var polymer: Dictionary = view.material_site_state[902]
	check(int(polymer.owner) == 1 and not bool(extension.call("can_see", 0, -17500.0, -8500.0)), "enemy-held Materials site starts out of sight")
	check(not by_id.has(902) and view.resource_sites.get_node_or_null("MaterialFacility_902") == null, "unscouted enemy-held Materials site has no marker or world model")

	await _redraw(view)
	var drawn_ids := {}
	for marker: Dictionary in overlay.last_resource_markers:
		drawn_ids[int(marker.id)] = marker
	check(drawn_ids.size() == by_id.size(), "full-map zoom draws exactly the scouted resource markers")

	# Close zoom: the nearest site keeps its screen-space marker.
	var rare: Dictionary = view.material_site_state[900]
	_look_at(view, rare.position.x, rare.position.y, 60.0)
	await _redraw(view)
	var close_ids: Array = overlay.last_resource_markers.map(func(marker: Dictionary) -> int: return int(marker.id))
	check(view.camera_distance < overlay.STRATEGIC_ZOOM_DISTANCE and not close_ids.has(900), "unscouted Materials site is hidden at close tactical zoom")
	var rare_scout := int(extension.call("create_unit_with_type", float(rare.position.x) + 20.0, float(rare.position.y), 13, 0))
	view._sync_new_entities()
	for _tick in range(10):
		extension.call("update_simulation", 50.0)
	view._sync_vision()
	check(rare_scout > 0 and bool(rare.discovered) and view.resource_sites.get_node_or_null("MaterialFacility_900") != null,
		"scouting reveals the Materials site and its world model")
	await _redraw(view)
	close_ids = overlay.last_resource_markers.map(func(marker: Dictionary) -> int: return int(marker.id))
	check(close_ids.has(900), "scouted Materials site is marked at close tactical zoom")
	_look_at(view, rare.position.x, rare.position.y, 1500.0)
	await _redraw(view)
	close_ids = overlay.last_resource_markers.map(func(marker: Dictionary) -> int: return int(marker.id))
	check(close_ids.has(900), "a Materials site is marked at strategic zoom")
	extension.call("destroy_unit", rare_scout)
	for _tick in range(10):
		extension.call("update_simulation", 50.0)
	view._sync_vision()
	var rare_remembered: bool = overlay.resource_markers(view.camera, everywhere).any(func(marker: Dictionary) -> bool: return int(marker.id) == 900)
	check(not bool(extension.call("can_see", 0, rare.position.x, rare.position.y)) and rare_remembered,
		"Materials site remains on the map after its scout leaves")

	# True owner changes out of sight do not reveal a site.
	extension.call("economy_assign_material_site", 903, 0)
	view._sync_fuel_supply()
	var composites: Dictionary = view.material_site_state[903]
	check(int(composites.owner) == 0, "fixture moved the unseen site to the player in the simulation")
	var after: Dictionary = {}
	for marker: Dictionary in overlay.resource_markers(view.camera, everywhere):
		after[int(marker.id)] = marker
	check(not after.has(903), "an owner change nobody saw does not reveal the Materials site")

	# Discovery: a scout at the military depot reveals it and its owner.
	var depot: Dictionary = view.fuel_depot_state[920]
	var scout := int(extension.call("create_unit_with_type", float(depot.position.x) + 20.0, float(depot.position.y), 13, 0))
	view._sync_new_entities()
	for _tick in range(20):
		extension.call("update_simulation", 50.0)
	view._sync_unit_transforms()
	view._sync_vision()
	check(scout > 0 and bool(depot.discovered) and int(depot.known_owner) == 1, "a friendly unit at the enemy depot discovers it")
	var found := false
	for marker: Dictionary in overlay.resource_markers(view.camera, everywhere):
		if int(marker.id) == 920:
			found = marker.kind == "fuel" and marker.affiliation == "hostile" and marker.site_kind == "military"
	check(found, "the discovered enemy depot is marked as hostile military fuel")
	check(not overlay.resource_markers(view.camera, everywhere).any(func(marker: Dictionary) -> bool: return int(marker.id) == 921), "the other enemy depot stays unmarked until found")
	extension.call("destroy_unit", scout)
	for _tick in range(10):
		extension.call("update_simulation", 50.0)
	view._sync_vision()
	var fuel_remembered: bool = overlay.resource_markers(view.camera, everywhere).any(func(marker: Dictionary) -> bool: return int(marker.id) == 920)
	check(not bool(extension.call("can_see", 0, depot.position.x, depot.position.y)) and fuel_remembered,
		"fuel depot remains marked after its scout leaves")

	# --- Unit symbols: frame per affiliation and dimension, function per unit. ---
	var origin := Vector2(-12500.0, 1000.0)
	var spawned := {}
	var column := 0
	for unit_type: int in UnitCatalog.units().keys():
		var unit: Dictionary = UnitCatalog.units()[unit_type]
		var function := NatoMapSymbol.function_for(unit_type)
		check(EXPECTED_FUNCTION.get(String(unit.name), "?") == function, "%s shows APP-6 function %s (got %s)" % [unit.name, EXPECTED_FUNCTION.get(String(unit.name), "?"), function])
		var dimension := NatoMapSymbol.dimension_for(unit_type)
		var expected_dimension := "air" if bool(unit.get("is_aircraft", false)) else ("sea" if bool(unit.get("is_naval", false)) else "land")
		check(dimension == expected_dimension, "%s is drawn in the %s dimension" % [unit.name, expected_dimension])
		var catalog_symbol := String(unit.get("nato_symbol", ""))
		check(CATALOG_SYMBOL.get(function, []).has(catalog_symbol), "%s build-catalog symbol %s agrees with %s" % [unit.name, catalog_symbol, function])
		for affiliation in ["friendly", "hostile"]:
			check(not UiIconRegistryScript.get_nato_symbol_path(StringName(catalog_symbol), StringName(affiliation)).is_empty(), "%s catalog symbol resolves for %s" % [unit.name, affiliation])
		for owner in [0, 1]:
			var x := origin.x + float(column) * 45.0
			var z := origin.y + float(owner) * 60.0
			var id := int(extension.call("create_unit_with_type", x, z, unit_type, owner))
			if id > 0:
				spawned[id] = {"type": unit_type, "owner": owner, "dimension": dimension, "function": function}
		column += 1
	view._sync_new_entities()
	for _tick in range(20):
		extension.call("update_simulation", 50.0)
	view._sync_unit_transforms()
	view._sync_vision()
	_look_at(view, origin.x + float(column) * 22.0, origin.y + 30.0, 1300.0)
	view._sync_unit_transforms()
	await _redraw(view)
	var drawn := {}
	for symbol: Dictionary in overlay.last_unit_symbols:
		drawn[int(symbol.entity_id)] = symbol
	var hostile_seen := 0
	for id: int in spawned.keys():
		var spec: Dictionary = spawned[id]
		var affiliation := "hostile" if int(spec.owner) == 1 else "friendly"
		if not view._is_unit_shown(id):
			check(affiliation == "hostile", "own unit %d is always shown" % id)
			continue
		if affiliation == "hostile":
			hostile_seen += 1
		check(drawn.has(id), "unit %d (%s, %s) has a map symbol" % [id, spec.function, affiliation])
		if not drawn.has(id):
			continue
		var symbol: Dictionary = drawn[id]
		check(symbol.affiliation == affiliation, "unit %d affiliation is %s" % [id, affiliation])
		check(symbol.frame == FRAME[spec.dimension][affiliation], "%s %s uses the %s frame (got %s)" % [affiliation, spec.function, FRAME[spec.dimension][affiliation], symbol.frame])
		check(symbol.function == spec.function, "unit %d draws the %s function icon" % [id, spec.function])
		check(symbol.fill == (NatoMapSymbol.HOSTILE_FILL if affiliation == "hostile" else NatoMapSymbol.FRIENDLY_FILL), "unit %d uses the APP-6 %s fill" % [id, affiliation])
	check(hostile_seen >= 10, "most hostile fixtures are in sight and get hostile symbols (%d)" % hostile_seen)
	for symbol: Dictionary in overlay.last_unit_symbols:
		if not spawned.has(int(symbol.entity_id)):
			check(not view.ai_entity_ids.has(int(symbol.entity_id)) or view.seen_enemy_ids.has(int(symbol.entity_id)), "no unseen enemy gets a symbol")
	print("STRATEGIC_MAP_SYMBOLS checks=%d failures=%d" % [checks, failures])
	view.queue_free()
	await process_frame
	quit(1 if failures > 0 else 0)
