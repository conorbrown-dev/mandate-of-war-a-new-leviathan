extends RefCounted
## Turns a map position into the words a controller would say. The simulation's
## +x is east and +y is south (Godot -Z is north), so bearing 000 is -y and
## bearings run clockwise. Nothing here invents a position: callers pass only
## what the native report carried.

const CARDINALS := ["north", "northeast", "east", "southeast", "south", "southwest", "west", "northwest"]
const CARDINAL_CODES := ["n", "ne", "e", "se", "s", "sw", "w", "nw"]
const DIGITS := ["zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "niner"]
const PHONETIC := ["Alpha", "Bravo", "Charlie", "Delta", "Echo", "Foxtrot", "Golf", "Hotel", "India",
	"Juliett", "Kilo", "Lima", "Mike", "November", "Oscar", "Papa", "Quebec", "Romeo", "Sierra",
	"Tango", "Uniform", "Victor", "Whiskey", "X-ray", "Yankee", "Zulu"]
const SMALL := ["zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten",
	"eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen", "seventeen", "eighteen", "nineteen"]
const TENS := ["", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety"]


## Whole degrees 0-359 from `from` to `to`, 0 = north, clockwise.
static func bearing_degrees(from: Vector2, to: Vector2) -> int:
	var offset := to - from
	if offset.length_squared() < 0.0001:
		return 0
	return posmod(roundi(rad_to_deg(atan2(offset.x, -offset.y))), 360)


static func cardinal_index(bearing: int) -> int:
	return posmod(roundi(float(bearing) / 45.0), 8)


static func cardinal(bearing: int) -> String:
	return CARDINALS[cardinal_index(bearing)]


static func cardinal_code(bearing: int) -> String:
	return CARDINAL_CODES[cardinal_index(bearing)]


## Three spoken digits: 45 -> "zero-four-five".
static func spoken_bearing(bearing: int) -> String:
	var value := posmod(bearing, 360)
	return "%s-%s-%s" % [DIGITS[value / 100], DIGITS[(value / 10) % 10], DIGITS[value % 10]]


## Clock direction of `to` for a unit at `from` facing `heading` degrees.
static func clock_direction(from: Vector2, heading: int, to: Vector2) -> int:
	var relative := posmod(bearing_degrees(from, to) - heading, 360)
	var hour := posmod(roundi(float(relative) / 30.0), 12)
	return 12 if hour == 0 else hour


static func spoken_clock(hour: int) -> String:
	return "%s o'clock" % SMALL[clampi(hour, 1, 12)]


## 0-999 as words: 80 -> "eighty", 115 -> "one hundred fifteen".
static func spoken_number(value: int) -> String:
	value = clampi(value, 0, 999)
	if value < 20:
		return SMALL[value]
	if value < 100:
		return TENS[value / 10] + ("" if value % 10 == 0 else "-" + SMALL[value % 10])
	var rest := value % 100
	return SMALL[value / 100] + " hundred" + ("" if rest == 0 else " " + spoken_number(rest))


## Whole kilometres, never less than one.
static func range_km(from: Vector2, to: Vector2) -> int:
	return maxi(1, roundi(from.distance_to(to) / 1000.0))


## Grid cell of a position: columns are lettered west to east, rows numbered
## north to south from one. Returns {} off the map.
static func grid_cell(position: Vector2, map: Rect2, cell_m: float) -> Dictionary:
	if cell_m <= 0.0 or not map.has_area() or not map.grow(0.01).has_point(position):
		return {}
	var column := mini(int(floor((position.x - map.position.x) / cell_m)), int(ceil(map.size.x / cell_m)) - 1)
	var row := mini(int(floor((position.y - map.position.y) / cell_m)), int(ceil(map.size.y / cell_m)) - 1)
	return {"column": column, "row": row}


static func column_name(column: int) -> String:
	if column < PHONETIC.size():
		return PHONETIC[column]
	return PHONETIC[(column / PHONETIC.size() - 1) % PHONETIC.size()] + " " + PHONETIC[column % PHONETIC.size()]


## "Delta-Seven" for column 3, row 6.
static func spoken_grid(cell: Dictionary) -> String:
	if cell.is_empty():
		return ""
	return "%s-%s" % [column_name(int(cell.column)), spoken_number(int(cell.row) + 1).capitalize().replace(" ", "-")]


## Short written form for a label: "D7".
static func grid_label(cell: Dictionary) -> String:
	if cell.is_empty():
		return ""
	var column := int(cell.column)
	var letters := char(65 + column % 26)
	if column >= 26:
		letters = char(65 + (column / 26 - 1) % 26) + letters
	return "%s%d" % [letters, int(cell.row) + 1]
