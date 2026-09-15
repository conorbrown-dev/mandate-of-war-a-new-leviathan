extends Node

var _hotkeys: Dictionary = {}
var _enabled: bool = true

func _ready() -> void:
	_load_default_hotkeys()

func _load_default_hotkeys() -> void:
	_hotkeys.clear()

	var default_hotkeys = [
		{
			"key": "KEY_ESCAPE",
			"action": "quit_or_cancel",
			"description": "Quit / Cancel current mode"
		},
		{
			"key": "KEY_F8",
			"action": "toggle_debug",
			"description": "Toggle debug panel"
		},
		{
			"key": "KEY_SPACE",
			"action": "free_camera",
			"description": "Free camera mode"
		},
		{
			"key": "KEY_X",
			"action": "stop_order",
			"description": "Stop selected units"
		},
		{
			"key": "KEY_D",
			"action": "reinforcement_select",
			"description": "Select reinforcement delivery zone"
		},
		{
			"key": "KEY_F",
			"action": "reinforcement_request",
			"description": "Request reinforcement package"
		},
		{
			"key": "KEY_8",
			"action": "fob_build",
			"description": "Forward Outpost build mode"
		},
		{
			"key": "KEY_6",
			"action": "build_structure_1",
			"description": "Build structure 1 (Forward Outpost)"
		},
		{
			"key": "KEY_7",
			"action": "build_structure_2",
			"description": "Build structure 2 (Radar Mast)"
		},
		{
			"key": "KEY_L",
			"action": "build_structure_3",
			"description": "Build structure 3 (Floodlight)"
		},
		{
			"key": "KEY_R",
			"action": "road_build",
			"description": "Road placement mode"
		},
		{
			"key": "KEY_2",
			"action": "material_claim",
			"description": "Claim material site"
		},
		{
			"key": "KEY_3",
			"action": "material_demolish",
			"description": "Demolish material site"
		},
		{
			"key": "KEY_1",
			"action": "unit_shortcut_1",
			"description": "Unit shortcut 1"
		},
		{
			"key": "KEY_4",
			"action": "unit_shortcut_4",
			"description": "Unit shortcut 4"
		},
		{
			"key": "KEY_5",
			"action": "unit_shortcut_5",
			"description": "Unit shortcut 5"
		},
		{
			"key": "KEY_9",
			"action": "unit_shortcut_9",
			"description": "Unit shortcut 9"
		},
		{
			"key": "KEY_0",
			"action": "unit_shortcut_0",
			"description": "Unit shortcut 0"
		},
		{
			"key": "KEY_P",
			"action": "unit_shortcut_P",
			"description": "Unit shortcut P"
		},
		{
			"key": "KEY_RMB",
			"action": "move_order",
			"description": "Move to position (right-click)"
		},
		{
			"key": "KEY_CTRL_RMB",
			"action": "attack_order",
			"description": "Attack enemy (Ctrl + right-click)"
		},
		{
			"key": "KEY_WHEEL_UP",
			"action": "zoom_in",
			"description": "Zoom in"
		},
		{
			"key": "KEY_WHEEL_DOWN",
			"action": "zoom_out",
			"description": "Zoom out"
		},
		{
			"key": "KEY_MIDDLE",
			"action": "pan_camera",
			"description": "Pan camera (middle-click drag)"
		}
	]

	for hotkey in default_hotkeys:
		_hotkeys[hotkey["key"]] = hotkey

func set_enabled(enabled: bool) -> void:
	_enabled = enabled

func is_enabled() -> bool:
	return _enabled

func register_hotkey(key: String, action: String, description: String) -> void:
	_hotkeys[key] = {
		"key": key,
		"action": action,
		"description": description
	}

func unregister_hotkey(key: String) -> void:
	if _hotkeys.has(key):
		_hotkeys.erase(key)

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
