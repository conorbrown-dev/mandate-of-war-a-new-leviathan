# Game Content Formats

Game content uses open formats: JSON, YAML, PNG, JPEG, SVG and glTF binary. Do
not add custom binary content. Game replays are saved output, not content.

## Fuel depots

`godot/project/scenarios/fuel_depots.json` lists each depot with its `id`, `name`,
`kind` (`civilian` or `military`), `position` and `stock`. An optional `owner` is
the id of the faction that holds it at the start. Ids must not match a material
site.

## Units and factions

`data/unit_faction_stats.json` is the only definition of units and factions.
There is no fallback copy in code, and missing or invalid data stops the game
with an error. To add a unit, add one entry to `unit_types`; no code changes are
needed unless it needs a new `visual_id`, which is defined in
`godot/project/visuals/visual_definitions.json`.

| Field | Meaning |
| --- | --- |
| `id` | Unique integer from 0 to 255, used in commands and scenarios |
| `type` | Unique text key, used by other data files |
| `faction` | Owning faction key |
| `material_cost`, `build_time_seconds`, `hp`, `speed` | Core stats |
| `view_range` | Sight. A unit only fires at what it can see |
| `weapon` | `damage`, `range`, `fire_rate` (seconds between shots), `projectile_speed`, `aoe_radius`, `lead_bias` |
| `capabilities` | Territorial actions |
| `construction_unit` | True for units that need a buildable site |
| `nato_symbol` | Build catalog symbol |
| `strategic_icon` | One of `mbt`, `artillery`, `air_defense`, `swarm`, `assault`, `missile`, `engineer`, `fighter`, `vtol`, `boat` |

Set `view_range` at least as high as `weapon.range`, or the extra range is unused.

`faction_start_data` holds faction names, starting resources and cost modifiers.
`data/research_projects.json` is content for strategic authorization, which
happens outside the skirmish.

## Terrain

`tools/generate_terrain.py` writes both files to `godot/project/scenarios/`:

- `terrain.json`: heights in meters, as `{"format":"heightfield","version":1,
  "width":W,"height":H,"heights":[...]}` in row order.
- `terrain_masks.png`: RGBA, where R is rivers, G is forest density, B is baked
  light and A is ground variation.

Maps exported by the map editor use the same `terrain.json` format beside their
YAML files.

## Balance

`data/balance_matchups.json` sets the budget and each faction's standard army.
`rts_balance_benchmark` fights every pairing on equal budgets from both sides of
the field, and runs in CTest as `faction_balance`. A score is the value left by
the first faction minus the second, as a share of the budget. The test fails
beyond `tolerance`. Scores can move sharply on small changes, so rerun it after
any unit data edit.
