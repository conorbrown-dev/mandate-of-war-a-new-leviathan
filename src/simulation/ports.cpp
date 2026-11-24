#include "simulation/ports.hpp"

#include <algorithm>
#include <cmath>

namespace rts {

namespace {
bool valid_owner(int owner) { return owner >= -1 && owner < PortNetwork::kFactions; }
}

bool PortNetwork::add(const Port& port) {
    if (port.id == 0 || ports_.contains(port.id) || !valid_owner(port.owner) ||
        !std::isfinite(port.x) || !std::isfinite(port.y) || !std::isfinite(port.berth_x) ||
        !std::isfinite(port.berth_y) || !std::isfinite(port.entry_x) || !std::isfinite(port.entry_y)) return false;
    Port stored = port;
    stored.claimant = -1;
    stored.hold_ms = 0.0f;
    ports_[stored.id] = stored;
    return true;
}

const Port* PortNetwork::port(uint32_t id) const {
    const auto found = ports_.find(id);
    return found == ports_.end() ? nullptr : &found->second;
}

bool PortNetwork::set_owner(uint32_t id, int owner) {
    const auto found = ports_.find(id);
    if (found == ports_.end() || !valid_owner(owner)) return false;
    found->second.owner = owner;
    found->second.claimant = -1;
    found->second.hold_ms = 0.0f;
    return true;
}

Port* PortNetwork::mutable_port(uint32_t id) {
    const auto found = ports_.find(id);
    return found == ports_.end() ? nullptr : &found->second;
}

std::vector<uint32_t> PortNetwork::usable_by(int faction) const {
    std::vector<uint32_t> usable;
    for (const auto& [id, port] : ports_) {
        if (port.owner == faction && !port.destroyed) usable.push_back(id);
    }
    return usable;
}

std::vector<uint32_t> PortNetwork::owned_by(int faction) const {
    std::vector<uint32_t> owned;
    for (const auto& [id, port] : ports_) {
        if (port.owner == faction) owned.push_back(id);
    }
    return owned;
}

std::vector<uint32_t> PortNetwork::update(float delta_ms, const std::vector<PortPresence>& ground_units) {
    std::vector<uint32_t> changed;
    if (!std::isfinite(delta_ms) || delta_ms <= 0.0f) return changed;
    for (auto& [id, port] : ports_) {
        unsigned present = 0;
        for (const auto& unit : ground_units) {
            if (unit.faction < 0 || unit.faction >= kFactions) continue;
            if (std::hypot(unit.x - port.x, unit.y - port.y) <= kPortCaptureRadius) present |= 1u << unit.faction;
        }
        int sole = -1;
        if (present != 0 && (present & (present - 1)) == 0) {
            for (int faction = 0; faction < kFactions; ++faction) {
                if (present & (1u << faction)) sole = faction;
            }
        }
        if (sole >= 0 && sole != port.owner) {
            if (port.claimant != sole) {
                port.claimant = sole;
                port.hold_ms = 0.0f;
            }
            port.hold_ms += delta_ms;
            if (port.hold_ms >= kPortHoldSeconds * 1000.0f) {
                port.owner = sole;
                port.claimant = -1;
                port.hold_ms = 0.0f;
                changed.push_back(id);
            }
        } else {
            // Nobody there, the owner is there, or both sides are: the claim fades.
            port.hold_ms = std::max(0.0f, port.hold_ms - delta_ms);
            if (port.hold_ms <= 0.0f) port.claimant = -1;
        }
    }
    return changed;
}

} // namespace rts
