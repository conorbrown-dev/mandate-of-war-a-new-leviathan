#pragma once

#include "ecs/component_manager.hpp"
#include "ecs/components/aircraft.hpp"

namespace rts {

// Real weapons engage either things in the air or things on the ground (or
// both, like an attack helicopter's missiles). An aircraft is an air target
// from take-off to touch-down; parked on its runway it is a ground target.
inline bool in_flight(Aircraft::Status status) {
    return status == Aircraft::Status::TAKING_OFF || status == Aircraft::Status::AIRBORNE ||
           status == Aircraft::Status::RETURNING || status == Aircraft::Status::QUEUED_FOR_LANDING ||
           status == Aircraft::Status::LANDING;
}

inline bool is_air_target(const ComponentManager& components, EntityId entity) {
    const auto* aircraft = components.get_component<Aircraft>(entity);
    return aircraft && in_flight(aircraft->status);
}

inline bool weapon_reaches(bool hits_ground, bool hits_air, bool target_in_air) {
    return target_in_air ? hits_air : hits_ground;
}

} // namespace rts
