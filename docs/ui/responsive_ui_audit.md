# Responsive UI audit

## Current architecture

`main.tscn` is the gameplay entry scene. Its `HUD` CanvasLayer now owns a full-rect, themed `UIRoot`; the custom-drawn `CommandHUD`, strategic screen-projection overlay, selection rectangle, debug surface, startup overlay, and territory label are children of that root. The Strategic Command scene is instantiated into the same root at runtime. World-space labels, unit range rings, and strategic markers remain camera-projected presentation and are not HUD-layout candidates.

Other UI surfaces are intentionally separate: `ui/showcase/ui_design_system_showcase.tscn` is the Strategic Command reference composition, `map_editor.tscn` is a centered container-based tool, `native_skirmish.gd` has a legacy dynamic HUD, and `visual_asset_viewer.tscn` has a CanvasLayer with a fixed inspector panel.

## Existing foundation

- Project scaling is canvas-item based, with a 1920x1080 logical reference and `expand` aspect behavior.
- `ui/theme/mandate_theme.tres`, `mandate_theme.gd`, `ui_tokens.gd`, and `mandate_typography.gd` are the shared theme and semantic typography foundation.
- Command/UI assets are SVGs resolved through `UiIconRegistry`; NATO symbols are generated SVGs under `assets/ui/symbols/nato`.
- `CommandHUD` and `HotkeyDisplay` are custom CanvasItem renderers. They use a shared 1920x1080 logical scale and redraw only when their state or size changes.

## Hardcoded layout risk

The main risk was the tactical HUD's logical pixel deck geometry and corresponding build-card hit test. It is appropriate for the custom-draw renderer, but both drawing and hit testing must use the same responsive metric. They now share `ui/responsive_ui.gd`.

The startup dialog uses centered anchors with bounded offsets and a VBoxContainer, so it is low risk. Debug controls and the legacy native-skirmish HUD still contain absolute offsets; they are developer/legacy surfaces and should be migrated after the gameplay HUD, not blindly rewritten. World-to-screen calculations in strategic overlays, selection rectangles, labels, and validation input are intentional and are not responsive-layout debt.

## Highest-risk surfaces and migration order

1. Core `CommandHUD` and its build-card hit boxes.
2. Strategic Command/requisition panels as they gain real catalog content.
3. Tooltips, notifications, and the future minimap.
4. Startup/menu dialog typography and max widths.
5. Debug, map-editor, visual-viewer, and native-skirmish legacy surfaces.

New HUD work must use `UIRoot` anchors or containers for node-based controls. Custom-drawn controls use `ResponsiveUi` only for local logical metrics; they must not add resize polling in `_process`.
