# Mandate of War — Simulation Verification Checklist

Use this checklist to verify that discussed, prompted, backlog, and implemented features actually exist and behave correctly in the simulation.

## Status Key

- **[PACK]** Explicitly required by an implementation prompt pack.
- **[DESIGN]** Added or refined in later design discussions.
- **[BACKLOG]** Explicitly added to the feature/fix backlog.
- **[META]** Supporting tooling/system rather than a normal tactical-battle feature.
- **[FUTURE]** Primarily belongs to the eventual strategic campaign layer.
- **[LEGACY]** Earlier design direction that may have been superseded.

> Important: The original prompt pack assumed a more traditional factory/build-queue/unit-spawn loop. The later direction moved toward major combat units being deployed into theater by helicopter, landing craft, ships, etc., while engineers construct battlefield infrastructure, defenses, roads, airfields, and related assets.

---

## 1. Core Simulation and Scale

- [ ] **[PACK]** Fixed simulation timestep independent of rendering.
- [ ] **[PACK]** Simulation/presentation separation.
- [ ] **[PACK]** Data-oriented/lightweight unit simulation rather than a heavyweight Godot object/script per unit.
- [ ] **[PACK]** Data-driven unit definitions.
- [ ] **[PACK]** Large maps / scalable world representation.
- [ ] **[PACK]** Efficient rendering of large armies.
- [ ] **[PACK]** At least 1,000 simultaneously simulated units.
- [ ] **[PACK]** 5,000-unit movement benchmark.
- [ ] **[PACK]** 10,000-unit movement benchmark.
- [ ] **[PACK]** 25,000-unit benchmark where practical.
- [ ] **[DESIGN]** 10,000 active units should be practically playable.
- [ ] **[DESIGN]** 25,000 active units is a major production target.
- [ ] **[DESIGN]** 50,000 active units remains the stretch target.
- [ ] **[PACK]** Headless simulation mode.
- [ ] **[PACK]** Repeatable headless benchmarks.
- [ ] **[PACK]** Simulation continues correctly irrespective of rendering FPS.
- [ ] **[PACK]** Scalable spatial partitioning/spatial queries.
- [ ] **[PACK]** Avoid excessive per-unit allocations.
- [ ] **[PACK]** Worker/job-system-friendly architecture.
- [ ] **[PACK]** Explicit deterministic/near-deterministic simulation state.

---

## 2. Camera and Strategic Zoom

- [-] **[PACK]** Supreme Commander-style strategic zoom.
- [-] Smooth transition between close tactical view and high strategic view.
- [-] Basic camera pan/movement.
- [ ] Zoom does not change simulation behavior.
- [-] Units remain meaningfully readable at strategic zoom levels.
- [-] **[BACKLOG]** Camera yaw properly follows/affects camera movement.
- [-] Appropriate strategic-level unit/icon representation when zoomed out.
- [ ] **[BACKLOG]** Better unit/structure icons.
- [ ] Free camera support for replays.
- [ ] Strategic zoom works during replay playback.

---

## 3. Unit Selection and Orders

- [-] **[PACK]** Single-unit selection.
- [ ] **[PACK]** Box/marquee selection.
- [ ] Multi-unit orders.
- [ ] **[PACK]** Move orders.
- [ ] Rally/move orders from deployment/production facilities where appropriate.
- [ ] Orders operate through the simulation rather than directly manipulating render objects.
- [ ] Risky orders are permitted instead of being silently rejected.
- [ ] Aircraft can intentionally be given a mission that they cannot survive.
- [ ] Orders can ultimately be serialized for multiplayer/replays.

---

## 4. Movement and Pathfinding

- [~] Ground movement.
- [ ] Air movement.
- [ ] Naval movement.
- [ ] Different movement classes can use different navigation rules.
- [ ] **[PACK]** Scalable pathfinding foundation.
- [ ] Shared/coarse paths.
- [ ] Navigation/terrain sectors.
- [ ] Flow-field capability where useful.
- [ ] Path caching.
- [ ] Async path jobs.
- [ ] Strategic routing separated from local avoidance.
- [ ] Formation/shared army routes rather than thousands of independent A* searches.
- [ ] Local collision/avoidance.
- [ ] Congested-unit movement remains stable.
- [ ] **[BACKLOG]** More realistic wheeled/tracked vehicle turning.
- [ ] Vehicles should not simply rotate unnaturally in place unless their drivetrain permits it.
- [ ] Terrain movement characteristics affect appropriate vehicles.
- [ ] **[BACKLOG]** Off-road movement causes appropriate attrition/penalty.
- [ ] **[BACKLOG]** Roads provide meaningful operational value.
- [ ] **[BACKLOG]** Roads can be constructed.

