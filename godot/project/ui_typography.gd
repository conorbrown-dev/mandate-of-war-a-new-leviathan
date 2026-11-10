class_name UiTypography
extends RefCounted

## Compatibility shim over the Mandate type system (ui/theme/mandate_fonts.gd).
## Fonts come from the shared Theme; this only keeps older call sites working.
const Fonts := preload("res://ui/theme/mandate_fonts.gd")


static func monospace_font() -> Font:
	return Fonts.label()


static func compact_tactical_font() -> Font:
	return Fonts.display()


## Short labels and buttons in a scene read as uppercase command text. Fonts and
## sizes are left to the Theme so every surface shares one type scale.
static func apply_to(root: Node) -> void:
	if root is Button:
		(root as Button).text = (root as Button).text.to_upper()
	for child in root.get_children():
		apply_to(child)
