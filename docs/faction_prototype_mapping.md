# Temporary Faction Prototype Mapping

The active visual paths for the ground, air, and logistics rows below have been
replaced by the user-provided reference pack and converted into
`godot/project/assets/source_3d/reference_models/`. Stable visual IDs and
gameplay identities are unchanged. The authoritative source-to-output record is
`data/provenance/reference_model_mapping.json`; the two naval slots remain on
the existing lightweight placeholders because the reference directory contains
no destroyer-class source and its carrier import is not yet lightweight enough
for the current validator.

All entries are CC0 donor/prototype visuals. Their `prototype_unit_id` and
`visual_id` are stable Mandate of War identifiers; donor filenames and history
do not determine balance, unit identity, orders, replay identity, or lore.

| Faction | Gameplay role | Prototype ID | Visual ID | Donor asset | Replace | Future art direction |
|---|---|---|---|---|---|---|
| Elite Precision | MBT | `prototype.elite.mbt.v0` | `visual.elite.mbt.prototype` | `cc0.ground.freeciv.armor` | High | Low-observable hull, recessed optics, APS |
| Mass Warfare | MBT | `prototype.mass.mbt.v0` | `visual.mass.swarm_tank.prototype` | `cc0.ground.tanks.t34_seed` | High | Compact unmanned turret, modern sensors, modular armor |
| Mass Warfare | Recon | `prototype.mass.recon.v0` | `visual.mass.recon.prototype` | `cc0.ground.tanks.t34_seed` | Immediate | Wheeled scout and sensor mast; donor is only a stopgap |
| Industrial Experimental | MBT | `prototype.industrial.mbt.v0` | `visual.industrial.mbt.prototype` | `cc0.ground.modern_tanks.tank_collection` | High | Modular cassettes, large power/cooling modules |
| Elite Precision | Fighter | `prototype.elite.fighter.v0` | `visual.elite.fighter.prototype` | `cc0.air.freeciv.fighter` | High | Fictional intakes, sensor apertures, weapon bays |
| Industrial Experimental | Logistics truck | `prototype.industrial.logistics_truck.v0` | `visual.industrial.logistics_truck.prototype` | `cc0.ground.freeciv.freight_02` | Medium | Armored cab, swappable logistics modules |
| Industrial Experimental | Naval combatant | `prototype.industrial.destroyer.v0` | `visual.industrial.destroyer.prototype` | `cc0.naval.destroyer_seed` | Medium | Modular deck weapons and industrial sensors |
| Industrial Experimental | Carrier | `prototype.industrial.carrier.v0` | `visual.industrial.carrier.prototype` | `cc0.naval.carrier_seed` | Medium | Modular deck, launch/recovery, repair bays |