---

## 5. Collision and Physical World Interaction

- [ ] Terrain constrains units appropriately.
- [ ] Ground units do not drive through impassable terrain.
- [ ] Units interact with other units enough to prevent obvious overlap/pass-through.
- [ ] Projectile/terrain collision exists where applicable.
- [ ] Projectile/unit collision exists where applicable.
- [ ] **[BACKLOG]** Units cannot pass through buildings/structures.
- [ ] Building footprints participate in navigation.
- [ ] Water/land boundaries constrain appropriate movement classes.
- [ ] Bridges/roads/runways can eventually participate in movement constraints.

---

## 6. Resources and Economy

### Material

- [-] **[PACK]** Material resource exists.
- [-] Material is used for construction feedstock.
- [ ] Material is used for ammunition.
- [ ] Material is used for spare parts.
- [ ] Material is used for physical logistics/deployment costs.

### Energy

- [-] **[PACK]** Energy resource exists.
- [ ] Energy represents electrical generation.
- [ ] Energy represents fuel where appropriate.
- [ ] Energy represents reactor output.
- [ ] Energy powers operational systems.
- [ ] Energy participates in deployment/operation costs.

### Research

- [-] **[PACK]** Research resource exists.
- [ ] Research supports technology development.
- [ ] Research supports doctrine.
- [ ] Research unlocks advanced systems.
- [ ] Research gates T3/T4 systems.
- [ ] Research behaves differently from simply being another Material pool.

### Economy Behavior

- [ ] Resource generation.
- [ ] Resource consumption.
- [ ] Resource costs are data-driven.
- [ ] Resource availability can prevent or delay actions.
- [ ] Resource systems behave deterministically.

---

## 7. Production vs. Newer Deployment Model

### Original / Legacy Implementation

- [ ] **[PACK][LEGACY]** Basic factory.
- [-] **[PACK][LEGACY]** Build queues.
- [ ] **[PACK][LEGACY]** Build time.
- [ ] **[PACK][LEGACY]** Resource-consuming production.
- [ ] **[PACK][LEGACY]** Unit spawning.

### Newer Intended Direction

- [ ] **[DESIGN]** Major mechanical combat units are generally brought into the battlefield rather than fabricated locally.
- [ ] **[DESIGN]** Helicopters can deliver appropriate equipment/forces.
- [ ] **[DESIGN]** Ships/landing craft can deliver equipment/forces.
- [ ] **[DESIGN]** Deployment has Material cost.
- [ ] **[DESIGN]** Deployment has Energy/fuel/operational cost.
- [ ] **[DESIGN]** Arrival method matters geographically.
- [ ] **[DESIGN]** A force cannot simply appear anywhere on the map.
- [ ] **[DESIGN]** Forward bases/staging areas enable further force projection.
- [ ] **[DESIGN]** Engineers construct battlefield infrastructure rather than manufacturing every tank from raw material.
- [ ] **[DESIGN]** Naval transport/landing capability.
- [ ] **[DESIGN]** Strategic/operational ground transport or mass transit remains an eventual logistics capability.

---

## 8. Three Asymmetric Factions

### Elite / Precision

- [ ] Expensive units.
- [ ] Lower unit count.
- [ ] High survivability.
- [ ] Powerful precision weapons.
- [ ] Excellent sensors.
- [ ] Advanced targeting.
- [ ] Advanced aircraft.
- [ ] Active defensive systems.
- [ ] Elite MBT archetype.
- [ ] Long-range artillery/missile platform.
- [ ] Advanced AA vehicle.

### Mass Warfare

- [ ] Cheap units.
- [ ] Rapid deployment/production.
- [ ] Very high numbers.
- [ ] Designed to tolerate attrition.
- [ ] Cheap logistics/transports.
- [ ] Swarm/map-control doctrine.
- [ ] Cheap swarm tank.
- [ ] Cheap assault vehicle.
- [ ] Cheap AA vehicle.

