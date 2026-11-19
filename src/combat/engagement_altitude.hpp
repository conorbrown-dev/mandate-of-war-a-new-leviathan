#pragma once

#include "combat/air_domain.hpp"
#include "ecs/component_manager.hpp"
#include "ecs/components/aircraft.hpp"
#include "spatial/spatial_grid.hpp"

namespace rts {

// The altitude a weapon must reach to engage this entity. Ground units, ships and
// aircraft on the ground are at zero; an aircraft in flight (see in_flight) is at
// the cruise altitude its unit data gives. A weapon engages an air target only
// when it hits air at all (weapon_reaches) and its ceiling reaches this high.
inline float engagement_altitude(EntityId id, ComponentManager& component_manager) {
    const auto* aircraft = component_manager.get_component<Aircraft>(id);
    if (!aircraft || !in_flight(aircraft->status)) return 0.0f;
    const auto* data = component_manager.get_component<UnitData>(id);
    return data ? data->flight_altitude : 0.0f;
}

} // namespace rts
