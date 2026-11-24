#pragma once
#include <cstdint>
#include <array>
#include <functional>
#include "ecs/entity.hpp"

namespace rts {
enum class CombatEventKind : uint8_t { SHOT, HIT, DESTROYED };
using CombatEventSink = std::function<void(CombatEventKind, EntityId, float, float)>;

// Transient presentation evidence, never part of replay/hash/gameplay state.
struct CombatPresentationEvent {
    uint64_t sequence;
    uint32_t tick;
    CombatEventKind kind;
    EntityId entity;
    int unit_type;
    int faction;
    float x, y, velocity_x, velocity_y;
    bool airborne;
    uint8_t audience;
    std::array<uint64_t, 3> viewer_sequences{};
};
}
