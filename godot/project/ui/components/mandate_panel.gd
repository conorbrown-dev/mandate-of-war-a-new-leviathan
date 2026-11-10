class_name MandatePanel
extends PanelContainer
## A themed surface. Command and modal surfaces carry the Mandate signature:
## short corner brackets and an optional accent rail along the top edge.

const PanelTokens = preload("res://ui/theme/ui_tokens.gd")

@export_enum("command", "raised", "inset", "modal", "glass") var surface := "command":
	set(value):
		surface = value
		_apply_surface()

## Draw targeting-style brackets on the four corners.
@export var brackets := true:
	set(value):
		brackets = value
		queue_redraw()

## A coloured rail along the top edge; transparent for none.
@export var rail_color := Color(0, 0, 0, 0):
	set(value):
		rail_color = value
		queue_redraw()


func _ready() -> void:
	_apply_surface()


func _apply_surface() -> void:
	theme_type_variation = {"command": "CommandPanel", "raised": "RaisedPanel", "inset": "InsetPanel", "modal": "ModalPanel", "glass": "GlassPanel"}.get(surface, "CommandPanel")
	queue_redraw()


func _draw() -> void:
	draw_panel_chrome(self, Rect2(Vector2.ZERO, size), rail_color, brackets and surface in ["command", "modal", "glass"])


## Shared by node panels and the custom-drawn HUD so both share one look.
static func draw_panel_chrome(canvas: CanvasItem, rect: Rect2, rail: Color, with_brackets: bool, bracket_color: Color = PanelTokens.BORDER_STRONG) -> void:
	if rail.a > 0.0:
		canvas.draw_rect(Rect2(rect.position, Vector2(rect.size.x, PanelTokens.RAIL_WIDTH)), rail, true)
	if not with_brackets:
		return
	var length := minf(PanelTokens.BRACKET_LENGTH, minf(rect.size.x, rect.size.y) * 0.25)
	var a := rect.position
	var b := Vector2(rect.end.x - 1.0, rect.position.y)
	var c := rect.end - Vector2.ONE
	var d := Vector2(rect.position.x, rect.end.y - 1.0)
	for corner in [[a, Vector2.RIGHT, Vector2.DOWN], [b, Vector2.LEFT, Vector2.DOWN], [c, Vector2.LEFT, Vector2.UP], [d, Vector2.RIGHT, Vector2.UP]]:
		var origin: Vector2 = corner[0]
		if rail.a > 0.0 and origin.y <= rect.position.y + 0.5:
			continue
		canvas.draw_line(origin, origin + corner[1] * length, bracket_color, 2.0)
		canvas.draw_line(origin, origin + corner[2] * length, bracket_color, 2.0)
