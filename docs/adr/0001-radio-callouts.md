# ADR 0001: Radio callouts and command voice alerts

Date: 2026-11-10. Issue: #3. Status: accepted for slices A to C.

## Decision

1. **The simulation reports; Godot speaks.** `Simulation` raises a `RadioEvent`
   (`src/simulation/radio_events.hpp`) on a real state change, addressed to one
   listening side. It is transient presentation evidence like the combat
   events: not hashed, not recorded, not networked, not read by the AI. Each
   side has its own bounded history (64) and its own sequence, so a cursor says
   nothing about what another side was told.
2. **One bridge call.** `get_radio_events(after_sequence, faction_id)` returns
   that side's reports. Reading consumes nothing.
3. **Content is a JSON catalog.** `godot/project/audio/radio/radio_catalog.json`
   holds stable event ids, the native kind each maps to, category, severity,
   cooldown, dedupe rule, per-faction line variants, a text fallback and a
   text-only location detail. Alerts whose game system does not exist yet are
   `"status": "parked"` and say what they need.
4. **Speech is pre-rendered and optional.** `tools/generate_radio_voice.py`
   renders catalog lines with ElevenLabs on a developer's machine and writes
   `voice_manifest.json`. The game plays clips listed there and shows text for
   everything else. The game never calls ElevenLabs.
5. **Spoken location is the compass point only.** A line may contain
   `{cardinal}`, which costs eight clips. Bearing, range and grid cell appear in
   the text detail, because voicing them needs hundreds of clips per line.
   Stitched number snippets are left for a later slice.

## Why these events first

- `objective.fuel_depot_secured` / `objective.fuel_depot_lost`: raised where a
  depot capture completes. The captor is standing on it and the previous holder
  has stopped receiving its fuel, so neither report needs sight.
- `air.hostile_inbound`: raised on the vision refresh when hostile aircraft are
  in the listener's own sky sight and none were before. "Bandit" means seen and
  hostile. Radar contacts stay anonymous, so `air.contact_unidentified` (bogeys)
  is parked until radar can tell an aircraft from a ground contact.

## Rules the announcer keeps

- One voice. A line never overlaps another.
- Order by severity (critical, urgent, operational, routine). Critical cuts in
  on a lower line; nothing cuts in on critical.
- One report per event (or per event and map sector) inside its cooldown;
  repeats are counted. The queue holds six. A report older than its severity's
  limit is dropped rather than read late.
- Text is always produced, with speech off, missing or muted.
- Variant choice is local round-robin, so it needs no shared randomness.

## Location reference frames

- North is simulation -y (Godot -Z); bearings run clockwise from 000.
- Bearings and ranges are from the listener's command post, carried in the
  report. With no command post no direction is spoken.
- The grid is 2 km cells, columns lettered from the west edge, rows numbered
  from the north edge (`"grid": {"cell_m": 2000}` in the catalog).
- Clock direction (`RadioLocation.clock_direction`) is implemented and tested
  for unit-level chatter, which no event uses yet.

## Not done

Voiced clips (needs an ElevenLabs key and a rights check), mod loading of
alternative catalogs from outside `res://` (the announcer accepts any path, but
nothing discovers mods), replay playback of radio (no replay player exists),
and the parked alerts.
