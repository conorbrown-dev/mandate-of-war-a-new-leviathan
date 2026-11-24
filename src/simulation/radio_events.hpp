#pragma once

#include <cstdint>

#include "ecs/components/factions.hpp"
#include "ecs/entity.hpp"

namespace rts {

// What a side's command network is told about. The names in the content
// catalog (godot/project/audio/radio/radio_catalog.json) map to these by
// `native_kind`; never renumber a shipped value, only append.
enum class RadioEventKind : uint8_t {
    FUEL_DEPOT_SECURED = 0,
    FUEL_DEPOT_LOST = 1,
    AIR_HOSTILE_INBOUND = 2,
};

// One report for one listener, raised on a real state change. It is transient
// presentation evidence like CombatPresentationEvent: never hashed, recorded,
// sent over the network or read by the AI. Each side has its own history and
// sequence, so a reader learns nothing about what another side was told.
struct RadioEvent {
    uint64_t sequence = 0;
    uint32_t tick = 0;
    RadioEventKind kind = RadioEventKind::FUEL_DEPOT_SECURED;
    FactionId listener = FactionId::ELITE_PRECISION;
    // Where it happened, as the listener legitimately knows it.
    float x = 0.0f;
    float y = 0.0f;
    // The listener's command post, for a bearing. Absent outside a match.
    bool has_reference = false;
    float reference_x = 0.0f;
    float reference_y = 0.0f;
    // How many things the report covers (aircraft in sight); 1 otherwise.
    int count = 1;
    EntityId subject = INVALID_ENTITY;
};

// A side hears "bandits inbound" when hostile aircraft come into its sight
// with none in sight before, and not again until its sky has been clear of
// them for this long.
inline constexpr uint32_t kAirThreatClearTicks = 600;

} // namespace rts
