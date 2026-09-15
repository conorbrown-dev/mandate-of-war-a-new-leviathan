#include "production/requisition_manager.hpp"
#include "simulation/simulation.hpp"
#include "production/production_manager.hpp"

namespace rts {

void RequisitionManager::add_requisition_order(FactionId faction_id, UnitType unit_type, float destination_x, float destination_y, EntityId destination_entity_id) {
    RequisitionOrder order;
    order.order_id = next_order_id_;
    order.unit_type = unit_type;
    order.faction_id = faction_id;
    order.destination_x = destination_x;
    order.destination_y = destination_y;
    order.destination_entity_id = destination_entity_id;
    order.tick_ordered = 0;
    order.tick_estimated_completion = 0;
    order.in_progress = false;
    order.completed = false;
    order.cancelled = false;
    
    orders_.push_back(order);
    next_order_id_++;
}

void RequisitionManager::complete_requisition(EntityId order_id, EntityId produced_unit_id) {
    for (auto& order : orders_) {
        if (order.order_id == order_id) {
            order.completed = true;
            order.in_progress = false;
            order.produced_unit_id = produced_unit_id;
            return;
        }
    }
}

void RequisitionManager::cancel_requisition(EntityId order_id) {
    for (auto& order : orders_) {
        if (order.order_id == order_id) {
            order.cancelled = true;
            if (!order.completed) {
                order.in_progress = false;
            }
            return;
        }
    }
}

bool RequisitionManager::validate_destination(Simulation& simulation, FactionId faction_id, UnitType unit_type, float destination_x, float destination_y) const {
    const auto& prototypes = get_unit_prototypes();
    auto proto_it = prototypes.find(unit_type);
    if (proto_it == prototypes.end()) {
        return false;
    }
    
    const auto& proto = proto_it->second;
    
    if (proto.is_aircraft && proto.requires_runway) {
        return simulation.territorial_control_manager().has_active_installation(faction_id, InstallationType::AIRFIELD) ||
               simulation.territorial_control_manager().has_active_installation(faction_id, InstallationType::NAVAL_BASE);
    }
    
    return true;
}

void RequisitionManager::update(float delta_ms, ProductionManager& production_manager, Simulation& simulation) {
    for (auto& order : orders_) {
        if (!order.in_progress && !order.completed && !order.cancelled) {
            if (!validate_destination(simulation, order.faction_id, order.unit_type, order.destination_x, order.destination_y)) {
                order.cancelled = true;
                continue;
            }
            
            order.tick_ordered = order.tick_ordered ? order.tick_ordered : simulation.get_state().tick_number;
            order.in_progress = true;
            order.tick_estimated_completion = order.tick_ordered + static_cast<uint32_t>(production_manager.get_build_time_seconds(order.unit_type) * 20.0f);
        }
    }
}

void RequisitionManager::reset() {
    orders_.clear();
    next_order_id_ = 0;
}

} // namespace rts
