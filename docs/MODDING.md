# Modding

> **Goal 05 status (2026-09-12):** A manifest-declared generated unit is now
> schema-validated with its placeholder mesh, assigned a stable SHA-256 content
> handle, and spawnable through the development simulation bridge. Dependency
> ordering/version enforcement, replacement semantics, and scripts remain
> unimplemented acceptance work; the Lua sections below are design direction,
> not a shipped runtime.

## Objective

Treat modding as a first-class requirement. Design mods to add:

- units
- factions
- weapons
- projectiles
- technologies/doctrines
- maps
- balance changes
- AI behaviors
- UI extensions where safe
- visual assets
- game modes

## Content ID System

### Content ID Format

Stable content IDs are the foundation of the modding system. They must be:

- deterministic: same string always produces same ID
- collision-resistant: different content rarely produces same ID
- reproducible: same input always yields same output
- portable: consistent across platforms

**Format:** full `SHA-256(content_type|namespace|identifier)` hex digest. The
full digest avoids a truncation collision boundary; re-registering the same
canonical identity is idempotent rather than a collision.

**Example:** `unit|faction_a|t1_interceptor` → `b7a38f2e`

### Content ID Prefixes

- `unit|`: unit definitions
- `faction|`: faction definitions
- `weapon|`: weapon/proyectile definitions
- `tech|`: technology/doctype definitions
- `map|`: map definitions
- `script|`: script definitions (if scripting is enabled)

### Registries

All content IDs must be registered in a content registry:

```cpp
struct ContentRegistry {
    std::unordered_map<std::string, ContentHandle> registry;
    std::unordered_set<std::string> collision_ids;
    
    ContentHandle register_content(const std::string& type, const std::string& namespace_id, const std::string& identifier);
    bool has_collision(const std::string& id) const;
    ContentHandle get_handle(const std::string& id) const;
};
```

## Manifest System

### Mod Manifest Format (YAML)

```yaml
manifest_version: "1.0"
id: "mymod"
name: "My Mod"
version: "1.0.0"
description: "A mod that adds new units"
author: "Author Name"
dependencies:
  - id: "base_content"
    version: ">=1.0.0"
  - id: "expansion_pack_a"
    version: ">=2.0.0"
content:
  units:
    - path: "units/new_unit.yaml"
      replace: false  # if true, replaces base content
  factions:
    - path: "factions/new_faction.yaml"
  weapons:
    - path: "weapons/new_weapon.yaml"
  maps:
    - path: "maps/new_map.yaml"
  scripts:
    - path: "scripts/new_script.lua"
balance:
  units:
    unit|faction_a|t1_interceptor:
      speed: 250  # override base value
```

### Manifest Loading

```cpp
struct ModManifest {
    std::string id;
    std::string version;
    std::vector<Dependency> dependencies;
    std::vector<ContentEntry> content;
    std::unordered_map<std::string, std::any> metadata;
};

class ModManager {
    std::vector<ModManifest> loaded_mods;
    std::unordered_set<std::string> loaded_ids;
    
    ModManifest load_manifest(const fs::path& mod_dir);
    bool validate_dependencies(const ModManifest& manifest);
    void apply_mod_content(const ModManifest& manifest);
};
```

## Dependency Resolution

### Dependency Syntax

- `id`: required content ID
- `version`: version constraint (semantic versioning)
  - `>=1.0.0`: at least version 1.0.0
  - `<=2.0.0`: at most version 2.0.0
  - `~1.5.0`: version 1.5.0 or compatible (patch-level)
  - `^1.5.0`: version 1.5.0 or compatible (minor-level)

### Resolution Algorithm

1. Parse all manifests
2. Build dependency graph
3. Detect cycles
4. Topologically sort
5. Validate version constraints
6. Report conflicts

## Content Replacement

### Replace vs Extend

Mods can either replace or extend base content:

```yaml
content:
  units:
    - path: "units/t1_interceptor.yaml"
      replace: true  # replaces base unit
```

### Replace Strategy

1. Load base content first
2. Apply mods in dependency order
3. Replace or extend based on `replace` flag
4. Report any merge conflicts

## Scripting Strategy

### Lua Integration

Lua is the primary scripting language for modding. It provides:

- Safety: sandboxed execution
- Performance: JIT compilation
- Accessibility: easy to learn
- Determinism: same behavior across platforms

### Lua API

```cpp
// Expose only safe, deterministic functions
struct LuaAPI {
    static void register_unit(const std::string& id, const UnitDefinition& def);
    static void register_faction(const std::string& id, const FactionDefinition& def);
    static void register_weapon(const std::string& id, const WeaponDefinition& def);
    static void register_tech(const std::string& id, const TechDefinition& def);
    static int spawn_unit(lua_State* L);
    static int get_tick(lua_State* L);
    static int get_position(lua_State* L);
    // ... more safe functions
};
```

### Lua Mod Entry Point

```lua
-- mod_main.lua
function initialize()
    -- Register new content
    register_unit("mod|faction_a|t2_interceptor", {
        name = "T2 Interceptor",
        cost = { material = 2000, energy = 1500, research = 1000 },
        speed = 275,
        range = 4000,
        -- ...
    })
end

function on_tick(tick)
    -- Game logic
end
```

## Asset Pipeline Integration

### Automatic Asset Generation

Mod authors provide specifications, the pipeline generates assets:

```
mod/
  units/
    t1_interceptor/
      specification.yaml  # faction, tier, role, lineage, balance
      assets/             # optional overrides
        model.blend
        material.mat
```

## Mod Loading Order

1. Base content (always first)
2. Engine-provided content packs
3. User mods (in dependency order)

## Mod Validation

### Validation Steps

1. Manifest syntax check
2. Dependency resolution
3. Content ID uniqueness check
4. Version compatibility check
5. Schema validation

### Error Reporting

```cpp
struct ModLoadError {
    enum class Type {
        MANIFEST_PARSE_FAILED,
        DEPENDENCY_RESOLUTION_FAILED,
        CONTENT_ID_COLLISION,
        SCHEMA_VALIDATION_FAILED,
        ASSET_GENERATION_FAILED
    };
    
    Type type;
    std::string mod_id;
    std::string message;
    std::optional<fs::path> file;
};
```

## Security Considerations

### Sandboxed Execution

- Lua scripts run in a sandboxed environment
- No direct file system access
- No network calls
- No unsafe operations

### Content Signature (Future)

- Optional digital signatures
- Verify mod integrity before loading
- Reject unsigned mods if configured

## Best Practices

### For Mod Authors

1. Always use content IDs, never hard-coded strings
2. Document all overrides and extensions
3. Test with clean save files
4. Avoid global state in scripts
5. Use the API, don't modify internal structures

### For Engine Developers

1. Keep content registry stable
2. Provide clear error messages
3. Support incremental updates
4. Document all extension points
5. Test mod compatibility

## File Locations

```bash
game/
  data/
    base/
      units/
      factions/
      weapons/
      # base content
  mods/
    user/
      # user-installed mods
    bundled/
      # engine-provided mods
  cache/
    content_ids/
    generated_assets/
    mod_registry/
```