### Industrial / Experimental

- [ ] Balanced early/mid-game force.
- [ ] Superior infrastructure scaling.
- [ ] Superior Material/Energy/Research generation.
- [ ] Strong research progression.
- [ ] Exceptional late-game technologies.
- [ ] Balanced MBT.
- [ ] Mobile missile/support vehicle.
- [ ] Engineering/support unit.
- [ ] Particularly powerful T4 experimental systems.

---

## 9. Technology Progression

- [ ] T1 systems.
- [ ] T2 systems.
- [ ] T3 systems.
- [ ] T4 experimental systems.
- [ ] Research prerequisites.
- [ ] Technologies/doctrines.
- [ ] Advanced systems are gated rather than immediately available.
- [ ] Industrial faction has infrastructure/research advantages.
- [ ] Faction-specific technology paths can diverge.
- [ ] T4 units feel strategically significant rather than merely being larger T3 units.

---

## 10. Land Combat

- [ ] Main battle tanks.
- [ ] Assault vehicles.
- [ ] Anti-air vehicles.
- [ ] Artillery.
- [ ] Missile/support vehicles.
- [ ] Engineering/support vehicles.
- [ ] Different armor/HP characteristics.
- [ ] Different acceleration/speed characteristics.
- [ ] Sensors/target acquisition.
- [ ] Target selection.
- [ ] Unit destruction.
- [ ] Large formations can fight simultaneously.
- [ ] Land warfare integrates with air and naval forces rather than existing as an isolated layer.

---

## 11. Physical Projectile Combat

- [ ] **[PACK]** Projectile entities/data.
- [ ] Physical shell travel.
- [ ] Projectile velocity.
- [ ] Moving-target prediction.
- [ ] **[PACK]** Lead/intercept calculations.
- [ ] **[PACK]** Ballistic artillery arcs.
- [ ] Bombs.
- [ ] Guided missiles.
- [ ] Torpedoes.
- [ ] Area-of-effect damage.
- [ ] Projectile/terrain interaction.
- [ ] Projectile/unit interaction.
- [ ] Destruction/damage resolution.
- [ ] Weapon reload timing.
- [ ] Weapon range.
- [ ] Missile/projectile interception where appropriate.
- [ ] Purpose-built math rather than heavyweight rigid-body physics for everything.
- [ ] Projectile simulation remains viable during mass combat.
- [ ] Representative 5,000-vs-5,000 active-combat benchmark where practical.

---

## 12. Aircraft Operations

- [-] Airbases.
- [ ] Airstrips.
- [ ] Physical/meaningful runway dependency.
- [ ] Fixed-wing aircraft queue for runway.
- [ ] Takeoff.
- [ ] Mission execution.
- [ ] Return-to-base.
- [ ] Landing/recovery.
- [ ] Refueling.
- [ ] Rearming.
- [ ] Aircraft have finite operational endurance.
- [ ] Endurance can represent fuel/Energy/Material/time/range as appropriate.
- [ ] Aircraft evaluate whether they can reach the target.
- [ ] Aircraft evaluate whether they can complete the mission.
- [ ] Aircraft evaluate whether they can return to a compatible recovery point.
- [ ] Safe-return estimation.
- [ ] Recovery calculations are cached/indexed rather than querying every base every tick.
- [ ] Unsafe-order warning.
- [ ] Unsafe orders remain permitted.
- [ ] Aircraft that exhaust endurance without recovery lose powered flight.
- [ ] Such aircraft crash and are destroyed.
- [ ] Destroying/losing the only runway can strand airborne conventional aircraft.

---

## 13. VTOL

- [ ] VTOL aircraft exist.
- [ ] VTOL does not require a normal runway.
- [ ] VTOL can operate from compatible pads/locations.
- [ ] VTOL has balance disadvantages.
- [ ] Potentially shorter range.
- [ ] Potentially lower payload.
- [ ] Potentially greater Energy consumption.
- [ ] Potentially higher cost.
- [ ] Potentially greater Research requirements.
- [ ] Runway independence creates a meaningful strategic reason to use VTOL.

---

## 14. Aircraft Carriers

