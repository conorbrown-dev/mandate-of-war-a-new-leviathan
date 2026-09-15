extends SceneTree

const ResponsiveUi = preload("res://ui/responsive_ui.gd")
const CommandButton = preload("res://ui/components/command_button.gd")
const UiTokens = preload("res://ui/theme/ui_tokens.gd")
const MandateTheme = preload("res://ui/theme/mandate_theme.gd")

var checks := 0
var failures := 0


func check(condition: bool, message: String) -> void:
	checks += 1
	if not condition:
		failures += 1
		push_error(message)


func _initialize() -> void:
	var settings: Node = root.get_node_or_null("UiScaleSettings")
	check(settings != null, "UI scale settings autoload is available")
	check(settings != null and settings.get_supported_scales().has(settings.get_ui_scale()), "UI scale is normalized to a supported persistent preference")
	check(ProjectSettings.get_setting("display/window/size/viewport_width") == 1920 and ProjectSettings.get_setting("display/window/size/viewport_height") == 1080, "project uses the 1920x1080 logical UI reference")
	check(ProjectSettings.get_setting("display/window/stretch/mode") == "canvas_items" and ProjectSettings.get_setting("display/window/stretch/aspect") == "expand", "project uses canvas-item scaling with expand aspect")
	check(is_equal_approx(ResponsiveUi.layout_scale(Vector2(1920, 1080)), 1.0), "reference layout has unit scale")
	check(is_equal_approx(ResponsiveUi.layout_scale(Vector2(1280, 720)), 2.0 / 3.0), "720p layout scales proportionally")
	check(is_equal_approx(ResponsiveUi.layout_scale(Vector2(2560, 1440)), 1.0), "QHD layout retains the reference HUD geometry")
	check(is_equal_approx(ResponsiveUi.layout_scale(Vector2(3440, 1440)), 1.0), "ultrawide layout retains its logical panel scale")
	check(is_equal_approx(ResponsiveUi.layout_scale(Vector2(3840, 2160)), 1.0), "4K layout retains the reference HUD geometry")
	check(ResponsiveUi.centered_origin(Vector2(3440, 1440), ResponsiveUi.PANEL_MAX_WIDTH).x > 1000.0, "ultrawide command deck remains centered rather than stretched")
	check(UiTokens.RADIUS_SM == 0 and UiTokens.RADIUS_MD == 0 and UiTokens.RADIUS_LG == 0 and UiTokens.SURFACE_PANEL.g > UiTokens.SURFACE_PANEL.r and UiTokens.SURFACE_PANEL.g > UiTokens.SURFACE_PANEL.b, "UI tokens use sharp-edged army-green card surfaces")
	var theme := MandateTheme.new()
	var primary_style := theme.get_stylebox("normal", "PrimaryButton") as StyleBoxFlat
	var panel_style := theme.get_stylebox("panel", "CommandPanel") as StyleBoxFlat
	check(primary_style != null and panel_style != null and primary_style.corner_radius_top_left == 0 and panel_style.corner_radius_top_left == 0, "themed buttons and windows render with square corners")
	var view: Node3D = load("res://main.tscn").instantiate()
	root.add_child(view)
	await process_frame
	var ui_root: Control = view.get_node_or_null("HUD/UIRoot") as Control
	var command_hud: Control = view.get_node_or_null("HUD/UIRoot/CommandHUD") as Control
	var startup_panel: Control = view.get_node_or_null("HUD/UIRoot/StartupOverlay/Panel") as Control
	check(ui_root != null and is_equal_approx(ui_root.anchor_right, 1.0) and is_equal_approx(ui_root.anchor_bottom, 1.0) and ui_root.mouse_filter == Control.MOUSE_FILTER_IGNORE, "HUD has a full-rect non-blocking themed root")
	check(command_hud != null and is_equal_approx(command_hud.anchor_right, 1.0) and is_equal_approx(command_hud.anchor_bottom, 1.0) and command_hud.mouse_filter == Control.MOUSE_FILTER_IGNORE, "custom tactical HUD remains full-rect and input-transparent")
	check(startup_panel != null and is_equal_approx(startup_panel.anchor_left, 0.5) and is_equal_approx(startup_panel.anchor_top, 0.5), "startup dialog remains center anchored")
	var command_button := CommandButton.new()
	root.add_child(command_button)
	await process_frame
	check(command_button.custom_minimum_size.x >= 96.0 and command_button.custom_minimum_size.y >= 44.0, "reusable command button has a stable logical hit area")
	command_button.queue_free()
	view.queue_free()
	if failures > 0:
		quit(1)
		return
	print("RESPONSIVE_UI checks=%d failures=0" % checks)
	quit(0)
