class_name UiIconRegistry
extends RefCounted

## Semantic UI icon access over generated Godot assets.
## Generated file paths are deliberately private to this registry; callers use
## stable IDs such as `order.move` and `fighter` instead.

const COMMAND_INDEX_PATH := "res://assets/ui/icons/index.json"
const NATO_INDEX_PATH := "res://assets/ui/symbols/nato/index.json"

static var _loaded := false
static var _command_icons: Dictionary = {}
static var _nato_symbols: Dictionary = {}
static var _textures: Dictionary = {}
static var _warnings_emitted: Dictionary = {}
static var _index_errors: PackedStringArray = []


static func get_icon(icon_id: StringName) -> Texture2D:
	return _load_texture(_command_path(icon_id), "command:%s" % icon_id)


static func get_nato_symbol(symbol_id: StringName, affiliation: StringName = &"friendly") -> Texture2D:
	return _load_texture(_nato_path(symbol_id, affiliation), "nato:%s:%s" % [symbol_id, affiliation])


static func get_icon_path(icon_id: StringName) -> String:
	return _command_path(icon_id)


static func get_nato_symbol_path(symbol_id: StringName, affiliation: StringName = &"friendly") -> String:
	return _nato_path(symbol_id, affiliation)


static func get_index_errors() -> PackedStringArray:
	_ensure_loaded()
	return _index_errors


static func validate_nato_entries(entries: Array) -> PackedStringArray:
	var errors := PackedStringArray()
	var seen := {}
	for entry in entries:
		if not entry is Dictionary:
			errors.append("NATO index contains a non-object entry")
			continue
		var key := "%s:%s" % [entry.get("id", ""), entry.get("affiliation", "")]
		if key == ":":
			errors.append("NATO index entry is missing id or affiliation")
		elif seen.has(key):
			errors.append("Duplicate NATO semantic ID: %s" % key)
		else:
			seen[key] = true
	return errors


static func _command_path(icon_id: StringName) -> String:
	_ensure_loaded()
	var entry: Dictionary = _command_icons.get(String(icon_id), {})
	if entry.is_empty():
		_warn_once("command:%s" % icon_id, "Missing command icon '%s'" % icon_id)
		return ""
	return String(entry.get("path", ""))


static func _nato_path(symbol_id: StringName, affiliation: StringName) -> String:
	_ensure_loaded()
	var key := "%s:%s" % [symbol_id, affiliation]
	var entry: Dictionary = _nato_symbols.get(key, {})
	if entry.is_empty():
		_warn_once("nato:%s" % key, "Missing NATO symbol '%s' for affiliation '%s'" % [symbol_id, affiliation])
		return ""
	return String(entry.get("path", ""))


static func _load_texture(path: String, warning_key: String) -> Texture2D:
	if path.is_empty():
		return null
	if _textures.has(path):
		return _textures[path]
	var texture := ResourceLoader.load(path, "Texture2D") as Texture2D
	if texture == null:
		_warn_once("texture:%s" % warning_key, "Unable to load UI icon at '%s'" % path)
		return null
	_textures[path] = texture
	return texture


static func _ensure_loaded() -> void:
	if _loaded:
		return
	_loaded = true
	var command_index: Variant = _read_json(COMMAND_INDEX_PATH)
	if command_index is Dictionary:
		for semantic_id in command_index:
			var entry = command_index[semantic_id]
			if not entry is Dictionary or String(entry.get("id", "")) != String(semantic_id) or String(entry.get("path", "")).is_empty():
				_index_errors.append("Invalid command icon index entry: %s" % semantic_id)
				continue
			_command_icons[semantic_id] = entry
	else:
		_index_errors.append("Command icon index is unavailable")
	var nato_index: Variant = _read_json(NATO_INDEX_PATH)
	if nato_index is Dictionary and nato_index.get("icons", null) is Array:
		var entries: Array = nato_index.get("icons", [])
		_index_errors.append_array(validate_nato_entries(entries))
		for entry in entries:
			if not entry is Dictionary or String(entry.get("path", "")).is_empty():
				continue
			_nato_symbols["%s:%s" % [entry.get("id", ""), entry.get("affiliation", "")]] = entry
	else:
		_index_errors.append("NATO symbol index is unavailable")
	for error in _index_errors:
		_warn_once("index:%s" % error, error)


static func _read_json(path: String) -> Variant:
	var file := FileAccess.open(path, FileAccess.READ)
	if file == null:
		_warn_once("file:%s" % path, "Unable to open generated UI icon index '%s'" % path)
		return null
	var parsed: Variant = JSON.parse_string(file.get_as_text())
	if parsed == null:
		_warn_once("json:%s" % path, "Unable to parse generated UI icon index '%s'" % path)
	return parsed


static func _warn_once(key: String, message: String) -> void:
	if _warnings_emitted.has(key):
		return
	_warnings_emitted[key] = true
	push_warning(message)