- [ ] T1 light carrier.
- [ ] T2 fleet carrier.
- [ ] T3 supercarrier.
- [ ] T4 experimental mobile airbase.
- [ ] T1 carrier prototype exists before later carrier tiers.
- [ ] Carriers function as moving recovery points.
- [ ] Aircraft capacity/storage.
- [ ] Conventional aircraft can land on compatible carriers.
- [ ] Aircraft launch from carriers.
- [ ] Aircraft refuel aboard carriers.
- [ ] Aircraft rearm aboard carriers.
- [ ] Launch queue.
- [ ] Recovery queue.
- [ ] Deck/capacity constraints.
- [ ] Carrier movement is included in recovery calculations.
- [ ] Carriers have exceptional operational endurance.
- [ ] Destroying a carrier materially damages regional air-power projection.
- [ ] Large oceans make carriers strategically necessary rather than cosmetic.

---

## 15. Naval Warfare and Endurance

- [ ] Naval surface combat.
- [ ] Destroyer/equivalent combat ship.
- [ ] Eventual cruisers.
- [ ] Eventual battleships.
- [ ] Eventual submarines.
- [ ] Eventual carriers.
- [ ] Eventual landing craft.
- [ ] Most ships have finite endurance.
- [ ] Naval fuel/supply consumption.
- [ ] Naval bases.
- [ ] Ports.
- [ ] Support/resupply ships as an eventual capability.
- [ ] Depleted ships do not simply explode/sink.
- [ ] Exhausted ships become stranded or heavily mobility-limited.
- [ ] Stranded ships remain vulnerable.
- [ ] Stranded ships can be resupplied.
- [ ] Naval base can restore operational endurance.
- [ ] Carriers have much greater strategic endurance.
- [ ] Naval transports/landing craft support theater deployment.

---

## 16. Strategic Reconnaissance

- [ ] T3 strategic reconnaissance aircraft.
- [ ] Fictional near-future descendant of the SR-71 role.
- [ ] Extremely high speed.
- [ ] Extremely long range.
- [ ] Expensive.
- [ ] Very strong sensors.
- [ ] Weakly armed or unarmed.
- [ ] Difficult to intercept.
- [ ] Capable of theater-wide reconnaissance.
- [ ] Can cross a large theater and return.
- [ ] Reduces repetitive scout spam.

---

## 17. Fog of War and Intelligence Memory

- [ ] Fog of war.
- [ ] Sensor-based observation.
- [ ] Currently visible/observed enemies.
- [ ] Last-known enemy position.
- [ ] Last-known enemy type.
- [ ] Last observation simulation tick/time.
- [ ] Intelligence freshness/confidence.
- [ ] Stale intelligence persists after visibility is lost.
- [ ] Stale intel is visually different from live intel.
- [ ] Intel ages over time.
- [ ] Historical map knowledge is distinguishable from current observation.
- [ ] Recon aircraft contributes intelligence records.
- [ ] Intel does not grant permanent omniscience.
- [ ] **[BACKLOG]** Fog/intel beacons are more visible/brighter.
- [ ] **[BACKLOG]** Development hotkey for adjusting/testing fog/brightness.

---

## 18. Logistics UI and Diagnostics

- [ ] Remaining endurance.
- [ ] Current consumption.
- [ ] Predicted return cost.
- [ ] Closest compatible recovery facility.
- [ ] Safe-return/unsafe-return state.
- [ ] Carrier deck occupancy.
- [ ] Runway queue.
- [ ] Carrier recovery queue.
- [ ] Naval stranded state.
- [ ] Intelligence age.
- [ ] **[BACKLOG]** Useful tooltips.
- [ ] **[BACKLOG]** Engineer build/range hex when engineer is selected.

---

## 19. Operational Logistics and Forward Bases

- [ ] Forward bases matter strategically.
- [ ] Airbase placement affects operational reach.
- [ ] Airstrips extend air operations.
- [ ] Carriers extend air operations.
- [ ] Island control extends air/naval reach.
- [ ] Naval resupply extends fleets.
- [ ] Staging areas exist as an operational concept.
- [ ] Units may need to stage before long-range operations.
- [ ] Transport capacity matters.
- [ ] Helicopters are useful for battlefield delivery.
- [ ] Landing craft/boats are useful for battlefield delivery.
- [ ] Forward logistics is vulnerable to attack.
- [ ] Destroying logistics assets changes what the enemy can sustain in that region.

