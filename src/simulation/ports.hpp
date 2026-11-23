#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include "ecs/entity.hpp"

namespace rts {

// A port is a map site on the coast. The side that holds it can bring forces
// and supplies in by cargo ship, so holding ports is how a side controls the
// sea lanes. Capture works like the power stations and depots: a ground unit
// that stays at the port, with no enemy ground unit there, takes it.
inline constexpr float kPortCaptureRadius = 140.0f;
inline constexpr float kPortHoldSeconds = 8.0f;
// A cargo ship has reached its port when it is this close to the berth.
inline constexpr float kPortBerthRadius = 80.0f;
// A port held by a side is a target: it has hit points, can be shot to rubble
// and then serves nobody until it is repaired. Neutral ports have no owner to
// attack and cannot be damaged.
inline constexpr float kPortMaxHealth = 3000.0f;
// A damaged port works again once repair has brought it past this share.
inline constexpr float kPortServiceFraction = 0.2f;
// Repair: hit points per second for each of the owner's engineers at the quay,
// and the Materials each hit point costs.
inline constexpr float kPortRepairRate = 60.0f;
inline constexpr float kPortRepairCostPerHealth = 0.1f;

struct Port {
    uint32_t id = 0;
    // The quay, on land: where it is captured and where cargo is landed.
    float x = 0.0f;
    float y = 0.0f;
    // Where a ship ties up, in the water beside the quay.
    float berth_x = 0.0f;
    float berth_y = 0.0f;
    // Where ships enter the map, in the water, at the start of the sea lane.
    float entry_x = 0.0f;
    float entry_y = 0.0f;
    // -1 is neutral: nobody can ship in through it.
    int owner = -1;
    // The side currently taking it and how long it has held on.
    int claimant = -1;
    float hold_ms = 0.0f;
    float hp = kPortMaxHealth;
    // Rubble: out of service until repaired past kPortServiceFraction.
    bool destroyed = false;
    // The facility entity that weapons shoot at while the port is held.
    EntityId entity = INVALID_ENTITY;
};

// Marks the hidden entity that stands for a held port so weapons can hit it.
struct PortFacility {
    uint32_t port_id = 0;
};

struct PortPresence {
    int faction = 0;
    float x = 0.0f;
    float y = 0.0f;
};

class PortNetwork {
public:
    static constexpr int kFactions = 3;

    void reset() { ports_.clear(); }
    bool active() const { return !ports_.empty(); }

    bool add(const Port& port);
    const std::map<uint32_t, Port>& ports() const { return ports_; }
    const Port* port(uint32_t id) const;
    bool set_owner(uint32_t id, int owner);
    Port* mutable_port(uint32_t id);
    // Held by this side and not in ruins: the ports it can ship through.
    std::vector<uint32_t> usable_by(int faction) const;
    std::vector<uint32_t> owned_by(int faction) const;

    // Advances captures from the ground units now at the ports. Returns the
    // ports that changed hands this step.
    std::vector<uint32_t> update(float delta_ms, const std::vector<PortPresence>& ground_units);

private:
    std::map<uint32_t, Port> ports_;
};

} // namespace rts
