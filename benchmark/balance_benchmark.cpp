// Fights each faction's standard army against the others on equal budgets.
//
// Armies come from data/balance_matchups.json and prices from unit data, so a
// balance change never needs a code change. Both sides run the same simple
// attack-move: close on the nearest enemy, then hold while one is in range. The
// process fails if any pairing, averaged across sides of the field, leaves one
// faction further ahead than the tolerance in the data file.
#include "data/json_parser.hpp"


#include "ecs/components/weapon.hpp"
#include "simulation/simulation.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace rts;

namespace {
struct Weighted {
    UnitType type;
    int weight;
};

struct Army {
    FactionId faction;
    std::string name;
    std::vector<Weighted> mix;
};

struct Fielded {
    EntityId id;
    float price;
};

struct Outcome {
    double first_value = 0.0;
    double second_value = 0.0;
    int ticks = 0;
    size_t first_units = 0;
    size_t second_units = 0;
};

float unit_price(FactionId faction, UnitType type) {
    const float discount = get_faction_start_data().at(faction).unit_cost_discount;
    return get_unit_prototypes().at(type).material_cost * (1.0f + discount);
}

std::vector<Fielded> deploy(Simulation& simulation, const Army& army, float budget, float x) {
    std::vector<Fielded> force;
    float spent = 0.0f;
    size_t position = 0;
    for (bool buying = true; buying;) {
        buying = false;
        for (const auto& entry : army.mix) {
            for (int count = 0; count < entry.weight; ++count) {
                const float price = unit_price(army.faction, entry.type);
                if (spent + price > budget) continue;
                const float row = static_cast<float>(position / 30);
                const float column = static_cast<float>(position % 30) - 14.5f;
                const int id = simulation.create_unit_with_type(x + (x < 0 ? -row : row) * 12.0f, column * 12.0f,
                                                                entry.type, army.faction);
                if (id < 0) throw std::runtime_error("Balance unit spawn rejected");
                simulation.stop_unit(static_cast<EntityId>(id));
                force.push_back({static_cast<EntityId>(id), price});
                spent += price;
                ++position;
                buying = true;
            }
        }
    }
    return force;
}

// Keeps only living units and returns their health-weighted value.
double assess(Simulation& simulation, std::vector<Fielded>& force) {
    double value = 0.0;
    std::vector<Fielded> alive;
    for (const auto& unit : force) {
        const auto* health = simulation.component_manager().get_component<Health>(unit.id);
        if (!health || health->is_dead) continue;
        value += static_cast<double>(unit.price) * health->current / health->max;
        alive.push_back(unit);
    }
    force = std::move(alive);
    return value;
}

void order(Simulation& simulation, const std::vector<Fielded>& force, const std::vector<Fielded>& enemies,
           FactionId faction, size_t first, size_t count) {
    auto& components = simulation.component_manager();
    for (size_t index = first; index < std::min(force.size(), first + count); ++index) {
        const auto id = force[index].id;
        const auto* position = components.get_component<Position>(id);
        const auto* weapon = components.get_component<Weapon>(id);
        if (!position || !weapon) continue;
        float nearest = std::numeric_limits<float>::max();
        float target_x = 0.0f, target_y = 0.0f;
        for (const auto& enemy : enemies) {
            const auto* other = components.get_component<Position>(enemy.id);
            if (!other) continue;
            const float distance = std::hypot(other->x - position->x, other->y - position->y);
            if (distance < nearest) {
                nearest = distance;
                target_x = other->x;
                target_y = other->y;
            }
        }
        const bool moving = simulation.has_move_target(id);
        if (nearest <= weapon->range * 0.92f) {
            if (moving) simulation.issue_commands({id}, faction, CommandType::STOP);
        } else if (nearest < std::numeric_limits<float>::max()) {
            simulation.issue_commands({id}, faction, CommandType::MOVE, target_x, target_y);
        }
    }
}

Outcome fight(const Army& first, const Army& second, float budget, float separation, int maximum_ticks) {
    Simulation simulation;
    simulation.configure_world_size(6000.0f, 6000.0f);
    simulation.set_unit_cap_per_player(100000);
    simulation.enable_ai(false);
    simulation.start();
    auto first_force = deploy(simulation, first, budget, -separation * 0.5f);
    auto second_force = deploy(simulation, second, budget, separation * 0.5f);
    Outcome outcome;
    const size_t batch = 100;
    for (int tick = 0; tick < maximum_ticks; ++tick) {
        outcome.first_value = assess(simulation, first_force);
        outcome.second_value = assess(simulation, second_force);
        outcome.ticks = tick;
        if (first_force.empty() || second_force.empty()) break;
        // The two sides order on alternate ticks, in a rolling window per side.
        const auto window = static_cast<size_t>(tick / 2) * batch;
        if (tick % 2 == 0) order(simulation, first_force, second_force, first.faction, window % first_force.size(), batch);
        else order(simulation, second_force, first_force, second.faction, window % second_force.size(), batch);
        simulation.update(50.0f);
    }
    const double lost = 2.0 * budget - outcome.first_value - outcome.second_value;
    if (lost < budget * 0.5) throw std::runtime_error("Armies did not fight: " + first.name + " vs " + second.name);
    outcome.first_units = first_force.size();
    outcome.second_units = second_force.size();
    return outcome;
}

data::JsonValue load_matchups(const std::string& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("Balance data unavailable: " + path);
    std::stringstream buffer;
    buffer << input.rdbuf();
    auto parsed = data::JsonParser::parse(buffer.str());
    if (!parsed) throw std::runtime_error("Balance data is not valid JSON: " + path);
    return *parsed;
}

FactionId faction_from_key(const std::string& key) {
    if (key == "ELITE_PRECISION") return FactionId::ELITE_PRECISION;
    if (key == "MASS_WARFARE") return FactionId::MASS_WARFARE;
    if (key == "INDUSTRIAL_EXPERIMENTAL") return FactionId::INDUSTRIAL_EXPERIMENTAL;
    throw std::runtime_error("Unknown faction in balance data: " + key);
}
} // namespace

