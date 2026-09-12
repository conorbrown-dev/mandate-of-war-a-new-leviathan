class_name UiTypography
extends RefCounted

## Keep the tactical UI legible and consistent across Linux, Windows, and macOS.
const CONTROL_FONT_SIZE_BUMP := 2

static func monospace_font() -> SystemFont:
	var font := SystemFont.new()
	font.font_names = PackedStringArray(["ui-monospace", "SFMono-Regular", "Consolas", "Liberation Mono", "monospace"])
	return font


static func compact_tactical_font() -> SystemFont:
	# Build cards carry costs and hotkeys at a much denser cadence than the
	# briefing/HUD text. Prefer installed condensed faces, with a neutral sans
	# fallback, to preserve readability without consuming tactical screen space.
	var font := SystemFont.new()
	font.font_names = PackedStringArray(["Liberation Sans Narrow", "DejaVu Sans Condensed", "Arial Narrow", "sans-serif"])
	return font


static func apply_to(root: Node) -> void:
	var font := monospace_font()
	_apply_recursive(root, font)


static func _apply_recursive(node: Node, font: Font) -> void:
	if node is Control:
		var control := node as Control
		control.add_theme_font_override("font", font)
		control.add_theme_font_size_override(
			"font_size",
			control.get_theme_font_size("font_size") + CONTROL_FONT_SIZE_BUMP
		)
	if node is Label:
		var label := node as Label
		label.text = label.text.to_upper()
	elif node is Button:
		var button := node as Button
		button.text = button.text.to_upper()
	for child in node.get_children():
		_apply_recursive(child, font)
