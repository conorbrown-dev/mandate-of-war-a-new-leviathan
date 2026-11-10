class_name RangeEnvelopeRenderer
extends Node3D
## Draws ground units' range circles as one merged outline per side and range
## kind, the way Forged Alliance merges overlapping range rings: an arc is
## drawn only where it is not inside another circle of the same kind and side,
## so a group shows its combined reach instead of a tangle of rings. Lines are
## the same curved, terrain-following, 3 px outline the fog of war edge uses.
## Presentation only; the ranges come from the simulation's unit profiles.

const RangeOutlineScript := preload("res://range_outline.gd")
const KIND_COLORS := {
	"VisibilityHex": Color("#2d8cff"),
	"RadarHex": Color("#a04dff"),
	"AttackHex": Color("#e33b3b"),
}
const LINE_LIFT := 0.12
# Coincident circles (two the same size and place) are not "inside" each
# other, so both are kept; a shared arc simply draws once over the other.
const INSIDE_MARGIN := 0.01

var _views := {}
var _signatures := {}
var _next_key := 0
# A moving group's outline is redrawn at most this often; a new one at once.
const REDRAW_INTERVAL_MSEC := 100
var _last_redraw_msec := -REDRAW_INTERVAL_MSEC
var rebuild_count := 0


## hexes_by_key: "faction:kind" -> Array of {center: Vector2, radius: float,
## dash_phase: int}. height_at(x, z) drapes the lines over the ground.
## At most one changed outline is rebuilt per call, so a large moving army
## spreads the work over frames instead of stalling one.
func update_envelopes(hexes_by_key: Dictionary, height_at: Callable) -> void:
	for key in _views.keys():
		if not hexes_by_key.has(key):
			(_views[key] as MeshInstance3D).queue_free()
			_views.erase(key)
			_signatures.erase(key)
	var keys := hexes_by_key.keys()
	keys.sort()
	for offset in range(keys.size()):
		var key: String = keys[(_next_key + offset) % keys.size()]
		var signature := _signature_of(hexes_by_key[key])
		if _signatures.get(key, "") == signature:
			continue
		var fresh := not _signatures.has(key)
		if not fresh and Time.get_ticks_msec() - _last_redraw_msec < REDRAW_INTERVAL_MSEC:
			continue
		if not fresh:
			_last_redraw_msec = Time.get_ticks_msec()
		_signatures[key] = signature
		_next_key = (_next_key + offset + 1) % keys.size()
		rebuild_count += 1
		_rebuild(key, hexes_by_key[key], height_at)
		return


func _rebuild(key: String, hexes: Array, height_at: Callable) -> void:
	var kind := String(key).get_slice(":", 1)
	var view: MeshInstance3D = _views.get(key)
	if view == null:
		view = MeshInstance3D.new()
		view.name = "Envelope_%s" % String(key).replace(":", "_")
		view.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		view.material_override = _material_for(kind)
		add_child(view)
		_views[key] = view
	var points := PackedVector3Array()
	for point in merged_outline(hexes):
		var height: float = height_at.call(point.x, point.y) if height_at.is_valid() else 0.0
		points.append(Vector3(point.x, height + LINE_LIFT, point.y))
	view.mesh = RangeOutlineScript.ribbon_mesh(points)


## The merged outline as line-segment endpoint pairs. Each circle is split into
## the same short pieces a single unit's circle uses, so the result follows the
## terrain and keeps the dashes that tell coincident sight and weapon ranges apart.
static func merged_outline(hexes: Array) -> PackedVector2Array:
	var pieces := PackedVector2Array()
	for index in range(hexes.size()):
		var hex: Dictionary = hexes[index]
		var center: Vector2 = hex.center
		var radius := float(hex.radius)
		if radius <= 0.0:
			continue
		var covered: Array = []
		var buried := false
		for other_index in range(hexes.size()):
			if other_index == index:
				continue
			var other: Dictionary = hexes[other_index]
			if float(other.radius) <= 0.0:
				continue
			var span := _covered_arc(center, radius, other)
			if span.y - span.x >= TAU:
				buried = true
				break
			if span.x < span.y:
				_add_wrapped(covered, span)
		if buried:
			continue
		var free := _free_arcs(covered)
		var dash_phase := int(hex.get("dash_phase", -1))
		var count: int = RangeOutlineScript.piece_count(radius, dash_phase >= 0)
		for piece in range(count):
			if dash_phase >= 0 and piece % 2 != dash_phase:
				continue
			var from := TAU * float(piece) / count
			var to := TAU * float(piece + 1) / count
			for arc in free:
				var low := maxf(from, arc.x)
				var high := minf(to, arc.y)
				if high - low <= 0.000001:
					continue
				pieces.append(center + Vector2(cos(low), sin(low)) * radius)
				pieces.append(center + Vector2(cos(high), sin(high)) * radius)
	return pieces


static func inside_hex(point: Vector2, hex: Dictionary) -> bool:
	return point.distance_to(hex.center as Vector2) < float(hex.radius) - INSIDE_MARGIN


# The angles (radians) of a circle's outline that lie strictly inside another
# circle, as Vector2(from, to) around the direction to it. A span of a full turn
# means the whole circle is buried; an empty one (x >= y) means none of it is.
static func _covered_arc(center: Vector2, radius: float, other: Dictionary) -> Vector2:
	var other_radius := float(other.radius) - INSIDE_MARGIN
	var offset: Vector2 = (other.center as Vector2) - center
	var distance := offset.length()
	if distance + radius <= other_radius:
		return Vector2(0.0, TAU)
	if distance >= radius + other_radius or distance + other_radius <= radius:
		return Vector2(1.0, 0.0)
	var half := acos(clampf((radius * radius + distance * distance - other_radius * other_radius) / (2.0 * radius * distance), -1.0, 1.0))
	var facing := atan2(offset.y, offset.x)
	return Vector2(facing - half, facing + half)


# Adds a span to a list of covered spans in [0, TAU], splitting where it wraps.
static func _add_wrapped(spans: Array, span: Vector2) -> void:
	var low := fposmod(span.x, TAU)
	var high := low + (span.y - span.x)
	if high > TAU:
		spans.append(Vector2(low, TAU))
		spans.append(Vector2(0.0, high - TAU))
	else:
		spans.append(Vector2(low, high))


# What is left of [0, TAU] once the covered spans are removed.
static func _free_arcs(covered: Array) -> Array:
	covered.sort_custom(func(first: Vector2, second: Vector2) -> bool: return first.x < second.x)
	var free: Array = []
	var cursor := 0.0
	for span in covered:
		if span.x > cursor:
			free.append(Vector2(cursor, span.x))
		cursor = maxf(cursor, span.y)
	if cursor < TAU:
		free.append(Vector2(cursor, TAU))
	return free


func _material_for(kind: String) -> ShaderMaterial:
	return RangeOutlineScript.material_for(KIND_COLORS.get(kind, Color.WHITE))


# Centres to the nearest half metre: small drift does not redraw the outline.
static func _signature_of(hexes: Array) -> String:
	var parts := PackedStringArray()
	for hex in hexes:
		var center: Vector2 = hex.center
		parts.append("%d,%d,%d,%d" % [roundi(center.x * 2.0), roundi(center.y * 2.0), roundi(float(hex.radius) * 10.0), int(hex.get("dash_phase", -1))])
	return "|".join(parts)