int main(int argc, char** argv) {
    try {
        const auto root = load_matchups(argc > 1 ? argv[1] : "data/balance_matchups.json");
        const auto budget = static_cast<float>(root.get("budget")->as_number());
        const auto separation = static_cast<float>(root.get("separation")->as_number());
        const auto maximum_ticks = static_cast<int>(root.get("maximum_ticks")->as_number());
        const auto tolerance = root.get("tolerance")->as_number();
        const auto armies_value = *root.get("armies");
        std::vector<Army> armies;
        for (const auto& entry : armies_value.as_array()) {
            Army army;
            army.name = entry.get("faction")->as_string();
            army.faction = faction_from_key(army.name);
            const auto units = *entry.get("units");
            for (const auto& [key, weight] : units.as_object()) {
                const auto type = unit_type_from_key(key);
                if (!type) throw std::runtime_error("Unknown unit in balance data: " + key);
                army.mix.push_back({*type, static_cast<int>(weight.as_number())});
            }
            std::sort(army.mix.begin(), army.mix.end(), [](const Weighted& a, const Weighted& b) {
                return static_cast<int>(a.type) < static_cast<int>(b.type);
            });
            armies.push_back(std::move(army));
        }

        bool balanced = true;
        std::cout << "budget=" << budget << " tolerance=" << tolerance << '\n';
        for (size_t a = 0; a < armies.size(); ++a) {
            for (size_t b = a + 1; b < armies.size(); ++b) {
                const auto left = fight(armies[a], armies[b], budget, separation, maximum_ticks);
                const auto right = fight(armies[b], armies[a], budget, separation, maximum_ticks);
                const double score = ((left.first_value - left.second_value) + (right.second_value - right.first_value)) / (2.0 * budget);
                const bool within = std::abs(score) <= tolerance;
                balanced = balanced && within;
                std::cout << armies[a].name << " vs " << armies[b].name << ": score " << score
                          << (within ? " OK" : " OUTSIDE") << "  [left " << left.first_units << "/" << left.second_units
                          << " units, value " << left.first_value << "/" << left.second_value << ", " << left.ticks << " ticks]"
                          << "  [right " << right.second_units << "/" << right.first_units
                          << " units, value " << right.second_value << "/" << right.first_value << ", " << right.ticks << " ticks]\n";
            }
        }
        if (!balanced) {
            std::cerr << "FAIL: a faction pairing is outside the balance tolerance\n";
            return 1;
        }
        std::cout << "BALANCE_OK\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
