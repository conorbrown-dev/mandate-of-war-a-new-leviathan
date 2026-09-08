#pragma once

#include <cstdint>

#include "factions.hpp"

namespace rts {

// Component to assign a faction to an entity
struct Faction {
    FactionId faction_id;
};

} // namespace rts
