class_name VisualDefinitionRegistry
extends RefCounted

## Presentation-only visual registry. It is deliberately independent from the
## simulation: a visual replacement cannot alter a unit's ID, balance, orders,
## combat, replay identity, or faction rules.

var _definitions: Dictionary = {}
var _resources: Dictionary = {}
var _missing_resources: Dictionary = {}
var errors: PackedStringArray = []


func load_definitions(path := "res://visuals/visual_definitions.json") -> bool:
	_definitions.clear()
	_resources.clear()
	_missing_resources.clear()
	errors.clear()
	var file := FileAccess.open(path, FileAccess.READ)
	if file == null:
		errors.append("Visual definitions missing: %s" % path)
		return false
	var parsed := JSON.new()
	if parsed.parse(file.get_as_text()) != OK or not parsed.data is Dictionary:
		errors.append("Visual definitions are not valid JSON: %s" % path)
		return false
	for definition in parsed.data.get("visual_definitions", []):
		if not definition is Dictionary:
			errors.append("Visual definition is not an object")
			continue
		var visual_id := String(definition.get("visual_id", ""))
		var model_path := String(definition.get("model_path", ""))
		if visual_id.is_empty() or model_path.is_empty():
			errors.append("Visual definition requires visual_id and model_path")
			continue
		if _definitions.has(visual_id):
			errors.append("Duplicate visual_id: %s" % visual_id)
			continue
		_definitions[visual_id] = definition
	return errors.is_empty()


func resolve(visual_id: String) -> Dictionary:
	if _definitions.has(visual_id):
		return _definitions[visual_id]
	errors.append("Unknown visual_id: %s" % visual_id)
	return fallback_definition(visual_id)


func load_model(visual_id: String) -> Resource:
	var definition := resolve(visual_id)
	if bool(definition.get("fallback", false)):
		return null
	if _resources.has(visual_id):
		return _resources[visual_id]
	if _missing_resources.has(visual_id):
		return null
	var path := String(definition.model_path)
	if not ResourceLoader.exists(path):
		errors.append("Visual model is missing: %s" % path)
		_missing_resources[visual_id] = true
		return null
	var resource := load(path)
	if resource == null:
		errors.append("Visual model failed to load: %s" % path)
		_missing_resources[visual_id] = true
		return null
	_resources[visual_id] = resource
	return resource


func fallback_definition(requested_id: String) -> Dictionary:
	return {
		"visual_id": "development.fallback",
		"requested_visual_id": requested_id,
		"fallback": true,
		"selection_radius": 2.0,
		"model_path": ""
	}
