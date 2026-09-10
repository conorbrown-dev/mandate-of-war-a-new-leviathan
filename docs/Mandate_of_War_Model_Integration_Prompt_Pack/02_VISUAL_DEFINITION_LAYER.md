# Goal 2 — Data-Driven Visual Definition Layer

Implement a replaceable visual-definition layer between gameplay unit definitions and GLB resources.

## Hard Rule
Never derive gameplay data from the model filename, path, dimensions, mesh name, or historical identity.

Do not do this:

```gdscript
if model_path.contains("t34"):
    armor = 500
```

Preserve this relationship:

```text
UnitDefinition
   visual_id
      ↓
VisualDefinition
      ↓
GLB / transforms / LOD / hardpoints / materials
```

## VisualDefinition Requirements
Support:
- stable visual ID
- model resource path
- optional local scale correction
- optional rotation correction
- ground/vertical offset
- selection visual size if presentation-specific
- LOD configuration
- turret/hardpoint metadata
- animation metadata
- faction material/mask configuration
- optional wreck visual ID
- optional icon/thumbnail path

Prefer the project's existing Resource/JSON/content format rather than inventing another system.

## Registry / Loader
Create or extend a registry that:
- resolves visual IDs
- validates referenced resources
- caches loaded PackedScenes/resources
- rejects duplicate IDs
- reports missing visuals clearly
- can provide a development fallback mesh

Do not repeatedly load the same GLB per unit instance.

## Modding
Design so mods can register additional visual definitions later without giant enums/match statements.

## Tests
Cover:
- valid visual resolution
- missing visual handling
- duplicate ID rejection
- invalid model path
- unit definition references visual ID
- gameplay stats remain independent of visual definition
