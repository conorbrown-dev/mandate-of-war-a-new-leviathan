# Tactical Resource Model Migration

## Phase 1 boundary

| Old concept | Phase 1 tactical concept |
| --- | --- |
| `Material` / metal storage | Materials |
| `Energy` wallet | Power generation/demand/reserve |
| `Research` wallet | Strategic-only; absent from new tactical state |
| none | Fuel current/capacity |

`TacticalResources` is the new authoritative C++ model used by new tactical
systems, deterministic skirmish checksums, and the GDExtension resource
snapshot. It contains Materials, Fuel, and Power only.

## Temporary compatibility

- `Storage` retains legacy `metal_*`, `energy_*`, and `research_*` fields so
  existing production queues, content, tests, and HUD can remain runnable.
  `ProductionManager::add_storage` initializes canonical Materials from metal
  and Fuel capacity from the legacy energy capacity. New code must use
  `Storage::tactical` / `ProductionManager::tactical_resources`.
- The per-entity `Energy` and `Research` components have been removed. The
  old off-road Energy drain is intentionally inactive until its intended Fuel
  behavior is designed in a later phase.
- Content `research_cost` remains parseable migration debt but is ignored by
  unit and structure construction. Research queues, command handling, AI
  behavior, and project presentation remain migration debt. They do not
  participate in the Phase-1 deterministic tactical resource checksum or
  resource snapshot.

## Phase 2 production migration

Production, construction queues, requisitions, and tactical extraction now use
canonical Materials. Legacy Energy is no longer a unit, structure, or road
purchase requirement; it remains only for unconverted logistics and HUD
callers. Legacy `ENERGY` resource nodes now fill canonical Fuel. Tactical
`RESEARCH` nodes are ignored rather than becoming a spendable balance.

The native economy API now also exposes `economy_add_tactical_storage` and
`economy_get_tactical_resources`; the GDExtension mirrors the latter.

## Phase 2 Godot tactical presentation migration

The active Godot tactical consumers now use the canonical seven-value
`economy_get_tactical_resources` array: Materials current/capacity, Fuel
current/capacity, and Power generation/demand/reserve. `main.gd` seeds storage
through `economy_add_tactical_storage`, passes canonical values to
`command_hud.gd`, and the HUD presents MATERIALS, FUEL, and POWER only. Power
uses a green reserve when non-negative and the existing danger color when in
deficit. `native_skirmish.gd` consumes the matching canonical skirmish snapshot.

Migrated Godot consumers: `main.gd`, `command_hud.gd`, `native_skirmish.gd`,
`test_skirmish.gd`, `test_native_skirmish.gd`, `test_hud_bridge.gd`,
`test_gdextension_boundary.gd`, and `validation/validation_runner.gd`.
`economy_get_storage_info` and
`economy_add_storage` are no longer bound to Godot; the old native C helpers
remain only for the legacy benchmark/native compatibility surface and will be
removed with that native API migration. They are not a tactical presentation
path. Build catalog `energy` fields, queued legacy cost display, and research
project interfaces remain later content-cost/research-workflow debt and are not
displayed as tactical resource balances.

Naval recovery still spends its legacy logistics budget. Its direct Materials
deduction now mirrors into `Storage::tactical.materials` so canonical snapshots
remain accurate; the later fuel-logistics phase removes this narrow bridge.

## Later phases

1. Migrate logistics, AI, content costs, and native economy C APIs, then delete
   the remaining legacy storage fields.
2. Remove tactical research commands, queues, and `research_cost`; add the
   strategic authorization handoff.
3. Remove the remaining native legacy storage C helpers and migrate content
   cost terminology after the active tactical presentation boundary is stable.
4. Add fuel consumption/transfer and power-priority behavior only after the
   canonical consumers are complete.
