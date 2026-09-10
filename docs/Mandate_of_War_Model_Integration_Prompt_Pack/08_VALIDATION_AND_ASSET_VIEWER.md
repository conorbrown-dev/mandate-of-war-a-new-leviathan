# Goal 8 — Developer Asset / Unit Validation Viewer

Build a reusable Godot developer tool/scene for inspecting every integrated prototype visual.

## Viewer Features
Support:
- enumerate visual definitions
- filter by ground/air/naval/logistics/structure
- spawn/select visual
- orbit camera
- show bounds
- show triangle count if metadata provides it
- show visual ID and provenance ID
- show collision/footprint representation
- show hardpoints/axes
- toggle LODs if implemented
- test team/faction material variation
- surface missing texture/material warnings

## Scale Comparison
Allow multiple units to be placed together for scale QA, such as infantry, recon, MBT, logistics truck, fighter, destroyer, carrier.

## Automated Validator
Detect:
- missing GLB
- missing visual definition
- duplicate visual ID
- zero/invalid bounds
- extreme scale
- required hardpoint missing for weapon-bearing unit
- multiple LODs visible simultaneously
- invalid material references
- prototype asset accidentally referenced by final/shipping content if content-state metadata exists

Generate a concise machine-readable and human-readable validation report.
