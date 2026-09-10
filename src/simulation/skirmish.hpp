#pragma once
#include "simulation/simulation.hpp"
#include <array>
#include <string>

namespace rts {
// Bounded match controller. Rules advance only on simulation ticks.
class Skirmish {
public:
    static constexpr uint32_t MAX_MATCH_TICKS = 20000; // 1,000 seconds, then draw
    explicit Skirmish(Simulation& simulation) : simulation_(simulation) {}
    bool load(const std::string& path);
    void update(float milliseconds);
    bool save_replay(const std::string& path);
    bool replay(const std::string& path);
    const std::vector<uint64_t>& checksums() const { return checksums_; }
    int result() const { return result_; } // -1 playing, 0/1 winner, 2 draw
    const std::string& error() const { return error_; }
    EntityId base(int faction) const { return bases_.at(faction); }
    const std::string& name() const { return name_; }
private:
    Simulation& simulation_;
    std::array<EntityId, 2> bases_{};
    float elapsed_ = 0;
    int result_ = -1;
    std::string error_, name_, scenario_path_, content_hash_;
    std::vector<uint64_t> checksums_;
    uint64_t checksum();
};
}
