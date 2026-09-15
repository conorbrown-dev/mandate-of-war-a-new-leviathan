# Mandate UI Design System v1

The UI system is native Godot presentation. It shares visual tokens and reusable controls, but it does not own simulation state, resource accounting, production, selection, or command authority.

## Source of truth

- `godot/project/ui/theme/ui_tokens.gd` holds the compact token set: 4/8/12/16/24/32 spacing, 2/4/8 radii, surfaces, borders, text hierarchy, accent, success, warning, danger, and disabled alpha.
- `godot/project/ui/theme/mandate_typography.gd` defines display, heading, section, body, label, and stat roles. Use roles instead of ad-hoc sizes when a component can express one.
- `godot/project/ui/theme/mandate_theme.tres` is the project-native `Theme` resource. Its script defines `PrimaryButton`, `SecondaryButton`, `GhostButton`, `DangerButton`, `CommandPanel`, `RaisedPanel`, `InsetPanel`, `SectionHeading`, `MutedLabel`, and `StatLabel` variations.

The visual language is restrained tactical tooling: dark blue-black surfaces, thin cyan/blue borders, strong title hierarchy, and amber/red only for cost pressure or destructive actions. Avoid gradients, oversized rounded cards, and generic dashboard chrome.

## Components

`godot/project/ui/components/` contains small native controls:

- `MandateButton`: primary, secondary, ghost, and danger action variants.
- `MandatePanel` and `MandateCard`: command, raised, and inset information surfaces.
- `MandateTabs`: an explicit selected command category.
- `StatusBadge`, `ResourceCostDisplay`, `MandateTooltip`, and `MandateSectionHeader`: reusable status, cost, help, and hierarchy primitives.

Use `theme_type_variation` and tokens first. Components may update only when bound data changes; do not add per-frame UI polling or shadow gameplay state in GDScript.

Icons are semantic and compact: resources use their existing wrench/bolt/research meaning; commands and unit roles should use distinct simple silhouettes with text tooltips. Faction identity is intentionally data-driven: map faction color and emblem assets through a single future faction presentation resolver, rather than baking faction-specific colors into components.

## Showcase and validation

`res://ui/showcase/ui_design_system_showcase.tscn` is the reference composition. It uses realistic Strategic Command requisition data: category tabs, interceptor/strike/recon cards, availability/research states, material/energy costs, actions, and a delivery explanation.

During `reinforcement_delivery_smoke`, complete an airfield first; native simulation selects the sole friendly airfield automatically. Press `G` to open Strategic Command and request the package directly. If multiple friendly airfields are active, **Select Airfield** enters the map-click route and accepts only a clicked airfield; terrain is not a delivery target. `Esc` or **Close** dismisses the panel. The panel is intentionally unavailable until an airfield-backed delivery operation is configured.

Run it directly:

```bash
./Godot_v4.7.2-stable_linux.x86_64 --path godot/project ui/showcase/ui_design_system_showcase.tscn
```

Run the repository validation scenario with a rendered checkpoint:

```bash
python3 tools/validate.py ui_design_system_showcase --rendered --require-screenshots --resolution 1920x1080
```

The existing tactical `command_hud.gd` now consumes the central color tokens while retaining its intentionally custom draw path. This is the v1 migration seam; do not rewrite the HUD solely to adopt a standard `Control` tree.
