extends SceneTree

const UiIconRegistryScript = preload("res://ui/icons/ui_icon_registry.gd")

var checks := 0
var failures := 0


func check(condition: bool, message: String) -> void:
	checks += 1
	if not condition:
		failures += 1
		push_error(message)


func _initialize() -> void:
	var move_path := UiIconRegistryScript.get_icon_path(&"order.move")
	var material_path := UiIconRegistryScript.get_icon_path(&"resource.material")
	var fighter_path := UiIconRegistryScript.get_nato_symbol_path(&"fighter", &"friendly")
	var hostile_fighter_path := UiIconRegistryScript.get_nato_symbol_path(&"fighter", &"hostile")
	check(move_path == "res://assets/ui/icons/orders/move.svg", "semantic command icon resolves its generated Godot path")
	check(material_path == "res://assets/ui/icons/resources/material.svg", "semantic resource icon resolves its generated Godot path")
	check(fighter_path == "res://assets/ui/symbols/nato/friendly/air/fighter.svg", "friendly NATO symbol resolves its generated Godot path")
	check(hostile_fighter_path == "res://assets/ui/symbols/nato/hostile/air/fighter.svg", "NATO affiliation selects a distinct generated symbol")
	check(UiIconRegistryScript.get_icon(&"order.move") != null, "command icon is imported as a drawable Godot texture")
	check(UiIconRegistryScript.get_nato_symbol(&"fighter", &"friendly") != null, "NATO symbol is imported as a drawable Godot texture")
	for icon_id in [&"order.stop", &"combat.attack", &"construction.build", &"construction.demolish", &"resource.energy", &"resource.research"]:
		check(UiIconRegistryScript.get_icon(icon_id) != null, "HUD semantic icon loads as a drawable texture: %s" % icon_id)
	check(UiIconRegistryScript.get_nato_symbol(&"medium_armor", &"friendly") != null, "NATO land symbol is imported as a drawable Godot texture")
	check(UiIconRegistryScript.get_nato_symbol(&"patrol_boat", &"friendly") != null, "NATO naval symbol is imported as a drawable Godot texture")
	check(UiIconRegistryScript.get_icon(&"order.move") == UiIconRegistryScript.get_icon(&"order.move"), "repeated semantic lookup returns the cached command texture")
	check(UiIconRegistryScript.get_icon_path(&"missing.icon").is_empty(), "missing command IDs degrade to an empty path")
	check(UiIconRegistryScript.get_nato_symbol_path(&"fighter", &"unsupported").is_empty(), "missing affiliations degrade to an empty path")
	check(UiIconRegistryScript.get_index_errors().is_empty(), "generated command and NATO indexes parse without registry errors")
	var duplicate_errors := UiIconRegistryScript.validate_nato_entries([
		{"id": "fighter", "affiliation": "friendly"},
		{"id": "fighter", "affiliation": "friendly"},
	])
	check(duplicate_errors.size() == 1 and duplicate_errors[0].contains("Duplicate"), "duplicate semantic NATO IDs are rejected instead of silently overwriting")
	if failures > 0:
		quit(1)
		return
	print("UI_ICON_REGISTRY checks=%d failures=0" % checks)
	quit(0)
