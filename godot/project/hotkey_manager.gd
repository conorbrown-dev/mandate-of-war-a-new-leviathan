extends Node

var _hotkeys: Dictionary = {}
var _enabled: bool = true

func _ready() -> void:
	_load_default_hotkeys()

## Every control the player has, in the order the F1 hotkey sheet shows them.
## "key" and "action" drive main.gd's _handle_hotkey; entries with no action are
## handled elsewhere in main.gd and are listed here so the sheet is complete.
## "label" is what the sheet shows and "category" groups it.
const CATEGORY_ORDER := ["Camera", "Selection and orders", "Building", "Logistics", "Interface"]

var _order: Array[String] = []


func _load_default_hotkeys() -> void:
	_hotkeys.clear()
	_order.clear()

	var default_hotkeys = [
		{"key": "KEY_WASD", "label": "W A S D", "category": "Camera", "action": "", "description": "Pan camera (arrow keys too)"},
		{"key": "KEY_QE", "label": "Q / E", "category": "Camera", "action": "", "description": "Rotate camera"},
		{"key": "KEY_WHEEL_UP", "label": "Wheel up", "category": "Camera", "action": "zoom_in", "description": "Zoom in toward the cursor"},
		{"key": "KEY_WHEEL_DOWN", "label": "Wheel down", "category": "Camera", "action": "zoom_out", "description": "Zoom out"},
		{"key": "KEY_MIDDLE", "label": "Middle drag", "category": "Camera", "action": "pan_camera", "description": "Pan camera"},
		{"key": "KEY_SPACE", "label": "Hold Space", "category": "Camera", "action": "free_camera", "description": "Free camera; drag to look around"},

		{"key": "KEY_LMB", "label": "Left click / drag", "category": "Selection and orders", "action": "", "description": "Select a unit, or box-select"},
		{"key": "KEY_SHIFT_LMB", "label": "Shift + left click", "category": "Selection and orders", "action": "", "description": "Add to selection"},
		{"key": "KEY_RMB", "label": "Right click", "category": "Selection and orders", "action": "move_order", "description": "Move to position"},
		{"key": "KEY_CTRL_RMB", "label": "Ctrl + right click", "category": "Selection and orders", "action": "attack_order", "description": "Attack"},
		{"key": "KEY_SHIFT_RMB", "label": "Shift + right click", "category": "Selection and orders", "action": "", "description": "Queue waypoint"},
		{"key": "KEY_SHIFT", "label": "Hold Shift", "category": "Selection and orders", "action": "", "description": "Show selected units' routes"},
		{"key": "KEY_X", "label": "X", "category": "Selection and orders", "action": "stop_order", "description": "Stop selected units"},

		{"key": "KEY_6", "label": "6", "category": "Building", "action": "build_structure_1", "description": "Forward Outpost"},
		{"key": "KEY_7", "label": "7", "category": "Building", "action": "build_structure_2", "description": "Radar Mast"},
		{"key": "KEY_T", "label": "T", "category": "Building", "action": "build_airfield", "description": "Airfield"},
		{"key": "KEY_L", "label": "L", "category": "Building", "action": "build_structure_3", "description": "Floodlight"},
		{"key": "KEY_R", "label": "R", "category": "Building", "action": "road_build", "description": "Road placement"},
		{"key": "KEY_8", "label": "8", "category": "Building", "action": "fob_build", "description": "FOB placement (units selected)"},

		{"key": "KEY_2", "label": "2", "category": "Logistics", "action": "material_claim", "description": "Claim material site"},
		{"key": "KEY_3", "label": "3", "category": "Logistics", "action": "material_demolish", "description": "Demolish material site"},
		{"key": "KEY_J", "label": "J", "category": "Logistics", "action": "fuel_shipment_request", "description": "Request fuel shipment"},
		{"key": "KEY_G", "label": "G", "category": "Logistics", "action": "", "description": "Strategic Command: order forces, which fly in to your airfield"},
		{"key": "KEY_D", "label": "D", "category": "Logistics", "action": "reinforcement_select", "description": "Choose reinforcement delivery zone"},
		{"key": "KEY_F", "label": "F", "category": "Logistics", "action": "reinforcement_request", "description": "Request reinforcement package"},

		{"key": "KEY_F1", "label": "F1", "category": "Interface", "action": "", "description": "Show or hide this hotkey sheet"},
		{"key": "KEY_F8", "label": "F8", "category": "Interface", "action": "toggle_debug", "description": "Debug panel"},
		{"key": "KEY_ESCAPE", "label": "Esc", "category": "Interface", "action": "quit_or_cancel", "description": "Close panel or cancel; quits when nothing is open"},
	]

	for hotkey in default_hotkeys:
		_hotkeys[hotkey["key"]] = hotkey
		_order.append(String(hotkey["key"]))


## The hotkeys grouped for display: [{"category", "hotkeys": [...]}] in sheet order.
func get_hotkey_sections() -> Array:
	if _hotkeys.is_empty():
		_load_default_hotkeys()
	var sections: Array = []
	for category in CATEGORY_ORDER:
		var entries: Array = []
		for key in _order:
			if _hotkeys.has(key) and String(_hotkeys[key].get("category", "")) == category:
				entries.append(_hotkeys[key])
		if not entries.is_empty():
			sections.append({"category": category, "hotkeys": entries})
	return sections

func set_enabled(enabled: bool) -> void:
	_enabled = enabled

func is_enabled() -> bool:
	return _enabled

func register_hotkey(key: String, action: String, description: String) -> void:
	if not _hotkeys.has(key):
		_order.append(key)
	_hotkeys[key] = {
		"key": key,
		"label": key.trim_prefix("KEY_"),
		"category": "Interface",
		"action": action,
		"description": description
	}

func unregister_hotkey(key: String) -> void:
	if _hotkeys.has(key):
		_hotkeys.erase(key)
		_order.erase(key)

func get_hotkey(key: String) -> Dictionary:
	return _hotkeys.get(key, {})

func get_all_hotkeys() -> Array:
	var result: Array = []
	for key in _hotkeys:
		result.append(_hotkeys[key])
	result.sort_custom(func(a, b):
		var key_a = a["key"]
		var key_b = b["key"]
		return key_a < key_b
	)
	return result

func get_hotkeys_by_category(category: String) -> Array:
	var result: Array = []
	for key in _hotkeys:
		var hotkey = _hotkeys[key]
		if hotkey.get("category", "") == category:
			result.append(hotkey)
	result.sort_custom(func(a, b):
		var key_a = a["key"]
		var key_b = b["key"]
		return key_a < key_b
	)
	return result

func get_description(key: String) -> String:
	var hotkey = _hotkeys.get(key, {})
	return hotkey.get("description", "")

func get_action(key: String) -> String:
	var hotkey = _hotkeys.get(key, {})
	return hotkey.get("action", "")

func clear() -> void:
	_hotkeys.clear()
	_load_default_hotkeys()
