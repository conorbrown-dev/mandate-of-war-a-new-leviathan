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

## AI styles and difficulty

`data/ai_doctrines.json` lists the AI's styles (`doctrines`) and difficulty
levels (`difficulties`). A style decides what the AI wants to do: wave sizes, a
home guard that never leaves, how far out it defends, which units it buys
(`cheapest`, `strongest` or `mixed`), how many scouts it keeps, how often it
flies recon, whether it saves for the next flight, and whether it waits to find
the enemy before attacking. A difficulty decides how well it does it: how often
it thinks, how long a new contact takes to register, how many unit orders it gives
per decision (0 for no limit), when damaged units head home (0 for never) and how
soon a scout is sent somewhere new.

Neither may grant extra resources or vision. A style whose wave numbers are 0
uses the scenario's `ai.assault` numbers. Every field is required, and an
unknown key, an out-of-range value or a default that names nothing stops the
game from loading the file. A scenario picks its enemy with `ai.doctrine` and
`ai.difficulty`; the `RTS_AI_DOCTRINE` and `RTS_AI_DIFFICULTY` environment
variables override it.

## Match records

Each finished match is written as JSON to `user://match_records/` (or
`RTS_MATCH_RECORD_DIR`). A record holds:

- `info`: the scenario, the AI style and difficulty, the factions and start time.
- `commands`: every order that ran, as `[tick, faction, type, entity, x, y, extra]`.
  `x` and `y` are the encoded command values; divide by `command_position_scale`
  for meters.
- `actions`: calls that changed the match outside the command queue, such as
  queueing a structure or calling in a delivery, with their arguments.
- `state_hashes`: a hash of the simulation every second. A playback must match
  every one.
- `samples`: every 10 seconds, each faction's units, fighting units, materials
  and army centre.
- the result, match length and the tick each side first sighted the enemy.
