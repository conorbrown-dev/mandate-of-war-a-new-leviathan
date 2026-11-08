extends SceneTree

const BuildCatalog := preload("res://ui/components/build_catalog.gd")

func _init() -> void:
	var catalog := BuildCatalog.new()
	root.add_child(catalog)
	await process_frame
	var requested: Array[int] = []
	catalog.build_requested.connect(func(build_type: int): requested.append(build_type))
	catalog.refresh_catalog([
		{"type": 8, "name": "Engineering Vehicle", "material": 180.0, "build_seconds": 12.0, "available": true, "is_structure": false, "is_aircraft": false, "icon_id": "construction.build"},
		{"type": 103, "name": "Floodlight", "material": 620.0, "build_seconds": 18.0, "available": true, "is_structure": true, "icon_id": "construction.build"},
	], true)
	await process_frame
	assert(catalog.visible)
	assert(is_equal_approx(catalog.MAX_WIDTH, 540.0))
	# The engineer builds structures only: the unit entry is not offered.
	assert(catalog._entries.size() == 1)
	assert(int(catalog._entries[0].get("type", -1)) == 103)
	var first_button: Button = catalog._entries[0].get("button")
	assert(first_button.custom_minimum_size.y >= 52.0)
	assert(first_button.get_node_or_null("ResourceCosts/MassCost") != null and first_button.get_node_or_null("ResourceCosts/EnergyCost") == null)
	first_button.pressed.emit()
	assert(requested == [103])
	print("BUILD_CATALOG checks=7 failures=0")
	quit()
