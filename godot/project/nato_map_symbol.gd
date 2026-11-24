class_name NatoMapSymbol
extends RefCounted

## APP-6 / MIL-STD-2525 unit symbols for the map-scale overlay, drawn as
## vectors so they stay crisp at 20 to 30 pixels. Geometry uses the standard's
## 200 x 200 symbol space (frame centred on 100,100), scaled to screen pixels.
##
## Frames follow the affiliation and battle dimension: friendly land is a
## rectangle, hostile land and hostile sea surface a diamond, friendly sea
## surface a circle, friendly air a dome and hostile air a pointed arch (both
## open at the bottom). The function icon comes from the unit's
## `strategic_icon` key in data/unit_faction_stats.json.

const UnitCatalog := preload("res://unit_catalog.gd")

# Standard APP-6 fill colours (light tones) and black frame/icon lines.
const FRIENDLY_FILL := Color("#80e0ff")
const HOSTILE_FILL := Color("#ff8080")
const LINE_COLOR := Color("#000000")
const HALO_COLOR := Color(0.0, 0.0, 0.0, 0.55)
const SCALE := 0.17 # Friendly land frame: 150 x 100 units -> about 26 x 17 px.

# strategic_icon key -> battle dimension and function icon.
const FUNCTIONS := {
	"armour": {"dimension": "land", "label": "Armour"},
	"sp_artillery": {"dimension": "land", "label": "Self-propelled artillery"},
	"air_defense": {"dimension": "land", "label": "Air defence (missile)"},
	"missile": {"dimension": "land", "label": "Surface-to-surface missile"},
	"recon": {"dimension": "land", "label": "Reconnaissance"},
	"engineer": {"dimension": "land", "label": "Engineer"},
	"command": {"dimension": "land", "label": "Headquarters"},
	"fighter": {"dimension": "air", "label": "Fixed-wing fighter", "letters": "F"},
	"recon_aircraft": {"dimension": "air", "label": "Fixed-wing reconnaissance", "letters": "R"},
	"cargo_aircraft": {"dimension": "air", "label": "Fixed-wing cargo", "letters": "C"},
	"attack_helicopter": {"dimension": "air", "label": "Rotary-wing attack", "letters": "A"},
	"naval_surface": {"dimension": "sea", "label": "Sea surface combatant (patrol)"},
}


static func function_for(unit_type: int) -> String:
	var key := UnitCatalog.strategic_icon(unit_type)
	return key if FUNCTIONS.has(key) else ""


static func dimension_for(unit_type: int) -> String:
	return String(FUNCTIONS.get(function_for(unit_type), {}).get("dimension", "land"))


static func frame_for(dimension: String, hostile: bool) -> String:
	match dimension:
		"air": return "air_arch" if hostile else "air_dome"
		"sea": return "diamond" if hostile else "circle"
		_: return "diamond" if hostile else "rectangle"


## What the overlay draws for one unit; tests read this instead of pixels.
static func describe(unit_type: int, hostile: bool) -> Dictionary:
	var function := function_for(unit_type)
	var dimension := dimension_for(unit_type)
	return {
		"function": function,
		"dimension": dimension,
		"affiliation": "hostile" if hostile else "friendly",
		"frame": frame_for(dimension, hostile),
		"letters": String(FUNCTIONS.get(function, {}).get("letters", "")),
		"fill": HOSTILE_FILL if hostile else FRIENDLY_FILL,
	}


static func draw(canvas: CanvasItem, center: Vector2, unit_type: int, hostile: bool, font: Font) -> Dictionary:
	var symbol := describe(unit_type, hostile)
	var frame := String(symbol.frame)
	var outline := _frame_outline(frame, center)
	var fill_points := PackedVector2Array(outline)
	canvas.draw_colored_polygon(fill_points, symbol.fill)
	var closed := frame in ["rectangle", "diamond", "circle"]
	var stroke := PackedVector2Array(outline)
	if closed:
		stroke.append(outline[0])
	canvas.draw_polyline(stroke, HALO_COLOR, 3.2, true)
	canvas.draw_polyline(stroke, LINE_COLOR, 1.6, true)
	_draw_function(canvas, center, String(symbol.function), frame, font)
	return symbol


static func _p(center: Vector2, x: float, y: float) -> Vector2:
	return center + Vector2(x - 100.0, y - 100.0) * SCALE


static func _frame_outline(frame: String, center: Vector2) -> PackedVector2Array:
	var points := PackedVector2Array()
	match frame:
		"rectangle":
			for xy in [[25, 50], [175, 50], [175, 150], [25, 150]]:
				points.append(_p(center, xy[0], xy[1]))
		"diamond":
			for xy in [[100, 28], [172, 100], [100, 172], [28, 100]]:
				points.append(_p(center, xy[0], xy[1]))
		"circle":
			for index in range(20):
				var angle := TAU * float(index) / 20.0
				points.append(_p(center, 100.0 + cos(angle) * 60.0, 100.0 + sin(angle) * 60.0))
		"air_dome":
			# Open-bottomed dome: straight sides rising into a half ellipse.
			points.append(_p(center, 45, 150))
			for index in range(13):
				var angle := PI + PI * float(index) / 12.0
				points.append(_p(center, 100.0 + cos(angle) * 55.0, 80.0 + sin(angle) * 55.0))
			points.append(_p(center, 155, 150))
		"air_arch":
			for xy in [[45, 150], [45, 70], [100, 20], [155, 70], [155, 150]]:
				points.append(_p(center, xy[0], xy[1]))
	return points


