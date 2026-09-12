extends SceneTree

const MapEditorModelScript = preload("res://map_editor_model.gd")

func _init() -> void:
	var editor = MapEditorModelScript.new()
	assert(editor.create_map("editor_test", 64, 64, 16.0))
	assert(editor.set_height(1, 1, 12.5))
	assert(editor.set_water(0, 0, true))
	assert(editor.add_spawn("player", "faction_a", Vector2(16, 16)))
	assert(editor.add_resource("metal", "material", Vector2(32, 32), 100.0, 4.0))
	assert(editor.add_entity("unit", "unit|test_mod|heavy", Vector2(48, 48)))
	assert(editor.validate().is_empty())
	assert(not editor.add_spawn("player", "faction_a", Vector2(16, 16)))
	assert(not editor.add_resource("outside", "material", Vector2(80, 0), 1.0, 1.0))
	print("Map editor model assertions passed")
	quit()
