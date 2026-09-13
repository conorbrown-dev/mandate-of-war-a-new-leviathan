# Goal 11 — Forward Seizure and Base Establishment

**Status:** VERIFIED on 2026-09-13

## Objective

Provide an authoritative territorial-control foundation in which units can
reconnoiter, seize, secure, and establish forward operating infrastructure.
Progression must be deterministic, capability-gated, and visible in the Godot
presentation.

## Acceptance criteria

| ID | Requirement | Evidence |
|---|---|---|
| G11-DOMAIN | Territorial states, progressive zone types, installation types, and establishment requirements are explicit data types. | `territorial_control.hpp` and unit assertions. |
| G11-CAPABILITIES | Unit seizure capabilities are content-authored and enforced by authoritative commands. | Prototype capability assignment and INSTALL rejection assertions. |
| G11-PROGRESSION | Units can be assigned to zones and deterministic presence/security advances zone progression without skipping states. | Zone progression assertions. |
| G11-INSTALL | A capable engineer can issue INSTALL to begin a FOB; invalid issuer, location, and type are rejected. | Command validation and integration assertions. |
| G11-CONSTRUCTION | FOB construction is tracked over fixed ticks, remains inactive before completion, and activates its authored logistics/defense effects after completion. | Construction progress and completion assertions. |
| G11-UI | Zone/install state and FOB completion are visible in the Godot HUD using authoritative state. | Godot presentation harness and inspected notification flow. |
| G11-TESTS | Release build, CTest, direct territorial tests, and Godot presentation tests pass. | Current command output and reports. |
| G11-REVIEW | Codex review evaluates authority, determinism, evidence, and limitations. | `docs/GOAL_11_CODEX_REVIEW.md`. |

## Definition of done

Every row is verified with current evidence and all durable state documents are
synchronized before any later goal is activated.
