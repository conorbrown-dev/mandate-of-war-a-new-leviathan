#pragma once

#include <cstdint>
#include <cmath>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <iostream>

#include "ecs/entity.hpp"
#include "ecs/components/weapon.hpp"

namespace rts {

struct Position {
    float x, y, z;
};

struct Velocity {
    float x, y, z;
};

struct Health {
    Health() : current(0.0f), max(1.0f), is_dead(false) {}
    Health(float c, float m) : current(c), max(m), is_dead(c <= 0.0f) {}
    Health(float c, float m, bool d) : current(c), max(m), is_dead(d) {}
    
    float current;
    float max;
    bool is_dead;
};

struct UnitData {
    float speed;
    float view_range;
};

class SpatialGrid {
public:
    SpatialGrid(float cell_size = 100.0f)
        : cell_size_(cell_size) {}
    
    void clear() {
        grid_.clear();
        positions_.clear();
        entity_to_cell_.clear();
    }
    
    void insert(EntityId entity, float x, float y) {
        remove(entity);
        auto key = cell_key(x, y);
        grid_[key].push_back(entity);
        entity_to_cell_[entity] = key;
        positions_[entity] = {x, y, 0.0f};
    }

    void remove(EntityId entity) {
        auto cell_key_it = entity_to_cell_.find(entity);
        if (cell_key_it != entity_to_cell_.end()) {
            auto cell = grid_.find(cell_key_it->second);
            if (cell != grid_.end()) {
                auto& entities = cell->second;
                entities.erase(std::remove(entities.begin(), entities.end(), entity), entities.end());
                if (entities.empty()) {
                    grid_.erase(cell);
                }
            }
            entity_to_cell_.erase(cell_key_it);
        }
        positions_.erase(entity);
    }
    
    void update(EntityId entity, float x, float y) {
        auto key = cell_key(x, y);
        
        auto it = entity_to_cell_.find(entity);
        if (it != entity_to_cell_.end() && it->second == key) {
            positions_[entity] = {x, y, 0.0f};
            return;
        }
        
        if (it != entity_to_cell_.end()) {
            auto old_cell = grid_.find(it->second);
            if (old_cell != grid_.end()) {
                auto& entities = old_cell->second;
                entities.erase(std::remove(entities.begin(), entities.end(), entity), entities.end());
                if (entities.empty()) {
                    grid_.erase(old_cell);
                }
            }
        }
        
        auto& cell = grid_[key];
        cell.push_back(entity);
        entity_to_cell_[entity] = key;
        positions_[entity] = {x, y, 0.0f};
    }
    
    std::vector<EntityId> query_in_region(float x, float y, float radius) const {
        std::vector<EntityId> result;
        float r2 = radius * radius;
        
        auto min_cell = cell_key(x - radius, y - radius);
        auto max_cell = cell_key(x + radius, y + radius);
        
        for (int cx = min_cell.first; cx <= max_cell.first; ++cx) {
            for (int cy = min_cell.second; cy <= max_cell.second; ++cy) {
                auto it = grid_.find({cx, cy});
                if (it != grid_.end()) {
                    for (auto entity : it->second) {
                        auto pos = positions_.find(entity);
                        if (pos != positions_.end()) {
                            float dx = pos->second.x - x;
                            float dy = pos->second.y - y;
                            if (dx * dx + dy * dy <= r2) {
                                result.push_back(entity);
                            }
                        }
                    }
                }
            }
        }
        
        return result;
    }
    
    std::vector<EntityId> query_all() const {
        std::vector<EntityId> result;
        result.reserve(positions_.size());
        for (const auto& cell : grid_) {
            for (auto entity : cell.second) {
                if (positions_.find(entity) != positions_.end()) {
                    result.push_back(entity);
                }
            }
        }
        return result;
    }

    size_t position_count() const { return positions_.size(); }

private:
    using CellKey = std::pair<int, int>;
    
    struct CellKeyHash {
        size_t operator()(const CellKey& k) const {
            return std::hash<int>()(k.first) * 31 + std::hash<int>()(k.second);
        }
    };
    
    CellKey cell_key(float x, float y) const {
        return {
            static_cast<int>(std::floor(x / cell_size_)),
            static_cast<int>(std::floor(y / cell_size_))
        };
    }
    
    float cell_size_;
    std::unordered_map<CellKey, std::vector<EntityId>, CellKeyHash> grid_;
    std::unordered_map<EntityId, Position> positions_;
    std::unordered_map<EntityId, CellKey> entity_to_cell_;
};

} // namespace rts
