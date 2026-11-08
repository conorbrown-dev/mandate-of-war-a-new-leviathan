#pragma once

#include <array>
#include <cstdint>
#include <map>

#include "ecs/components/factions.hpp"
#include "ecs/entity.hpp"

namespace rts {

// Where a faction last saw each enemy. The player's map markers and the AI's
// planning both read this, so neither knows more than the other.
inline constexpr uint32_t kSightingMemoryTicks = 20 * 180;
// A friendly unit this close that can see the spot has checked it. Watching it
// from afar does not count: a unit that walks out of view was last seen on ground
// still in view.
inline constexpr float kSightingCheckedMeters = 200.0f;

struct Sighting {
    float x = 0.0f;
    float y = 0.0f;
    int unit_type = -1;
    uint32_t tick = 0;
    bool mobile = true;
    bool in_view = false;
};

using SightingLog = std::map<EntityId, Sighting>;
using FactionSightings = std::array<SightingLog, 3>;

} // namespace rts
