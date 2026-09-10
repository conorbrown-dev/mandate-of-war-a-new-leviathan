# Goal 1 — Audit Processed Models and Establish Godot Asset Structure

Inspect the Mandate of War repository and processed model library. Locate the actual `.glb` files produced by the CC0 Blender pipeline.

Search likely locations such as:

```text
downloaded_assets/
Mandate_of_War_CC0_Asset_Import_Pack/
godot_ready/
```

Do not implement gameplay yet.

## Inventory
Create a machine-readable inventory for every usable model containing:
- internal asset ID
- filename and relative path
- category: ground, air, naval, logistics, structure/prop, unknown
- intended prototype role
- `.meta.json` path if present
- bounds from metadata if available
- triangle count if available
- provenance/source ID if available
- obvious duplicate/alternate model indicator
- status: usable, needs cleanup, rejected

## Godot Structure
Align with the existing repo. If no equivalent exists, prefer something like:

```text
assets/
  source_3d/cc0/
  generated/units/
    ground/
    air/
    naval/
    logistics/
  provenance/
```

Do not duplicate large binaries unnecessarily. If processed assets already live in an appropriate repo location, reference them there.

## Provenance
Preserve a non-gameplay provenance record with:
- internal asset ID
- original source title
- creator if known
- source URL if available in existing manifest
- license = CC0
- processed source path
- GLB path
- intended Mandate of War prototype role
- status = prototype/donor

## Import Validation
Verify Godot can import each GLB. Record problems including:
- missing textures
- broken material references
- unexpected orientation
- impossible scale/bounds
- empty models
- excessive geometry
- duplicate LOD meshes simultaneously visible

Do not perform major mesh surgery in Godot. Record art-pipeline problems for Blender.

## Completion
Every usable model has a stable internal asset identifier, known resource path, category, provenance entry, and validation state.
