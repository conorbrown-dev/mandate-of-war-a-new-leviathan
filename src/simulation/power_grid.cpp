#include "simulation/power_grid.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace rts {

namespace {
bool valid_faction(int faction) { return faction >= 0 && faction < PowerGrid::kFactions; }
bool valid_owner(int owner) { return owner == -1 || valid_faction(owner); }
}

void PowerGrid::reset() {
    nodes_.clear();
    lines_.clear();
    structure_demand_.fill(0.0f);
}

bool PowerGrid::add_node(const PowerNode& node) {
    if (node.id == 0 || nodes_.contains(node.id) || !std::isfinite(node.x) || !std::isfinite(node.y) ||
        !valid_owner(node.owner) || !std::isfinite(node.output_mw) || node.output_mw < 0.0f ||
        !std::isfinite(node.demand_mw) || node.demand_mw < 0.0f) return false;
    // A load belongs to the side it serves, for good.
    if (node.kind == PowerNodeKind::LOAD && !valid_faction(node.owner)) return false;
    nodes_[node.id] = node;
    return true;
}

bool PowerGrid::add_line(uint32_t id, uint32_t from, uint32_t to, PowerLineKind kind, int owner,
                         const std::vector<std::array<float, 2>>& via) {
    const auto first = nodes_.find(from);
    const auto last = nodes_.find(to);
    if (id == 0 || lines_.contains(id) || from == to || first == nodes_.end() || last == nodes_.end() ||
        !valid_owner(owner)) return false;
    PowerLine line;
    line.id = id;
    line.from = from;
    line.to = to;
    line.kind = kind;
    line.owner = owner;
    line.path.push_back({first->second.x, first->second.y});
    for (const auto& point : via) {
        if (!std::isfinite(point[0]) || !std::isfinite(point[1])) return false;
        line.path.push_back(point);
    }
    line.path.push_back({last->second.x, last->second.y});
    for (size_t index = 1; index < line.path.size(); ++index) {
        line.length += std::hypot(line.path[index][0] - line.path[index - 1][0],
                                  line.path[index][1] - line.path[index - 1][1]);
    }
    if (line.length <= 0.0f) return false;
    lines_[id] = std::move(line);
    return true;
}

const PowerNode* PowerGrid::node(uint32_t id) const {
    const auto found = nodes_.find(id);
    return found == nodes_.end() ? nullptr : &found->second;
}

const PowerLine* PowerGrid::line(uint32_t id) const {
    const auto found = lines_.find(id);
    return found == lines_.end() ? nullptr : &found->second;
}

bool PowerGrid::capture_station(uint32_t id, int faction) {
    const auto found = nodes_.find(id);
    if (found == nodes_.end() || found->second.kind != PowerNodeKind::STATION || !valid_faction(faction) ||
        found->second.owner == faction) return false;
    found->second.owner = faction;
    return true;
}

bool PowerGrid::capture_line(uint32_t id, int faction) {
    const auto found = lines_.find(id);
    if (found == lines_.end() || !valid_faction(faction) || !found->second.intact ||
        found->second.owner == faction) return false;
    found->second.owner = faction;
    return true;
}

bool PowerGrid::sever_line(uint32_t id, float x, float y) {
    const auto found = lines_.find(id);
    if (found == lines_.end() || !found->second.intact) return false;
    PowerLine& line = found->second;
    distance_to_line(id, x, y, &line.cut_x, &line.cut_y);
    line.intact = false;
    line.repair_remaining_ms = 0.0f;
    line.repair_faction = -1;
    return true;
}

bool PowerGrid::begin_repair(uint32_t id, int faction) {
    const auto found = lines_.find(id);
    if (found == lines_.end() || !valid_faction(faction) || found->second.intact ||
        found->second.repair_remaining_ms > 0.0f) return false;
    found->second.repair_remaining_ms = repair_seconds(id) * 1000.0f;
    found->second.repair_faction = faction;
    return true;
}

std::vector<uint32_t> PowerGrid::update(float delta_ms) {
    std::vector<uint32_t> repaired;
    for (auto& [id, line] : lines_) {
        if (line.intact || line.repair_remaining_ms <= 0.0f) continue;
        line.repair_remaining_ms -= delta_ms;
        if (line.repair_remaining_ms > 0.0f) continue;
        line.repair_remaining_ms = 0.0f;
        line.intact = true;
        // Whoever rebuilt it runs it.
        line.owner = line.repair_faction;
        line.repair_faction = -1;
        repaired.push_back(id);
    }
    return repaired;
}

