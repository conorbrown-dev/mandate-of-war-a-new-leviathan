extends SceneTree
## Trees must vary clearly in size, within each stand and between stands.
## Run: godot --headless --path godot/project --script res://test_tree_sizes.gd

const TerrainVegetation := preload("res://terrain_vegetation.gd")

var checks := 0
var failures := 0


func check(condition: bool, reason: String) -> void:
	checks += 1
	if not condition:
		push_error(reason)
		failures += 1


func _initialize() -> void:
	for heights: Vector2 in [TerrainVegetation.CONIFER_HEIGHTS, TerrainVegetation.BROADLEAF_HEIGHTS]:
		var rng := RandomNumberGenerator.new()
		rng.seed = 7
		var smallest := INF
		var tallest := 0.0
		var stand_means := {}
		for sample in range(6000):
			var position := Vector2(rng.randf_range(-6000.0, 6000.0), rng.randf_range(-6000.0, 6000.0))
			var height: float = TerrainVegetation.tree_height(heights, position, rng)
			check(height >= heights.x - 0.001 and height <= heights.y + 0.001, "a tree stays within its kind's range")
			smallest = minf(smallest, height)
			tallest = maxf(tallest, height)
			var cell := Vector2i(floori(position.x / TerrainVegetation.STAND_SIZE), floori(position.y / TerrainVegetation.STAND_SIZE))
			var entry: Vector2 = stand_means.get(cell, Vector2.ZERO)
			stand_means[cell] = entry + Vector2(height, 1.0)
		check(tallest / smallest > 2.5, "the tallest tree is over 2.5 times the smallest (%.1f to %.1f m)" % [smallest, tallest])
		var low := INF
		var high := 0.0
		for cell in stand_means:
			var entry: Vector2 = stand_means[cell]
			if entry.y < 5.0:
				continue
			low = minf(low, entry.x / entry.y)
			high = maxf(high, entry.x / entry.y)
		check(high - low > (heights.y - heights.x) * 0.3, "stands differ in average age (%.1f to %.1f m)" % [low, high])
	print("TREE_SIZES checks=%d failures=%d" % [checks, failures])
	quit(1 if failures > 0 else 0)
