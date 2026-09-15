#!/usr/bin/env node
/** Generate tint-friendly, non-NATO command/UI SVGs from the command manifest. */
import * as fa from "@fortawesome/free-solid-svg-icons";
import { mkdir, readFile, rm, writeFile } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const manifestPath = resolve(here, "command-icons-manifest.json");
// Godot's resource root is godot/project, so this is res://assets/ui/icons/.
const outputRoot = resolve(here, "../../godot/project/assets/ui/icons");
const categories = new Set(["orders", "construction", "logistics", "infrastructure", "resources", "combat", "air", "naval", "intel", "status", "economy", "controls", "system"]);
const args = process.argv.slice(2);
const option = (name) => args.includes(name) ? args[args.indexOf(name) + 1] : null;
const selectedCategory = option("--category");
const selectedIcon = option("--icon");
const verbose = args.includes("--verbose");

const library = new Map();
for (const value of Object.values(fa)) if (value?.iconName && value?.icon) library.set(value.iconName, value);

const custom = {
  road: '<path d="M112 448 196 64h120l84 384h-64l-20-92H196l-20 92zm96-156h96l-24-112h-48z"/><path d="M244 96h24v48h-24zm0 96h24v48h-24zm0 96h24v48h-24zm0 96h24v48h-24z" fill="#07131d"/>',
  railway: '<path d="M104 72h52v368h-52zm252 0h52v368h-52z"/><path d="M124 118h264v40H124zm0 118h264v40H124zm0 118h264v40H124z"/>',
  bridge: '<path d="M52 396h408v48H52zM76 348c0-132 72-236 180-236s180 104 180 236h-48c0-104-54-188-132-188s-132 84-132 188z"/><path d="M232 136h48v212h-48z"/>',
  runway: '<path d="M200 48h112v416H200z"/><path d="M244 76h24v54h-24zm0 92h24v54h-24zm0 92h24v54h-24zm0 92h24v54h-24z" fill="#07131d"/><path d="m78 396 178-96 178 96-178 68z"/>',
  rail_station: '<path d="M84 84h344v344H84zM124 124v152h264V124zM148 320h216v44H148z"/><path d="M172 140h32v120h-32zm68 0h32v120h-32zm68 0h32v120h-32z" fill="#07131d"/>',
  ammo: '<path d="M166 52h72v264h-72zM274 52h72v264h-72zM142 316h100v144H142zm128 0h100v144H270z"/><path d="M166 52 202 12l36 40zm108 0 36-40 36 40z"/>',
  material: '<path d="m80 176 176-96 176 96-176 96zM80 264l176 96 176-96v88l-176 96-176-96z"/>',
  stale_intel: '<path d="M256 48a208 208 0 1 0 208 208h-56a152 152 0 1 1-152-152z"/><path d="M224 112h64v136l88 52-32 52-120-72z"/>',
  historical_intel: '<path d="M96 64h320v384H96zM144 120h168v40H144zm0 80h224v40H144zm0 80h176v40H144z"/><path d="M342 318a70 70 0 1 0 70 70h-20a50 50 0 1 1-50-50z"/><path d="M332 336h20v38l28 16-10 18-38-22z"/>',
  front_line: '<path d="M48 104h64v304H48zm88 0h64v304h-64zm88 0h64v304h-64zm88 0h64v304h-64zm88 0h64v304h-64z"/>',
  fortification: '<path d="M64 424V208h64v-72h64v72h64v-72h64v72h64v216zM128 272h48v56h-48zm104 0h48v56h-48zm104 0h48v56h-48z"/>',
  drydock: '<path d="M48 80h416v80h-48v216H96V160H48zM144 160v168h224V160z"/><path d="m178 286 78-84 78 84z"/>',
};

function symbol(iconName) {
  const definition = library.get(iconName);
  if (!definition) return null;
  const [width, height, , , path] = definition.icon;
  const scale = Math.min(400 / width, 400 / height);
  const x = (512 - width * scale) / 2;
  const y = (512 - height * scale) / 2;
  const paths = Array.isArray(path) ? path : [path];
  return `<g transform="translate(${x.toFixed(3)} ${y.toFixed(3)}) scale(${scale.toFixed(6)})">${paths.map((d) => `<path d="${d}"/>`).join("")}</g>`;
}

function transform(body, entry) {
  const transforms = [];
  if (entry.flipX) transforms.push("translate(512 0) scale(-1 1)");
  if (entry.flipY) transforms.push("translate(0 512) scale(1 -1)");
  if (entry.rotate) transforms.push(`rotate(${entry.rotate} 256 256)`);
  return transforms.length ? `<g transform="${transforms.join(" ")}">${body}</g>` : body;
}