float PowerGrid::repair_cost(uint32_t id) const {
    const PowerLine* found = line(id);
    if (!found) return 0.0f;
    const PowerLineRules rules = power_line_rules(found->kind);
    return std::round(rules.material_base + rules.material_per_meter * found->length);
}

float PowerGrid::repair_seconds(uint32_t id) const {
    const PowerLine* found = line(id);
    if (!found) return 0.0f;
    const PowerLineRules rules = power_line_rules(found->kind);
    return rules.repair_seconds_base + rules.repair_seconds_per_meter * found->length;
}

void PowerGrid::add_structure_demand(int faction, float demand_mw) {
    if (valid_faction(faction) && std::isfinite(demand_mw) && demand_mw > 0.0f) {
        structure_demand_[static_cast<size_t>(faction)] += demand_mw;
    }
}

std::vector<uint32_t> PowerGrid::stations_reached_from(uint32_t load_id, int faction) const {
    std::vector<uint32_t> stations;
    std::set<uint32_t> visited{load_id};
    std::vector<uint32_t> frontier{load_id};
    while (!frontier.empty()) {
        const uint32_t current = frontier.back();
        frontier.pop_back();
        for (const auto& [id, line] : lines_) {
            (void)id;
            if (!line.intact || (line.owner != -1 && line.owner != faction)) continue;
            if (line.from != current && line.to != current) continue;
            const uint32_t next = line.from == current ? line.to : line.from;
            if (!visited.insert(next).second) continue;
            const PowerNode& reached = nodes_.at(next);
            if (reached.kind == PowerNodeKind::STATION) {
                // Power does not pass through a station the other side holds.
                if (reached.owner != -1 && reached.owner != faction) continue;
                stations.push_back(next);
            }
            frontier.push_back(next);
        }
    }
    return stations;
}

PowerSupply PowerGrid::supply(int faction) const {
    PowerSupply supply;
    if (!valid_faction(faction)) return supply;
    supply.demand_mw = structure_demand_[static_cast<size_t>(faction)];
    std::set<uint32_t> stations;
    for (const auto& [id, node] : nodes_) {
        if (node.kind != PowerNodeKind::LOAD || node.owner != faction) continue;
        supply.demand_mw += node.demand_mw;
        for (const uint32_t station : stations_reached_from(id, faction)) stations.insert(station);
    }
    for (const uint32_t station : stations) supply.generation_mw += nodes_.at(station).output_mw;
    return supply;
}

bool PowerGrid::load_powered(uint32_t load_id) const {
    const PowerNode* load = node(load_id);
    return load && load->kind == PowerNodeKind::LOAD && !stations_reached_from(load_id, load->owner).empty();
}

float PowerGrid::distance_to_line(uint32_t id, float x, float y, float* nearest_x, float* nearest_y) const {
    const PowerLine* found = line(id);
    float best = std::numeric_limits<float>::infinity();
    if (!found) return best;
    for (size_t index = 1; index < found->path.size(); ++index) {
        const float ax = found->path[index - 1][0];
        const float ay = found->path[index - 1][1];
        const float dx = found->path[index][0] - ax;
        const float dy = found->path[index][1] - ay;
        const float span = dx * dx + dy * dy;
        const float along = span > 0.0f ? std::clamp(((x - ax) * dx + (y - ay) * dy) / span, 0.0f, 1.0f) : 0.0f;
        const float px = ax + dx * along;
        const float py = ay + dy * along;
        const float distance = std::hypot(x - px, y - py);
        if (distance < best) {
            best = distance;
            if (nearest_x) *nearest_x = px;
            if (nearest_y) *nearest_y = py;
        }
    }
    return best;
}

std::vector<std::array<float, 2>> PowerGrid::sample_line(uint32_t id, float spacing) const {
    std::vector<std::array<float, 2>> points;
    const PowerLine* found = line(id);
    if (!found || !(spacing > 0.0f)) return points;
    points.push_back(found->path.front());
    for (size_t index = 1; index < found->path.size(); ++index) {
        const auto& a = found->path[index - 1];
        const auto& b = found->path[index];
        const float span = std::hypot(b[0] - a[0], b[1] - a[1]);
        const int steps = std::max(1, static_cast<int>(std::ceil(span / spacing)));
        for (int step = 1; step <= steps; ++step) {
            const float along = static_cast<float>(step) / static_cast<float>(steps);
            points.push_back({a[0] + (b[0] - a[0]) * along, a[1] + (b[1] - a[1]) * along});
        }
    }
    return points;
}

} // namespace rts
