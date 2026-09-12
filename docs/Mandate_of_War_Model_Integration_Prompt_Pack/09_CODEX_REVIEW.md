# Codex Review — Model Integration

Review the completed Mandate of War processed-model integration. Inspect repository code, tests, visual definitions, unit mappings, and developer tooling.

Do not redesign working systems for style preference.

## Review Priorities
### Architecture
Flag raw GLB paths in gameplay logic, gameplay stats derived from visual data, dependence on donor node names, visual state mutating simulation, or excessive per-unit scene complexity.

### Performance
Look for per-unit `_process()`, thousands of physics bodies, triangle collision, unique materials per instance, all LODs visible, runtime mesh processing, repeated resource loads, signal storms, or transform-update bottlenecks.

### Identity
Replacing one visual's GLB must not alter health, armor, movement, cost, weapons, faction, orders, or replay identity.

### LOD / Strategic Zoom
Verify only intended LOD renders and the design has a path toward strategic icon/impostor representation at extreme zoom.

### Hardpoints / Projectiles
Verify stable hardpoint mappings, safe fallbacks, and compatibility with physical projectiles.

### Replay / Multiplayer
Ensure unit/content IDs—not local GLB paths—are serialized. Presentation-only animation/banking/turrets must not affect authoritative simulation.

### Modding
Verify future mods can register visual IDs, models, hardpoint mappings, and faction material settings without editing core code.

### Validation
Review the asset viewer/validator and automated coverage.

## Output
Return:
1. Critical issues
2. High-priority issues
3. Performance risks
4. Determinism/network risks
5. Asset-pipeline risks
6. Missing tests
7. Recommended fixes in implementation order

For every issue cite the exact file/class/function, explain practical impact, and propose the smallest useful correction. Avoid style-only churn.
