#!/usr/bin/env python3
"""Validate that temporary faction mappings cite audited donors and visual IDs."""

from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
inventory = json.loads((ROOT / "data/provenance/cc0_model_inventory.json").read_text())
mapping = json.loads((ROOT / "data/provenance/faction_prototype_mapping.json").read_text())
visuals = json.loads((ROOT / "godot/project/visuals/visual_definitions.json").read_text())

asset_ids = {entry["asset_id"] for entry in inventory["models"]}
visual_ids = {entry["visual_id"] for entry in visuals["visual_definitions"]}
prototype_ids: set[str] = set()
for entry in mapping["mappings"]:
    assert entry["donor_asset_id"] in asset_ids, entry
    assert entry["visual_id"] in visual_ids, entry
    assert entry["prototype_unit_id"] not in prototype_ids, entry
    prototype_ids.add(entry["prototype_unit_id"])
    assert entry["replacement_priority"] in {"immediate", "high", "medium", "low"}, entry

print(f"FACTION_PROTOTYPE_MAPPING checks={len(mapping['mappings']) * 4} failures=0")
