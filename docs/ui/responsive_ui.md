# Responsive UI guide

## Reference resolution and scaling

Mandate of War uses a **1920x1080 logical UI reference**. `project.godot` uses Godot canvas-item stretching with `expand` aspect behavior, allowing wider monitors to expose extra space without stretching central panels across the display. The 3D renderer is not changed by the UI-scale preference.

`UiScaleSettings` is an autoloaded, persistent player preference. It supports 75%, 90%, 100%, 110%, 125%, 150%, 175%, and 200% through `set_ui_scale(scale)`. It applies Godot Window content scaling rather than manually scaling `UIRoot`; renderer resolution and quality options remain independent.

## Layout rules

Do use full-rect roots, semantic anchors, containers for related node-based controls, `custom_minimum_size`, size flags, the Mandate theme, and SVG assets. Use `ResponsiveUi` for shared local metrics in a custom-drawn HUD.

Do not hardcode screen coordinates for regular HUD controls, scale the UI root manually, create resolution-specific scenes, duplicate font overrides, or rebuild layouts from `_process`.

`UIRoot` is the game HUD's full-rect, input-transparent root. Add top, bottom, left, right, center, modal, and tooltip surfaces under it according to their semantic screen position. Keep world-space UI and camera projections outside this hierarchy.

## Panels, typography, and icons

Use `res://ui/theme/mandate_theme.tres` and its `BodyLabel`, `CommandLabel`, `SectionHeading`, `MajorHeading`, `StrategicWarning`, `MutedLabel`, and `StatLabel` variations. Spacing comes from `MandateTokens` (`SPACE_XS` through `SPACE_2XL`).

Use SVG icon resources at logical sizes: 16-20 for tiny status, 24 for inline, 28-36 for commands, and 48+ for strategic identity. Resolve command assets with semantic IDs, for example `UiIconRegistry.get_icon(&"order.move")`; do not hardcode generated SVG paths. Unit identification uses `UiIconRegistry.get_nato_symbol(&"fighter", &"friendly")`.

## Adding UI

1. Add a Control under the appropriate anchored `UIRoot` region, or a container beneath an existing panel.
2. Set a meaningful minimum size and theme variation; use token spacing instead of ad-hoc margins.
3. Set `mouse_filter` deliberately. Decorative controls must ignore input.
4. For a new command control, use `MandateCommandButton`, set `icon_id`, and provide its hotkey text/tooltip.
5. Run `res://test_responsive_ui.gd`, the focused HUD test, and inspect 1280x720, 1920x1080, 2560x1440, 3440x1440, and 3840x2160 when a graphical display is available.

## Validation

`test_responsive_ui.gd` verifies project scaling settings, reference/HD/ultrawide metrics, the input-transparent full-rect HUD root, and centered startup dialog anchors. It is a lightweight architectural guard, not a screenshot replacement. Use the existing rendered validation scenario for visual evidence when a graphical display is available.
