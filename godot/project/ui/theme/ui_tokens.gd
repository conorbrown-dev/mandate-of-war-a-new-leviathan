class_name MandateTokens
extends RefCounted
## Design tokens for every Mandate UI surface: node-based panels and the
## custom-drawn tactical HUD alike. Use these instead of literal colours/sizes.

const SPACE_XS := 4
const SPACE_SM := 8
const SPACE_MD := 12
const SPACE_LG := 16
const SPACE_XL := 24
const SPACE_2XL := 32
# Square, sharp edges everywhere. Near-future character comes from hairline
# borders, accent rails and corner brackets, not rounded corners.
const RADIUS_SM := 0
const RADIUS_MD := 0
const RADIUS_LG := 0
const BORDER_WIDTH := 1
const RAIL_WIDTH := 2
const BRACKET_LENGTH := 10

# Type scale (logical pixels at the 1920x1080 reference).
const FONT_XS := 11
const FONT_SM := 13
const FONT_MD := 15
const FONT_LG := 18
const FONT_XL := 24
const FONT_2XL := 32
const FONT_DISPLAY := 48

# Graphite surfaces with an army-green undertone. Panels over the battlefield
# use the translucent GLASS tone so terrain stays readable behind them.
const SURFACE_BASE := Color("#0a0f0c")
const SURFACE_PANEL := Color("#121a15")
const SURFACE_RAISED := Color("#18231c")
const SURFACE_HOVER := Color("#203026")
const SURFACE_SELECTED := Color("#173532")
const SURFACE_INSET := Color("#070b09")
const SURFACE_GLASS := Color(0.047, 0.071, 0.059, 0.86)
const SURFACE_SCRIM := Color(0.016, 0.024, 0.02, 0.78)

const BORDER_SUBTLE := Color("#223028")
const BORDER_DEFAULT := Color("#33463b")
const BORDER_STRONG := Color("#4f6858")
const BORDER_FOCUS := Color("#3ddcff")

const TEXT_PRIMARY := Color("#e3ece6")
const TEXT_SECONDARY := Color("#a4b4aa")
const TEXT_MUTED := Color("#6f8277")
const TEXT_DISABLED := Color("#4a5a50")
const TEXT_ON_ACCENT := Color("#04161b")

# Cyan is reserved for command, focus and interactive affordances.
const ACCENT := Color("#3ddcff")
const ACCENT_STRONG := Color("#97ecff")
const ACCENT_DIM := Color(0.239, 0.863, 1.0, 0.16)
const ACCENT_FILL := Color("#0d3440")

const RESOURCE_MASS := Color("#4cb8ff")
const RESOURCE_ENERGY := Color("#ff9a3d")
const RESOURCE_FUEL := Color("#c6e85a")
const RESOURCE_POWER := Color("#ffb547")

const SUCCESS := Color("#6fe0a0")
const WARNING := Color("#ffb547")
const DANGER := Color("#ff5c5c")
const FRIENDLY := Color("#6fe0a0")
const HOSTILE := Color("#ff5c5c")
const NEUTRAL := Color("#ffb547")
const DISABLED_ALPHA := 0.45
