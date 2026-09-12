# Validation Architecture Reference

This file is conceptual guidance, not a mandate to introduce these exact classes.

## Target flow

```text
                    +--------------------+
                    | Gameplay Goal      |
                    +---------+----------+
                              |
                              v
                    +--------------------+
                    | Validation Scenario|
                    +---------+----------+
                              |
           +------------------+------------------+
           |                  |                  |
           v                  v                  v
  +----------------+ +----------------+ +----------------+
  | Real Input /   | | Production     | | Seed / Time /  |
  | Command Path   | | Game Systems   | | Scenario Setup |
  +-------+--------+ +-------+--------+ +-------+--------+
          \                  |                  /
           \                 |                 /
            +----------------+----------------+
                             |
                             v
                    +--------------------+
                    | Observable State   |
                    +---------+----------+
                              |
          +-------------------+--------------------+
          |                   |                    |
          v                   v                    v
 +----------------+  +----------------+   +----------------+
 | Assertions     |  | Screenshots    |   | Metrics        |
 +----------------+  +----------------+   +----------------+
          |                   |                    |
          +-------------------+--------------------+
                              |
                              v
                    +--------------------+
                    | report.json        |
                    | PASS / FAIL        |
                    +---------+----------+
                              |
                              v
                    +--------------------+
                    | Optional Video /   |
                    | Visual Diff        |
                    +--------------------+
```

## Validation tiers

### Tier 1 — State

Fast and cheap.

Examples:

- unit received order;
- projectile spawned;
- target health changed;
- fuel decreased;
- intelligence state became stale.

Run constantly during implementation.

### Tier 2 — Scene/input

Exercises real Godot scene/input or command boundaries.

Examples:

- drag-select;
- right-click move;
- strategic zoom;
- UI button invokes production order.

Run on related gameplay changes.

### Tier 3 — Visual

Screenshot checkpoints and optional visual regression.

Examples:

- strategic icons;
- selection outlines;
- HUD state;
- camera transition appearance.

Run when presentation matters.

### Tier 4 — Recorded behavior

Fixed-step video.

Examples:

- carrier recovery;
- bombing run;
- formation maneuver;
- artillery arc;
- missile interception.

Generate when motion/sequence is valuable evidence.

### Tier 5 — Scale/performance

Examples:

- 1k/10k unit simulation;
- formation path request count;
- projectile stress test;
- sensor/intel update cost.

Run deliberately, not after every code edit.

## Design principle

A high-cost tier should not be required when a lower-cost tier can prove the criterion.

This keeps AI-agent feedback fast and reduces token/tool churn.
