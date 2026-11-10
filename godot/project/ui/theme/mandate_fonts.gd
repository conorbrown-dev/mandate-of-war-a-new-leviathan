class_name MandateFonts
extends RefCounted
## The three Mandate type faces, resolved from installed system fonts so the
## repository ships no font files. Each list prefers a DIN-style engineering
## face (Bahnschrift on Windows, DIN Alternate on macOS) and falls back to a
## neutral sans on Linux.
##   display: headings, numbers and values.
##   label:   small uppercase captions, tracked out for legibility.
##   body:    sentences and descriptions.

const DISPLAY_NAMES := ["Bahnschrift", "DIN Alternate", "Barlow Semi Condensed", "Roboto Condensed", "Arial Narrow", "Liberation Sans Narrow", "DejaVu Sans Condensed", "sans-serif"]
const BODY_NAMES := ["Bahnschrift", "Avenir Next", "Segoe UI", "Inter", "Roboto", "Noto Sans", "DejaVu Sans", "sans-serif"]

static var _display: Font
static var _label: Font
static var _body: Font


static func display() -> Font:
	if _display == null:
		var font := SystemFont.new()
		font.font_names = PackedStringArray(DISPLAY_NAMES)
		font.font_weight = 600
		font.antialiasing = TextServer.FONT_ANTIALIASING_GRAY
		_display = font
	return _display


static func label() -> Font:
	if _label == null:
		var variation := FontVariation.new()
		variation.base_font = display()
		variation.spacing_glyph = 1
		_label = variation
	return _label


static func body() -> Font:
	if _body == null:
		var font := SystemFont.new()
		font.font_names = PackedStringArray(BODY_NAMES)
		font.font_weight = 400
		font.antialiasing = TextServer.FONT_ANTIALIASING_GRAY
		_body = font
	return _body
