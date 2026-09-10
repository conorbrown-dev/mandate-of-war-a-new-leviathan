extends Node

func _ready():
	var extension: Object = ClassDB.instantiate("RtsExtension")
	if extension == null:
		push_error("RtsExtension is registered but could not be instantiated.")
		return
	
	var map_data: Dictionary = extension.call("map_loader_load_map", "res://scenarios/two_landmass_skirmish.json")
	if not map_data.get("ok", false):
		push_error("Map load failed: %s" % map_data.get("error", "unknown"))
		return
	
	print("Map loaded successfully")
	print("Initial entities count: %d" % extension.call("get_initial_entity_count"))
	
	extension.call("start_simulation")
	
	get_tree().quit()
