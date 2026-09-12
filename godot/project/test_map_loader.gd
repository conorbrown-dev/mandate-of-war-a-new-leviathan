extends SceneTree

func _init():
	OS.set_environment("RTS_DATA_ROOT", ProjectSettings.globalize_path("res://../../data").simplify_path())
	
	if not ClassDB.class_exists("RtsExtension"):
		print("RtsExtension not registered")
		quit(1)
		return
	
	var extension: Object = ClassDB.instantiate("RtsExtension")
	if extension == null:
		print("RtsExtension could not be instantiated")
		quit(1)
		return
	
	var map_data: Dictionary = extension.call("map_loader_load_map", "res://scenarios/two_landmass_skirmish.json")
	print("Map load result: ", map_data)
	
	if not map_data.get("ok", false):
		print("Error: ", map_data.get("error", "unknown"))
		quit(1)
	else:
		print("Map loaded successfully!")
		print("ID: ", map_data.get("id"))
		print("Name: ", map_data.get("name"))
		print("Dimensions: ", map_data.get("width"), "x", map_data.get("height"))
		quit(0)