---

## 20. Engineers and Base Establishment

- [ ] Engineers/support units exist.
- [ ] Engineers establish battlefield infrastructure.
- [ ] Engineers construct roads.
- [ ] Engineers construct/support airfields or related infrastructure where appropriate.
- [ ] Engineers build structures/defenses.
- [ ] Engineer construction range is represented visually.
- [ ] Forward command/base establishment can grow from an initial foothold.
- [ ] Supply/logistics infrastructure is distinct from simply spawning combat vehicles.

---

## 21. Battle Opening / Attacker-Defender System

- [ ] **[DESIGN]** Every relevant tactical battle can identify attacker and defender.
- [ ] Attacker deployment differs from defender deployment.
- [ ] Defender can begin with an established position where strategic circumstances justify it.
- [ ] Attacker may need to establish a lodgment/foothold.
- [ ] Starting situation can reflect what existed on the strategic map.
- [ ] Initial force does not have to be a SupCom-style single ACU.
- [ ] Reinforcements/deployment can arrive through realistic logistics mechanisms.

### Battle Opening Archetypes

- [ ] Deliberate assault.
- [ ] Hasty attack.
- [ ] Meeting engagement.
- [ ] Expeditionary/lodgment assault.
- [ ] Prepared defense.
- [ ] Hasty defense.
- [ ] Airborne/airmobile assault.
- [ ] Pursuit/counterattack.

### Forward Command Group Candidate

- [ ] Command vehicle.
- [ ] Engineers.
- [ ] Reconnaissance/UAV capability.
- [ ] Security forces.
- [ ] Short-range AA.
- [ ] Logistics/recovery element.
- [ ] Deployable radar/power.
- [ ] Functions as a combined-arms foothold rather than a single super-unit.

---

## 22. Terrain and Battlefield Environment

- [ ] Terrain map.
- [ ] Elevation/terrain meaningful to movement/combat where practical.
- [ ] Water/ocean zones.
- [ ] Large bodies of water.
- [ ] Islands.
- [ ] Logistics-relevant islands/staging locations.
- [ ] Roads.
- [ ] Runways.
- [ ] Props.
- [ ] Foliage.
- [ ] **[BACKLOG]** Better trees.
- [ ] **[BACKLOG]** Civilian buildings.
- [ ] Civilian/world structures obey collision.
- [ ] Playable map bounds.
- [ ] Resource locations/strategic locations.

---

## 23. Map Editor

- [ ] **[META][PACK]** Built-in map editor foundation.
- [ ] Terrain creation/editing.
- [ ] Water/sea creation.
- [ ] Spawn placement.
- [ ] Resource placement.
- [ ] Props.
- [ ] Airbase markers.
- [ ] Naval-base markers.
- [ ] Playable bounds.
- [ ] Save map.
- [ ] Load map.
- [ ] Map validation.
- [ ] Future road/runway editing.
- [ ] Future foliage support.
- [ ] Strategic markers.
- [ ] AI navigation hints.
- [ ] Scenarios/scripts.
- [ ] Environmental metadata.
- [ ] Logistics islands/staging points.

---

## 24. Data-Driven Content

- [ ] Costs live outside core simulation code.
- [ ] HP data-driven.
- [ ] Armor data-driven.
- [ ] Speed data-driven.
- [ ] Acceleration data-driven.
- [ ] Weapon damage data-driven.
- [ ] Reload data-driven.
- [ ] Projectile speed data-driven.
- [ ] Range data-driven.
- [ ] Sensor capability data-driven.
- [ ] Research prerequisites data-driven.
- [ ] Schema/content validation.
- [ ] Stable content IDs.

---

## 25. Modding

- [ ] Mods can define units.
- [ ] Mods can define factions.
- [ ] Mods can define weapons.
- [ ] Mods can define projectiles.
- [ ] Mods can define technologies.
- [ ] Mods can define doctrines.
- [ ] Mods can define maps.
- [ ] Mods can define balance.
- [ ] Mods can define AI behavior.
- [ ] Mods can define safe UI extensions.
- [ ] Mods can define assets.
- [ ] Mods can define game modes.
- [ ] Stable mod/content IDs.
- [ ] Mod manifests.
- [ ] Dependencies.
- [ ] Dependency resolution.
- [ ] Content-ID collision detection.
- [ ] Deterministic/cross-platform mod behavior where required.
- [ ] At least one test mod actually loads.

