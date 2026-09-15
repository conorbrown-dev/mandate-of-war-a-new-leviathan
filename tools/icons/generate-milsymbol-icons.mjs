#!/usr/bin/env node
import ms from "milsymbol";
import { mkdir, readFile, rm, writeFile } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const outputRoot = resolve(here, "../../godot/project/assets/ui/symbols/nato");
const manifestPath = resolve(here, "milsymbol-manifest.json");
const affiliations = { friendly: "F", hostile: "H", neutral: "N", unknown: "U" };

function validate(entries) {
  const errors = [], ids = new Set();
  if (!Array.isArray(entries) || !entries.length) return ["manifest must be a non-empty array"];
  for (const entry of entries) {
    for (const key of ["id", "category", "label", "symbolType", "role", "sidc"])
      if (typeof entry?.[key] !== "string" || !entry[key]) errors.push(`${entry?.id ?? "<unknown>"}: missing ${key}`);
    if (!/^[a-z0-9_]+$/.test(entry?.id ?? "")) errors.push(`${entry?.id}: id must be snake_case`);
    if (ids.has(entry?.id)) errors.push(`${entry.id}: duplicate id`);
    ids.add(entry?.id);
    if ((entry?.sidc?.length ?? 0) < 10) errors.push(`${entry?.id}: SIDC is too short`);
    else if (!new ms.Symbol(entry.sidc, renderOptions).isValid()) errors.push(`${entry.id}: SIDC is not supported by milsymbol`);
  }
  return errors;
}

const withAffiliation = (sidc, affiliation) => `${sidc[0]}${affiliations[affiliation]}${sidc.slice(2)}`;
const renderOptions = { size: 96, frame: true, fill: false, outlineWidth: 2, outlineColor: "#07131d" };
const svg = (sidc) => new ms.Symbol(sidc, renderOptions).asSVG();

const args = new Set(process.argv.slice(2));
const manifest = JSON.parse(await readFile(manifestPath, "utf8"));
const errors = validate(manifest);
if (errors.length) { console.error(`Manifest validation failed:\n${errors.map((x) => `- ${x}`).join("\n")}`); process.exit(1); }
if (args.has("--validate-only")) { console.log(`Manifest valid: ${manifest.length} entries.`); process.exit(0); }
if (args.has("--clean")) await rm(outputRoot, { recursive: true, force: true });
try { await mkdir(outputRoot, { recursive: true }); } catch (error) { console.error(`Unable to create ${outputRoot}: ${error.message}`); process.exit(1); }
const index = [], warnings = [];
for (const entry of manifest) {
  if (/analog|approximation|closest/i.test(entry.mapping ?? ""))
    warnings.push(`${entry.id}: ${entry.mapping}`);
  for (const affiliation of Object.keys(affiliations)) {
    const sidc = withAffiliation(entry.sidc, affiliation);
    const path = `${affiliation}/${entry.category}/${entry.id}.svg`;
    try {
      const symbol = new ms.Symbol(sidc, renderOptions);
      if (!symbol.isValid()) warnings.push(`${entry.id}: ${sidc} is an approximation`);
      const destination = resolve(outputRoot, path);
      await mkdir(dirname(destination), { recursive: true });
      await writeFile(destination, svg(sidc), "utf8");
      index.push({ id: entry.id, category: entry.category, affiliation, relativePath: path, path: `res://assets/ui/symbols/nato/${path}`, label: entry.label, role: entry.role, symbolType: entry.symbolType, sidc, mapping: entry.mapping });
      console.log(`generated ${path}`);
    } catch (error) { warnings.push(`${entry.id}/${affiliation}: ${error.message}`); }
  }
}
await writeFile(resolve(outputRoot, "index.json"), `${JSON.stringify({ iconSize: 96, icons: index }, null, 2)}\n`);
console.log(`Summary: total entries=${manifest.length}; generated=${index.length}; skipped=${manifest.length * 4 - index.length}; warnings=${warnings.length}`);
warnings.forEach((warning) => console.warn(`warning: ${warning}`));
