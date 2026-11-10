extends SceneTree
## Overlapping range circles are drawn as one merged outline per side and kind.

const Renderer := preload("res://range_envelope_renderer.gd")

var checks := 0
var failures := 0


func check(condition: bool, reason: String) -> void:
	checks += 1
	if not condition:
		push_error(reason)
		failures += 1


func _initialize() -> void:
	_run.call_deferred()


func _length(pieces: PackedVector2Array) -> float:
	var total := 0.0
	for index in range(0, pieces.size() - 1, 2):
		total += pieces[index].distance_to(pieces[index + 1])
	return total


# No drawn piece may lie inside any hex of the same group. Pieces end exactly on
# a neighbour's edge, so each is judged by its midpoint.
func _points_on_outside(pieces: PackedVector2Array, hexes: Array) -> bool:
	for index in range(0, pieces.size() - 1, 2):
		var middle := (pieces[index] + pieces[index + 1]) * 0.5
		for hex in hexes:
			# A straight piece sits under a metre inside its own circle.
			if middle.distance_to(hex.center as Vector2) < float(hex.radius) - 1.0:
				return false
	return true


func _test_geometry() -> void:
	var one := [{"center": Vector2.ZERO, "radius": 100.0}]
	check(absf(_length(Renderer.merged_outline(one)) - TAU * 100.0) < 3.0, "a lone circle keeps its whole outline")
	var apart := [{"center": Vector2.ZERO, "radius": 100.0}, {"center": Vector2(1000, 0), "radius": 100.0}]
	check(absf(_length(Renderer.merged_outline(apart)) - TAU * 200.0) < 6.0, "circles that do not meet keep both outlines")
	var nested := [{"center": Vector2.ZERO, "radius": 300.0}, {"center": Vector2(40, 20), "radius": 100.0}]
	var nested_pieces := Renderer.merged_outline(nested)
	check(absf(_length(nested_pieces) - TAU * 300.0) < 6.0, "a circle inside a bigger one disappears into it")
	var pair := [{"center": Vector2.ZERO, "radius": 100.0}, {"center": Vector2(120, 0), "radius": 100.0}]
	var pair_pieces := Renderer.merged_outline(pair)
	var pair_length := _length(pair_pieces)
	check(pair_length > TAU * 100.0 and pair_length < TAU * 190.0, "two overlapping circles lose the edges inside each other (%.0f m drawn)" % pair_length)
	check(_points_on_outside(pair_pieces, pair), "nothing is drawn inside the merged area")
	var crowd: Array = []
	for row in range(10):
		for column in range(10):
			crowd.append({"center": Vector2(column * 14.0, row * 14.0), "radius": 150.0})
	var started := Time.get_ticks_usec()
	var crowd_pieces := Renderer.merged_outline(crowd)
	var crowd_ms := float(Time.get_ticks_usec() - started) / 1000.0
	var squad: Array = crowd.slice(0, 20)
	var squad_started := Time.get_ticks_usec()
	Renderer.merged_outline(squad)
	var squad_ms := float(Time.get_ticks_usec() - squad_started) / 1000.0
	print("RANGE_ENVELOPES crowd_100_ms=%.1f squad_20_ms=%.1f crowd_length=%.0f" % [crowd_ms, squad_ms, _length(crowd_pieces)])
	check(_points_on_outside(crowd_pieces, crowd), "a crowd of 100 draws only its outer outline")
	# The union of offset hexes has a stepped edge, so it is longer than one
	# smooth ring, but a small part of the 100 separate outlines (90 km).
	check(_length(crowd_pieces) < 0.1 * 100.0 * TAU * 150.0, "and that outline is the group's edge, not 100 circles (%.0f m)" % _length(crowd_pieces))
	check(crowd_ms < 250.0, "merging 100 circles stays cheap (%.1f ms)" % crowd_ms)


func _test_in_game() -> void:
	var view: Node = load("res://main.tscn").instantiate()
	root.add_child(view)
	await process_frame
	await process_frame
	view._on_start_skirmish_pressed()
	for _frame in range(5):
		await process_frame
	view.extension.call("skirmish_set_ai_enabled", false)
	var engineer := int(view.engineer_ids.get(0, -1))
	var at: PackedFloat32Array = view.extension.call("get_unit_position", engineer)
	for index in range(4):
		view.extension.call("create_unit_with_type", at[0] + 25.0 + index * 14.0, at[1] + 20.0, 0, 0)
	for _frame in range(20):
		await process_frame
	var renderer: Node = view.range_envelope_renderer
	check(renderer != null, "the game draws merged range envelopes")
	var player_views: Array = renderer.get_children().filter(func(child): return String(child.name).begins_with("Envelope_0_"))
	check(player_views.size() == 3, "one outline each for the player's sight, radar and weapon reach (%d)" % player_views.size())
	var own_hexes_shown := 0
	for id in view.player_entity_ids:
		var wrapper = view.prototype_visual_views.get(id)
		if wrapper == null:
			continue
		for ring in ["VisibilityHex", "RadarHex", "AttackHex"]:
			var node: Node3D = wrapper.get_node_or_null(ring)
			if node != null and node.visible:
				own_hexes_shown += 1
	check(own_hexes_shown == 0, "ground units no longer draw their own overlapping circles (%d shown)" % own_hexes_shown)
	var hexes: Array = []
	for id in view.player_entity_ids:
		var wrapper = view.prototype_visual_views.get(id)
		for envelope in wrapper.range_envelopes():
			if envelope.kind == "AttackHex":
				hexes.append({"center": Vector2(wrapper.global_position.x, wrapper.global_position.z), "radius": envelope.radius})
	var attack_pieces := Renderer.merged_outline(hexes)
	var separate := 0.0
	for hex in hexes:
		separate += TAU * float(hex.radius)
	check(hexes.size() >= 5 and _length(attack_pieces) < separate * 0.5 and _points_on_outside(attack_pieces, hexes),
		"the group's weapon reach is drawn as one outline (%.0f m of %.0f m of separate circles)" % [_length(attack_pieces), separate])
	# Outlines are rebuilt one per frame; give them all a chance to settle.
	for _frame in range(10):
		await process_frame
	var rebuilt: int = renderer.rebuild_count
	for _frame in range(10):
		await process_frame
	check(renderer.rebuild_count == rebuilt, "standing units do not redraw the outline every frame")
	view.queue_free()
	await process_frame


func _run() -> void:
	_test_geometry()
	await _test_in_game()
	print("RANGE_ENVELOPES checks=%d failures=%d" % [checks, failures])
	quit(1 if failures > 0 else 0)