---

## 26. Automated Asset Pipeline

- [ ] **[META]** Blender CLI/headless pipeline.
- [ ] Blender Python.
- [ ] Geometry Nodes/procedural generation where useful.
- [ ] Procedural placeholder meshes.
- [ ] Kitbashing support.
- [ ] UV generation.
- [ ] Materials.
- [ ] Faction masks.
- [ ] LOD generation.
- [ ] Collision mesh generation.
- [ ] Unit icon/thumbnail generation.
- [ ] Engine/Godot import metadata.
- [ ] `.glb`/glTF-oriented workflow.
- [ ] Unit specification drives generation.
- [ ] Generated unit is immediately spawnable.
- [ ] Unit visual-lineage metadata.
- [ ] Historical/modern design inspiration without direct replicas.

---

## 27. Offline AI

### General

- [ ] AI works completely offline.
- [ ] No LLM attached to individual units.
- [ ] Deterministic/heuristic/utility AI initially.

### Strategic AI

- [ ] Economy.
- [ ] Expansion.
- [ ] Technology/doctrine.
- [ ] Theater selection.
- [ ] Carrier deployment.
- [ ] Airbase placement.
- [ ] Logistics planning.
- [ ] Invasion planning.
- [ ] Recon priorities.

### Operational AI

- [ ] Army grouping.
- [ ] Front management.
- [ ] Staging.
- [ ] Transport planning.
- [ ] Fleet routing.
- [ ] Escorts.
- [ ] Defensive sectors.

### Tactical AI

- [ ] Target selection.
- [ ] Positioning.
- [ ] Retreat.
- [ ] Focus fire.
- [ ] Local air defense.
- [ ] Aircraft landing/recovery prioritization.

---

## 28. Multiplayer

- [ ] LAN multiplayer.
- [ ] Direct-connect host/IP multiplayer.
- [ ] Windows/macOS/Linux cross-play.
- [ ] Appropriate deterministic/authoritative/hybrid simulation model.
- [ ] Command synchronization.
- [ ] State hashing/checksums.
- [ ] Desync detection.
- [ ] Reconnect architecture.
- [ ] Spectator support foundation.
- [ ] Does not naïvely transmit every unit transform every render frame.
- [ ] Two local instances can complete a minimal match.

---

## 29. Replays

- [ ] Replay stores map/content version.
- [ ] Initial simulation state.
- [ ] Match settings.
- [ ] Random seed(s).
- [ ] Commands.
- [ ] Necessary simulation events.
- [ ] Replay reproduces the actual simulation.
- [ ] Play/pause.
- [ ] Speed control.
- [ ] Free camera.
- [ ] Strategic zoom.
- [ ] Player-perspective switching.
- [ ] Timeline.
- [ ] Future snapshot/keyframe seeking.
- [ ] Replay format can eventually provide structured AI-training data.

---

## 30. Historical Match Statistics

- [ ] Wins/losses.
- [ ] Faction usage.
- [ ] Maps played.
- [ ] Match duration.
- [ ] Units built/deployed.
- [ ] Units lost.
- [ ] Units destroyed.
- [ ] Material collected/used.
- [ ] Energy generated/used.
- [ ] Research generated.
- [ ] Peak army size.
- [ ] Carrier losses.
- [ ] Air losses.
- [ ] Naval losses.
- [ ] T4 units fielded.
- [ ] Intelligence gathered.

---

## 31. Tactical UI / HUD

- [ ] Main tactical HUD.
- [ ] Unit selection information.
- [ ] Unit health/state.
- [ ] Resource displays.
- [ ] Material display.
- [ ] Energy display.
- [ ] Research display.
- [ ] Unit/action controls.
- [ ] Strategic zoom representation.
- [ ] Logistics/endurance information.
- [ ] Fog/intel state.
- [ ] **[BACKLOG]** Better icons.
- [ ] **[BACKLOG]** Tooltips.
- [ ] **[BACKLOG]** Engineer build-range hex.
- [ ] Performance/debug overlay.
- [ ] FPS display.
- [ ] Simulation tick-time display.
- [ ] Unit-count display.
- [ ] Memory usage where available.

