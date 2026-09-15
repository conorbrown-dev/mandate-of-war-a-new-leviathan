# Mandate of War icon pipeline

`milsymbol` is a development-time renderer for APP-6 / MIL-STD-2525-style SVG symbols. This tool generates files ahead of time; JavaScript is never loaded by Godot at runtime.

```bash
cd tools/icons
npm install
npm run generate-icons
```

Output is deterministic under `assets/ui/symbols/nato/<affiliation>/<category>/`, with `index.json` for future Godot lookup tooling. Icons are 96px-equivalent SVGs with transparent backgrounds, consistent frames, and affiliation-standard frames. Use `npm run generate-icons -- --clean` to rebuild the generated output, or `npm run validate-manifest` to validate only.

Add a snake_case object to `milsymbol-manifest.json`. It supplies the role, category, label, closest standard SIDC, and a documented mapping. The generator validates IDs, duplicate entries, and `milsymbol` SIDC support, then replaces the SIDC affiliation character to emit friendly, hostile, neutral, and unknown frames. It logs every generated file, warns once for each documented approximation, and writes a summary with total, generated, skipped, and warning counts.

These symbols are for strategic zoom, tactical overlays, selection, and identification—not the primary command-button style. Fictional or more specific Mandate of War concepts use the closest readable standard analog and should not be interpreted as doctrinally exact.

## Command and UI icons (non-NATO)

`command-icons-manifest.json` is the single source of truth for player actions,
construction, logistics, resources, infrastructure, tactical state, and general
UI controls. Its namespaced IDs (for example `order.attack_move` and
`logistics.resupply`) are stable lookup keys; Godot-facing paths are generated
into `godot/project/assets/ui/icons/index.json`.

```bash
cd tools/icons
npm install
npm run generate-command-icons
# Regenerate both independent pipelines:
npm run generate-icons
```

Useful focused commands are `npm run generate-command-icons -- --clean`,
`node generate-command-icons.mjs --category logistics`, and
`node generate-command-icons.mjs --icon order.attack_move --verbose`.
`--clean` is only intended for a complete output regeneration; filtered runs
update their selected SVGs and index entries without deleting other categories.

The generator validates duplicate IDs, labels, categories, output collisions,
Font Awesome names, and custom/composite definitions before it writes files.
All output has a square `0 0 512 512` viewBox, transparent background, white
geometry, and padding suitable for Godot tinting. Apply normal, hover, pressed,
disabled, selected, warning, and critical colors in Godot controls rather than
making color variants of an asset.

### Sources and license

- Direct and component SVG paths use `@fortawesome/free-solid-svg-icons`
  **6.7.2**, Font Awesome Free, licensed under
  [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). The dependency is
  pinned in `package.json` and `package-lock.json` for repeatability. Its
  attribution is retained in `assets/ui/icons/ATTRIBUTION.md`.
- `custom` entries are small original geometric compositions in
  `generate-command-icons.mjs` (road, rail track, bridge, runway, ammunition,
  industrial material, intelligence-age, fortification, drydock). They have no
  third-party source dependency.
- `composite` entries are explicit manifest layers of the above permitted
  sources; examples include attack-move, build-road, build-railway, build-bridge
  and low-resource warnings. No web scraping, proprietary packs, or NATO unit
  identifiers are used in the command pipeline.

To add an icon, add one manifest object with `id`, `category`, `label`,
`description`, and `source`. Use a Font Awesome `icon` name, an explicit
`custom` generator, or a `composite` `layers` array. Run
`npm run validate-command-icons` before generating. Keep output paths stable:
they are `godot/project/assets/ui/icons/<category>/<last-id-segment>.svg` and the index maps
the content ID to the `res://` path.
