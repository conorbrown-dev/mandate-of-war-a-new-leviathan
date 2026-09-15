#pragma once

#include <vector>

#include "ecs/entity.hpp"
#include "ecs/components/production.hpp"
#include "ecs/components/factions.hpp"
#include "ecs/components/territorial_control.hpp"

namespace rts {

class ProductionManager;
class Simulation;

struct RequisitionOrder {
    EntityId order_id;
    UnitType unit_type;
    FactionId faction_id;
    float destination_x;
    float destination_y;
    EntityId destination_entity_id;
    EntityId produced_unit_id;
    uint32_t tick_ordered = 0;
    uint32_t tick_estimated_completion = 0;
    bool in_progress = false;
    bool completed = false;
    bool cancelled = false;
};

class RequisitionManager {
public:
    RequisitionManager() = default;
    
    void add_requisition_order(FactionId faction_id, UnitType unit_type, float destination_x, float destination_y, EntityId destination_entity_id);
    void complete_requisition(EntityId order_id, EntityId produced_unit_id);
    void cancel_requisition(EntityId order_id);
    
    void update(float delta_ms, ProductionManager& production_manager, Simulation& simulation);
    
    std::vector<RequisitionOrder>& orders() { return orders_; }
    const std::vector<RequisitionOrder>& orders() const { return orders_; }
    
    void clear_completed() { orders_.clear(); }
    void reset();

private:
    bool validate_destination(Simulation& simulation, FactionId faction_id, UnitType unit_type, float destination_x, float destination_y) const;
    
    std::vector<RequisitionOrder> orders_;
    
    EntityId next_order_id_ = 0;
};

} // namespace rts