static func _line(canvas: CanvasItem, center: Vector2, a: Vector2, b: Vector2, width := 1.5) -> void:
	canvas.draw_line(_p(center, a.x, a.y), _p(center, b.x, b.y), LINE_COLOR, width, true)


static func _stadium(center: Vector2, left: float, right: float, top: float, bottom: float) -> PackedVector2Array:
	var points := PackedVector2Array()
	var radius := (bottom - top) * 0.5
	var mid := (top + bottom) * 0.5
	for index in range(9):
		var angle := -PI * 0.5 + PI * float(index) / 8.0
		points.append(_p(center, right - radius + cos(angle) * radius, mid + sin(angle) * radius))
	for index in range(9):
		var angle := PI * 0.5 + PI * float(index) / 8.0
		points.append(_p(center, left + radius + cos(angle) * radius, mid + sin(angle) * radius))
	points.append(points[0])
	return points


static func _draw_function(canvas: CanvasItem, center: Vector2, function: String, frame: String, font: Font) -> void:
	var diamond := frame == "diamond"
	match function:
		"armour":
			# Track outline (oval).
			canvas.draw_polyline(_stadium(center, 50, 150, 80, 120), LINE_COLOR, 1.5, true)
		"sp_artillery":
			# Field-artillery dot over a tracked-mobility oval.
			canvas.draw_circle(_p(center, 100, 88), 15.0 * SCALE, LINE_COLOR)
			var base := 128.0 if diamond else 132.0
			canvas.draw_polyline(_stadium(center, 75, 125, base, base + 14), LINE_COLOR, 1.3, true)
		"air_defense":
			# Air-defence dome along the frame's base, missile standing in it.
			var base := 136.0 if diamond else 150.0
			var half := 34.0 if diamond else 50.0
			var arc := PackedVector2Array()
			for index in range(11):
				var angle := PI + PI * float(index) / 10.0
				arc.append(_p(center, 100.0 + cos(angle) * half, base + sin(angle) * half * 0.72))
			canvas.draw_polyline(arc, LINE_COLOR, 1.5, true)
			_missile(canvas, center, 62.0 if diamond else 60.0, base - 4.0, 7.0)
		"missile":
			_missile(canvas, center, 58.0, 142.0, 9.0)
		"recon":
			# Cavalry slash from lower left to upper right of the frame.
			if diamond:
				_line(canvas, center, Vector2(64, 136), Vector2(136, 64))
			else:
				_line(canvas, center, Vector2(25, 150), Vector2(175, 50))
		"engineer":
			var engineer := PackedVector2Array([
				_p(center, 60, 120), _p(center, 60, 82), _p(center, 140, 82), _p(center, 140, 120)])
			canvas.draw_polyline(engineer, LINE_COLOR, 1.6, true)
			_line(canvas, center, Vector2(100, 82), Vector2(100, 120), 1.6)
		"command":
			# Headquarters staff from the lower-left of the frame.
			var foot := Vector2(28, 100) if diamond else Vector2(25, 150)
			_line(canvas, center, foot, foot + Vector2(0, 55), 2.0)
		"attack_helicopter":
			# Rotary-wing bow tie with the attack modifier letter above.
			canvas.draw_colored_polygon(PackedVector2Array([
				_p(center, 62, 98), _p(center, 100, 114), _p(center, 62, 130)]), LINE_COLOR)
			canvas.draw_colored_polygon(PackedVector2Array([
				_p(center, 138, 98), _p(center, 138, 130), _p(center, 100, 114)]), LINE_COLOR)
			_letters(canvas, center, "A", Vector2(100, 70), 10, font)
		"fighter", "recon_aircraft", "cargo_aircraft":
			_letters(canvas, center, String(FUNCTIONS[function].letters), Vector2(100, 98), 13, font)
		"naval_surface":
			# Surface combatant hull with a mast.
			var hull := PackedVector2Array([
				_p(center, 62, 104), _p(center, 138, 104), _p(center, 126, 124), _p(center, 74, 124)])
			canvas.draw_colored_polygon(hull, LINE_COLOR)
			_line(canvas, center, Vector2(100, 104), Vector2(100, 78), 1.6)
			_line(canvas, center, Vector2(88, 86), Vector2(112, 86), 1.4)


static func _missile(canvas: CanvasItem, center: Vector2, top: float, bottom: float, half_width: float) -> void:
	var body := PackedVector2Array([
		_p(center, 100, top), _p(center, 100 + half_width, top + 16), _p(center, 100 + half_width, bottom),
		_p(center, 100 - half_width, bottom), _p(center, 100 - half_width, top + 16)])
	canvas.draw_colored_polygon(body, LINE_COLOR)


static func _letters(canvas: CanvasItem, center: Vector2, text: String, at: Vector2, font_size: int, font: Font) -> void:
	if font == null:
		return
	var origin := _p(center, at.x, at.y)
	var extent := font.get_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size)
	canvas.draw_string(font, origin + Vector2(-extent.x * 0.5, font_size * 0.36), text, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, LINE_COLOR)
