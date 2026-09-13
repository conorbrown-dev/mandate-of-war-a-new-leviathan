extends Control

const MapEditorModelScript = preload("res://map_editor_model.gd")
var model = MapEditorModelScript.new()
var status: Label

func _ready() -> void:
	var panel := VBoxContainer.new()
	panel.set_anchors_and_offsets_preset(Control.PRESET_CENTER)
	panel.custom_minimum_size = Vector2(420, 0)
	add_child(panel)
	var title := Label.new(); title.text = "MAP EDITOR  //  FOUNDATION"; panel.add_child(title)
	status = Label.new(); panel.add_child(status)
	_add_button(panel, "New 64m Map", _new_map)
	_add_button(panel, "Raise Center", _raise_center)
	_add_button(panel, "Toggle Water", _toggle_water)
	_add_button(panel, "Add Player Spawn", _add_spawn)
	_add_button(panel, "Add Material Site", _add_resource)
	_add_button(panel, "Validate", _validate)
	_add_button(panel, "Save", _save)
	_add_button(panel, "Load", _load)
	_new_map()

func _add_button(parent: Control, text: String, action: Callable) -> void:
	var button := Button.new(); button.text = text; button.pressed.connect(action); parent.add_child(button)

func _new_map() -> void:
	model.create_map("editor_map", 64, 64, 16.0); _message("New editable 64m map")
func _raise_center() -> void: _message("Center height edited" if model.set_height(2, 2, 12.0) else "Create a map first")
func _toggle_water() -> void: _message("Water cell toggled" if model.set_water(0, 0, not model.water_cells.has(Vector2i(0, 0))) else "Invalid cell")
func _add_spawn() -> void: _message("Spawn added" if model.add_spawn("player", "faction_a", Vector2(16, 16)) else "Spawn already exists or is invalid")
func _add_resource() -> void: _message("Resource added" if model.add_resource("metal", "material", Vector2(32, 32), 100.0, 4.0) else "Resource already exists or is invalid")
func _validate() -> void:
	var errors := model.validate(); _message("Map valid" if errors.is_empty() else errors[0])
func _save() -> void: _message("Saved" if model.save_to_file("user://map_editor_draft.json") else "Save failed")
func _load() -> void: _message("Loaded" if model.load_from_file("user://map_editor_draft.json") else "Load failed")
func _message(text: String) -> void: status.text = text
