# Mandate UI Design System v2

The UI is native Godot presentation. It shares tokens, fonts and reusable
controls, but it does not own simulation state, resource accounting,
production, selection or command authority.

## Look

Near-future military command tooling. Graphite surfaces with an army-green
undertone, hairline borders, square corners, and three signature details:

- **Accent rail**: a 2 px line along the top of a panel, coloured by meaning
  (cyan for command, amber for construction/warning, green for selection or
  success, red for danger or defeat).
- **Corner brackets**: short L-shaped marks on panel corners, like a targeting
  frame. `MandatePanel.draw_panel_chrome` draws them for node panels and for the
  custom-drawn HUD, so both look the same.
- **Key caps**: every hotkey is shown as a small raised key (`MandateKeycap`),
  in the HUD, the build catalog, Strategic Command and the F1 sheet.

Cyan is reserved for command, focus and interactive affordances. Amber means
warning or construction, red means danger, green means friendly or success.
Corners stay square (radius 0); there are no gradients.

## Source of truth

- `ui/theme/ui_tokens.gd`: spacing (4/8/12/16/24/32), the type scale
  (`FONT_XS` 11 to `FONT_DISPLAY` 48), surfaces (`SURFACE_GLASS` for panels over
  the battlefield, `SURFACE_SCRIM` behind modals), borders, text, accent,
  resource and status colours.
- `ui/theme/mandate_fonts.gd`: three faces from installed system fonts, so the
  repository ships no font files.
  - `display()` for headings, numbers and values (Bahnschrift on Windows, DIN
    Alternate on macOS, Barlow/Roboto Condensed or Liberation Sans Narrow on
    Linux).
  - `label()` for small uppercase captions, tracked out by one pixel.
  - `body()` for sentences (Bahnschrift, Avenir Next, Segoe UI, then Noto/DejaVu).
- `ui/theme/mandate_theme.tres` (script `mandate_theme.gd`): styles every
  Control under a themed root by default (labels, buttons, option buttons, popup
  menus, tooltips, line edits, progress bars, separators, scroll bars). Variations:
  - Buttons: `PrimaryButton` (solid cyan, the one main action), `SecondaryButton`,
    `GhostButton` (low emphasis), `DangerButton` (destructive only), `TabButton`,
    `TabSelected`. All buttons have a separate cyan focus ring for keyboard use.
  - Panels: `CommandPanel`, `RaisedPanel`, `InsetPanel`, `GlassPanel`,
    `ModalPanel` (with drop shadow), `KeycapPanel`, `BadgePanel`.
  - Labels: `DisplayHeading`, `MajorHeading`, `SectionHeading`, `EyebrowLabel`
    (small cyan caption above a title), `BodyLabel`, `CaptionLabel`, `MutedLabel`,
    `StatLabel`, `CommandLabel`, `StrategicWarning`, `KeycapLabel`.

## Components (`ui/components/`)

- `MandatePanel`: surfaces `command`, `raised`, `inset`, `glass`, `modal`, with
  `brackets` and `rail_color`.
- `MandateButton` / `MandateCommandButton`: variants above, semantic `icon_id`,
  pointing-hand cursor, focusable.
- `MandateCard`: icon, title, subtitle, accent rail and a `body()` column.
- `MandateTabs`: underlined segmented tabs; emits `tab_selected`.
- `StatusBadge`, `ResourceCostDisplay`, `MandateTooltip`, `MandateSectionHeader`,
  `MandateKeycap`.

Use `theme_type_variation` and tokens first. Components update only when bound
data changes; no per-frame UI polling or shadowed gameplay state in GDScript.

Icons are semantic: `UiIconRegistry.get_icon(&"namespace.id")` for tintable UI
icons, `UiIconRegistry.get_nato_symbol(&"fighter", &"friendly")` for military
identification (Strategic Command cards use these).

## Screens

- **Briefing** (`main.tscn` StartupOverlay): eyebrow, scenario title, factions;
  SITUATION, OBJECTIVE and FIRST ORDERS sections in sentence case; one primary
  action, BEGIN OPERATION (focused, so Enter starts). The deterministic 2D
  controller is a ghost button labelled DETERMINISTIC TEST.
- **Tactical HUD** (`command_hud.gd`, custom drawn): operation and speed
  top-left; objective and logistics banner top-centre; materials, fuel and power
  with capacity bars top-right; selection card bottom-left with a segmented
  integrity bar and an orders reference above it; construction/FOB progress and
  the engineer build catalog bottom-centre; the inspect strip along the bottom
  with G / M / F1 key hints on the right.
- **Strategic Command** (G): a centred modal over a dimmed battlefield, not a
  full-screen takeover. Header with G/Esc hint and Close; GROUND/AIR tabs and a
  colour-coded status line; unit cards with NATO symbol, cost, build time, a
  short status badge and a full-width ORDER (PARADROP below when the unit can
  drop); an IN FLIGHT footer with fuel and recon requests.
- **Controls** (F1): modal sheet with key caps per key, grouped by category.
- **Audio & Credits** (F2): shared modal surface with four persistent channel
  sliders, scrollable sound credits, restore defaults and Close. Accessible
  before/during/after a match; tactical hotkeys are blocked while it is open.
- **After-action report**: result in large type coloured by outcome, reason,
  match facts, PLAY AGAIN focused as the default.
- **Magnifier** (M): bracketed inset with zoom readout and a centre reticle.

## Seeing every screen

`tools/capture_ui_tour.gd` saves a screenshot of each screen above (needs a
display, not `--headless`):

```bash
RTS_UI_TOUR_DIR=/tmp/ui_tour Godot --path godot/project --resolution 1920x1080 \
  --script res://tools/capture_ui_tour.gd
```

`res://ui/showcase/ui_design_system_showcase.tscn` is the component reference.

```bash
python3 tools/validate.py ui_design_system_showcase --rendered --require-screenshots --resolution 1920x1080
```
