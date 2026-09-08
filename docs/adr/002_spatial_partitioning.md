# ADR-002: Spatial Partitioning Strategy

**Date:** 2026-08-24  
**Status:** Accepted

## Decision

Use hierarchical spatial partitioning: sparse grid at macro scale, quadtree at micro scale.

## Rationale

For 10k-50k units across large maps (10km²), naive O(n²) distance checks are infeasible:

```
Naive: 50k units → 2.5B pairwise checks/tick → impossible
Spatial: ~100 units/region → 10k region queries → feasible
```

## Architecture

### Macro Scale: Sparse Grid (1km cells)

```
Map: 10000m x 10000m
Grid: 1000m cells → 10x10 = 100 cells

Each cell holds:
- List of entity IDs in that region
- Bounding box
- Aggregated properties (total units, faction counts)
```

**Operations:**
- Insert: O(1) (hash to cell)
- Query: O(k) where k = units in region + neighbors
- Update: O(1) (rehash to cell if moved)

### Micro Scale: Quadtree (within cell, >50 units)

When a cell exceeds threshold (50 units):
- Build quadtree for fine-grained partitioning
- Children: 4 quadrants, each with their own entities

```
Cell (1000m)
├── NW Quadrant (500m) → 20 units
├── NE Quadrant (500m) → 15 units
├── SW Quadrant (500m) → 18 units
└── SE Quadrant (500m) → 22 units
```

### Level of Detail

| Scale | Technique | Use Case |
|-------|-----------|----------|
| Global | Sparse grid (1km) | Strategy view, long-range queries |
| Regional | Quadtree (100m) | Tactical view, combat queries |
| Local | Linear array | Per-unit updates, tight loops |

## Query Types

### 1. Region Query (Radius)

```cpp
// Find all units within 200m of point
auto results = spatial.query_circle(x, y, radius=200);
```

Implementation:
1. Identify which grid cells intersect circle
2. Filter to actual entities within radius
3. Return subset

### 2. Line of Sight

```cpp
// Find units along line segment
auto results = spatial.query_line(start, end);
```

Implementation:
1. Raycast through grid cells (Bresenham-style)
2. For each cell, check quadtree/ entities

### 3. Nearest Neighbor

```cpp
// Find closest unit to point
auto nearest = spatial.nearest_unit(x, y);
```

Implementation:
1. Check immediate grid cell
2. Expand to neighbors if empty
3. Use quadtree to find exact nearest

## Performance Targets

| Query | 10k units | 50k units |
|-------|-----------|-----------|
| Circle (100m) | <1ms | <5ms |
| Circle (1km) | <5ms | <20ms |
| Line of Sight | <2ms | <10ms |
| Nearest Neighbor | <1ms | <2ms |

## Implementation

### Grid Structure

```cpp
struct SpatialGrid {
    float cell_size;
    std::unordered_map<uint64_t, Cell> cells;
    
    uint64_t cell_key(float x, float y) {
        return hash((int)(x / cell_size), (int)(y / cell_size));
    }
    
    void insert(EntityId entity, float x, float y) {
        auto key = cell_key(x, y);
        cells[key].entities.push_back(entity);
    }
};
```

### Quadtree Structure

```cpp
struct Quadtree {
    AABB bounds;
    std::vector<EntityId> entities;
    std::array<std::unique_ptr<Quadtree>, 4> children;
    
    void insert(EntityId entity, float x, float y) {
        if (children.empty()) {
            if (entities.size() < LEAF_THRESHOLD) {
                entities.push_back(entity);
            } else {
                subdivide();
                for (auto e : entities) reinsert(e);
                reinsert(entity);
            }
        } else {
            int idx = quadrant_index(x, y);
            children[idx]->insert(entity, x, y);
        }
    }
};
```

## Updates and Movement

### Movement Tracking

```cpp
void update_movement() {
    for (auto& entity : moving_entities) {
        auto [old_cell, new_cell] = compute_movement(entity);
        if (old_cell != new_cell) {
            grid.move(entity, old_cell, new_cell);
            quadtree.remove(entity);
            quadtree.insert(entity);
        }
    }
}
```

### Lazy Updates

- Only update spatial index when entity moves significantly (threshold: 10% of cell size)
- Batch updates per tick to amortize cost

## Validation

1. **Unit test**: Insert 10k entities, verify query correctness
2. **Benchmark**: Compare query times vs naive approach
3. **Stress test**: 50k moving entities, measure update overhead

## Future Enhancements

1. **Multi-level grid**: Coarse (10km) → Medium (1km) → Fine (100m)
2. **Dynamic regrid**: Adjust cell size based on unit density
3. **GPU spatial query**: Use compute shaders for parallel queries