function render(entry) {
  if (entry.source === "fontawesome") return transform(symbol(entry.icon), entry);
  if (entry.source === "custom") return custom[entry.generator];
  if (entry.source === "composite") return entry.layers.map((layer) => {
    const body = layer.source === "custom" ? custom[layer.generator] : symbol(layer.icon);
    const scale = layer.scale ?? 1;
    const x = layer.x ?? 0, y = layer.y ?? 0, rotate = layer.rotate ?? 0;
    return `<g transform="translate(${x} ${y}) rotate(${rotate} 256 256) translate(256 256) scale(${scale}) translate(-256 -256)">${body}</g>`;
  }).join("");
  return null;
}

function validate(entries) {
  const errors = [], ids = new Set(), paths = new Set();
  if (!Array.isArray(entries) || !entries.length) return ["manifest must be a non-empty array"];
  for (const entry of entries) {
    const name = entry?.id ?? "<unknown>";
    for (const key of ["id", "category", "label", "description", "source"]) if (typeof entry?.[key] !== "string" || !entry[key]) errors.push(`${name}: missing ${key}`);
    if (!/^[a-z][a-z0-9_.]*$/.test(entry?.id ?? "")) errors.push(`${name}: id must be lower-case namespaced text`);
    if (ids.has(entry?.id)) errors.push(`${name}: duplicate id`); ids.add(entry?.id);
    if (!categories.has(entry?.category)) errors.push(`${name}: invalid category '${entry?.category}'`);
    const output = `${entry?.category}/${entry?.file ?? entry?.id?.split(".").at(-1)}.svg`;
    if (paths.has(output)) errors.push(`${name}: output collision at ${output}`); paths.add(output);
    if (entry?.source === "fontawesome" && !symbol(entry.icon)) errors.push(`${name}: unknown Font Awesome icon '${entry.icon}'`);
    if (entry?.source === "custom" && !custom[entry.generator]) errors.push(`${name}: unknown custom generator '${entry.generator}'`);
    if (entry?.source === "composite" && (!Array.isArray(entry.layers) || !entry.layers.length)) errors.push(`${name}: composite needs layers`);
    for (const layer of entry?.layers ?? []) if ((layer.source === "fontawesome" && !symbol(layer.icon)) || (layer.source === "custom" && !custom[layer.generator])) errors.push(`${name}: malformed composite layer`);
    if (!["fontawesome", "custom", "composite"].includes(entry?.source)) errors.push(`${name}: unsupported source '${entry?.source}'`);
  }
  return errors;
}

const manifest = JSON.parse(await readFile(manifestPath, "utf8"));
const errors = validate(manifest);
if (errors.length) { console.error(`Command icon manifest validation failed:\n${errors.map((error) => `- ${error}`).join("\n")}`); process.exit(1); }
if (args.includes("--validate-only")) { console.log(`Command icon manifest valid: ${manifest.length} entries.`); process.exit(0); }
if (selectedCategory && !categories.has(selectedCategory)) { console.error(`Unknown category: ${selectedCategory}`); process.exit(1); }
let selected = manifest.filter((entry) => (!selectedCategory || entry.category === selectedCategory) && (!selectedIcon || entry.id === selectedIcon || entry.file === selectedIcon));
if (!selected.length) { console.error("No manifest entries match the supplied filters."); process.exit(1); }
if (args.includes("--clean") && !selectedCategory && !selectedIcon) await rm(outputRoot, { recursive: true, force: true });
await mkdir(outputRoot, { recursive: true });
const indexPath = resolve(outputRoot, "index.json");
const existing = args.includes("--clean") ? {} : JSON.parse(await readFile(indexPath, "utf8").catch(() => "{}"));
const index = { ...existing }, counts = { fontawesome: 0, composite: 0, custom: 0 };
for (const entry of selected) {
  const name = entry.file ?? entry.id.split(".").at(-1);
  const relativePath = `${entry.category}/${name}.svg`;
  const body = render(entry);
  const destination = resolve(outputRoot, relativePath);
  await mkdir(dirname(destination), { recursive: true });
  await writeFile(destination, `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512" role="img" aria-label="${entry.label}"><g fill="#ffffff">${body}</g></svg>\n`);
  index[entry.id] = { id: entry.id, label: entry.label, category: entry.category, description: entry.description, source: entry.source, sourceIcon: entry.icon ?? entry.generator ?? entry.layers.map((layer) => layer.icon ?? layer.generator).join(" + "), path: `res://assets/ui/icons/${relativePath}`, aliases: entry.aliases ?? [] };
  counts[entry.source]++;
  if (verbose) console.log(`generated ${relativePath}`);
}
await writeFile(indexPath, `${JSON.stringify(index, null, 2)}\n`);
console.log(`Command Icon Generation Complete\n\nManifest icons:       ${manifest.length}\nGenerated this run:   ${selected.length}\nFont Awesome icons:   ${counts.fontawesome}\nComposite icons:      ${counts.composite}\nCustom icons:         ${counts.custom}\n\nOutput:\ngodot/project/assets/ui/icons/ (res://assets/ui/icons/)`);
