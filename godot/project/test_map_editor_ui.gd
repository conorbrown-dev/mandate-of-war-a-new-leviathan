extends SceneTree

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	var editor = load("res://map_editor.tscn").instantiate()
	root.add_child(editor)
	await process_frame
	_press(editor, "Raise Center")
	_press(editor, "Add Player Spawn")
	_press(editor, "Save")
	assert(not "failed" in editor.status.text.to_lower(), "Save action must succeed")
	_press(editor, "New 64m Map")
	_press(editor, "Load")
	assert(editor.status.text == "Loaded", "Load must open the map written by Save")
	assert(editor.model.terrain[10] == 12.0, "Save/Load preserves edited terrain")
	assert(editor.model.spawn_points.size() == 1, "Save/Load preserves player spawn")
	print("MAP_EDITOR_UI checks=4 failures=0")
	quit()

func _press(node: Node, label: String) -> bool:
	if node is Button and node.text == label:
		node.pressed.emit()
		return true
	for child in node.get_children():
		if _press(child, label):
			return true
	return false