---

## 32. Explicit Feature/Fix Backlog

- [ ] **[BACKLOG]** Better trees.
- [ ] **[BACKLOG]** Realistic vehicle turning.
- [ ] **[BACKLOG]** Road construction.
- [ ] **[BACKLOG]** Off-road attrition.
- [ ] **[BACKLOG]** Developer hotkey for brightness/fog testing.
- [ ] **[BACKLOG]** Civilian buildings.
- [ ] **[BACKLOG]** Better icons.
- [ ] **[BACKLOG]** Units cannot pass through structures.
- [ ] **[BACKLOG]** Fog beacons brighter/more readable.
- [ ] **[BACKLOG]** Camera yaw follow corrected.
- [ ] **[BACKLOG]** Engineer range hex shown on selection.
- [ ] **[BACKLOG]** Tooltips.

---

## 33. Strategic Campaign Layer

> These are important, but should generally not cause the current tactical simulation to fail verification unless that layer has specifically been assigned for implementation.

- [ ] **[FUTURE]** Total War-style strategic map.
- [ ] Territories/regions.
- [ ] Attacker and defender generated from strategic circumstances.
- [ ] Tactical battles resolve territorial conflict.
- [ ] Tactical victory changes strategic-map ownership/control.
- [ ] Theater-level movement.
- [ ] Strategic force staging.
- [ ] Strategic logistics.
- [ ] Strategic research.
- [ ] Technology progression.
- [ ] Resource competition.
- [ ] Sea lanes.
- [ ] Trade/resource routes.
- [ ] Island/port/airfield strategic value.
- [ ] Strategic combat calculations where a full tactical battle is not used.
- [ ] Tactical forces/deployment reflect what is actually available strategically.

---

# Integration Test Theater

Use one test theater to prove a large percentage of Mandate of War's defining systems together.

- [ ] Start as an attacker with a limited forward force.
- [ ] Establish a foothold with engineers.
- [ ] Construct infrastructure.
- [ ] Construct a road.
- [ ] Bring additional armored units in via transport rather than spawning them beside the engineer.
- [ ] Drive vehicles on-road and off-road and verify the difference.
- [ ] Encounter an enemy and verify target acquisition.
- [ ] Verify physical shell travel.
- [ ] Verify moving-target lead/intercept.
- [ ] Verify artillery arcs.
- [ ] Verify AoE damage.
- [ ] Verify destruction/damage resolution.
- [ ] Launch a fighter from an airbase.
- [ ] Give it a target beyond safe round-trip range.
- [ ] Verify unsafe-mission warning.
- [ ] Verify the unsafe order can still be accepted.
- [ ] Recover the aircraft on a carrier.
- [ ] Launch a VTOL and verify that it does not require the runway.
- [ ] Move the carrier and verify aircraft recovery updates correctly.
- [ ] Exhaust a destroyer's endurance.
- [ ] Verify it becomes stranded/degraded rather than sinking.
- [ ] Resupply the destroyer at a naval base or support asset.
- [ ] Fly the T3 strategic recon aircraft over an enemy base.
- [ ] Verify current enemy observation.
- [ ] Allow the enemy to disappear into fog.
- [ ] Verify last-known information remains.
- [ ] Verify intelligence becomes stale over time.
- [ ] Destroy or disable the carrier.
- [ ] Verify regional conventional air operations are materially impaired.
- [ ] Run the same scenario at increasing unit counts.
- [ ] Inspect simulation tick time.
- [ ] Inspect pathfinding cost.
- [ ] Inspect projectile-system cost.
- [ ] Record enough command/state information to replay the battle deterministically.
- [ ] Replay the battle.
- [ ] Have offline AI perform the opposing side's logistics, movement, reconnaissance, and combat.

---

# Verification Tracking Template

Use this section when validating each feature with Codex/MIRIAM.

| Feature | Implemented | Visually Verified | Automated Test | Benchmark | Broken | Not Yet Intended | Notes / Evidence |
|---|---|---|---|---|---|---|---|
| Example: Strategic Zoom | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | |
| Example: Aircraft Endurance | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | |
| Example: Road Construction | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | |
| Example: Intelligence Memory | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | |

