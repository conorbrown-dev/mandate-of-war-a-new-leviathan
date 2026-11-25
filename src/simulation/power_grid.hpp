#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <vector>

namespace rts {

// Power is carried, not stored. A power station feeds a side only while an
// unbroken chain of lines that side may use joins the station to one of the
// side's loads (its command post). Lines can be captured, cut and repaired.
enum class PowerNodeKind : uint8_t { STATION = 0, JUNCTION = 1, LOAD = 2 };

// Wooden poles serve small loads. Steel pylons run to and from stations, large
// cities and large structures, and cross wide water.
enum class PowerLineKind : uint8_t { WOOD = 0, STEEL = 1 };

// A unit takes, cuts or repairs a line or station when it is this close.
inline constexpr float kPowerWorkRadius = 40.0f;

struct PowerLineRules {
    float material_base = 0.0f;
    float material_per_meter = 0.0f;
    float repair_seconds_base = 0.0f;
    float repair_seconds_per_meter = 0.0f;
};

constexpr PowerLineRules power_line_rules(PowerLineKind kind) {
    return kind == PowerLineKind::STEEL
        ? PowerLineRules{90.0f, 0.03f, 15.0f, 1.0f / 300.0f}
        : PowerLineRules{30.0f, 0.01f, 8.0f, 1.0f / 500.0f};
}

struct PowerNode {
    uint32_t id = 0;
    PowerNodeKind kind = PowerNodeKind::JUNCTION;
    float x = 0.0f;
    float y = 0.0f;
    // -1 is civilian: a civilian station feeds any side joined to it.
    int owner = -1;
    float output_mw = 0.0f; // stations
    float demand_mw = 0.0f; // loads
};

struct PowerLine {
    uint32_t id = 0;
    uint32_t from = 0;
    uint32_t to = 0;
    PowerLineKind kind = PowerLineKind::WOOD;
    // -1 is civilian: any side may draw through it. A side's own line carries
    // power for that side only, so capturing a line denies it to the other.
    int owner = -1;
    bool intact = true;
    // The route from one node to the other, both ends included.
    std::vector<std::array<float, 2>> path;
    float length = 0.0f;
    // Where it was cut, for the view's sparks. Valid while not intact.
    float cut_x = 0.0f;
    float cut_y = 0.0f;
    // Above zero while a paid repair is under way.
    float repair_remaining_ms = 0.0f;
    int repair_faction = -1;
};

struct PowerSupply {
    float generation_mw = 0.0f;
    float demand_mw = 0.0f;
};

class PowerGrid {
public:
    static constexpr int kFactions = 3;

    void reset();
    // A match has a grid once its scenario adds a node. Without one the
    // simulation leaves each side's Power state alone.
    bool active() const { return !nodes_.empty(); }

    bool add_node(const PowerNode& node);
    // `via` lists the route's bends between the two nodes, in order.
    bool add_line(uint32_t id, uint32_t from, uint32_t to, PowerLineKind kind, int owner,
                  const std::vector<std::array<float, 2>>& via = {});

    const std::map<uint32_t, PowerNode>& nodes() const { return nodes_; }
    const std::map<uint32_t, PowerLine>& lines() const { return lines_; }
    const PowerNode* node(uint32_t id) const;
    const PowerLine* line(uint32_t id) const;

    bool capture_station(uint32_t id, int faction);
    bool capture_line(uint32_t id, int faction);
    // Cuts the line at the point on it nearest (x, y). A repair under way is lost.
    bool sever_line(uint32_t id, float x, float y);
    bool begin_repair(uint32_t id, int faction);
    // Advances repairs. Returns the lines that came back into service.
    std::vector<uint32_t> update(float delta_ms);

    float repair_cost(uint32_t id) const;
    float repair_seconds(uint32_t id) const;

    // Finished structures draw power wherever they stand.
    void add_structure_demand(int faction, float demand_mw);

    PowerSupply supply(int faction) const;
    // Whether this load is joined to a station its side may draw from.
    bool load_powered(uint32_t load_id) const;

    // Distance from a point to the line's route, with the nearest point on it.
    float distance_to_line(uint32_t id, float x, float y, float* nearest_x = nullptr, float* nearest_y = nullptr) const;
    // Points along the route, both ends included, no further apart than `spacing`.
    std::vector<std::array<float, 2>> sample_line(uint32_t id, float spacing) const;

private:
    std::vector<uint32_t> stations_reached_from(uint32_t load_id, int faction) const;

    std::map<uint32_t, PowerNode> nodes_;
    std::map<uint32_t, PowerLine> lines_;
    std::array<float, kFactions> structure_demand_{};
};

} // namespace rts
