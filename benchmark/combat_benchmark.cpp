#include "simulation/simulation.hpp"
#include "ecs/components/factions.hpp"
#include "ecs/components/weapon.hpp"
#include <cstring>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct CombatResult {
    int units_per_faction{};
    int tick_count{};
    double command_enqueue_ms{};
    double cold_first_tick_ms{};
    double cache_hit_tick_average_ms{};
    double cache_hit_tick_p50_ms{};
    double cache_hit_tick_p95_ms{};
    double cache_hit_tick_max_ms{};
    size_t initial_live_factions{};
    size_t final_live_factions{};
    size_t projectiles_fired{};
    size_t units_destroyed{};
    double snapshot_avg_ms{};
    std::uint64_t initial_hash{};
    std::uint64_t final_hash{};
};

double elapsed_ms(Clock::time_point start, Clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

std::uint64_t fnv_mix(std::uint64_t hash, std::uint32_t value) {
    constexpr std::uint64_t FNV_PRIME = 1099511628211ULL;
    for (int byte = 0; byte < 4; ++byte) {
        hash ^= (value >> (byte * 8)) & 0xffU;
        hash *= FNV_PRIME;
    }
    return hash;
}

std::uint64_t state_hash(const rts::SimulationState& state) {
    std::uint64_t hash = 1469598103934665603ULL;
    hash = fnv_mix(hash, state.tick_number);
    for (size_t i = 0; i < state.entity_ids.size(); ++i) {
        hash = fnv_mix(hash, state.entity_ids[i]);
        hash = fnv_mix(hash, static_cast<uint32_t>(state.positions_x[i] * 1000.0f));
        hash = fnv_mix(hash, static_cast<uint32_t>(state.positions_y[i] * 1000.0f));
        hash = fnv_mix(hash, static_cast<uint32_t>(state.health_current[i] * 1000.0f));
        hash = fnv_mix(hash, state.is_dead[i] ? 1U : 0U);
    }
    return hash;
}

double percentile(std::vector<double> samples, double fraction) {
    if (samples.empty()) {
        return 0.0;
    }
    std::sort(samples.begin(), samples.end());
    const auto index = static_cast<std::size_t>(
        std::ceil(fraction * static_cast<double>(samples.size())) - 1.0
    );
    return samples[std::min(index, samples.size() - 1)];
}

int parse_positive(const char* value, const char* name, int maximum) {
    try {
        std::size_t consumed = 0;
        const int parsed = std::stoi(value, &consumed);
        if (consumed != std::string(value).size() || parsed <= 0 || parsed > maximum) {
            throw std::invalid_argument("range");
        }
        return parsed;
    } catch (const std::exception&) {
        throw std::invalid_argument(
            std::string(name) + " must be an integer from 1 through " + std::to_string(maximum)
        );
    }
}

CombatResult run_combat_scenario(int units_per_faction, int tick_count) {
    rts::Simulation simulation;
    simulation.start();

    constexpr float FACTION_A_X = 50.0f;
    constexpr float FACTION_A_Y = 160.0f;
    constexpr float FACTION_B_X = 150.0f;
    constexpr float FACTION_B_Y = 160.0f;

    const auto enqueue_start = Clock::now();

    for (int i = 0; i < units_per_faction; ++i) {
        const float offset_x = static_cast<float>((i * 7) % 20) - 10.0f;
        const float offset_y = static_cast<float>((i * 11) % 20) - 10.0f;
        
        int entity_id = simulation.create_unit_with_type(
            FACTION_A_X + offset_x, FACTION_A_Y + offset_y,
            rts::UnitType::ELITE_MAIN_BATTLE_TANK,
            rts::FactionId::ELITE_PRECISION
        );
        if (entity_id >= 0) {
            auto* weapon = simulation.component_manager().get_component<rts::Weapon>(
                static_cast<rts::EntityId>(entity_id)
            );
            if (weapon) {
                weapon->cooldown_remaining = 0.0f;
            }
        }


        entity_id = simulation.create_unit_with_type(
            FACTION_B_X + offset_x, FACTION_B_Y + offset_y,
            rts::UnitType::MASS_SWARM_TANK,
            rts::FactionId::MASS_WARFARE
        );

        if (entity_id >= 0) {
            auto* weapon = simulation.component_manager().get_component<rts::Weapon>(
                static_cast<rts::EntityId>(entity_id)
            );
            if (weapon) {
                weapon->cooldown_remaining = 0.0f;
            }
        }
    }

    const double command_enqueue_ms = elapsed_ms(enqueue_start, Clock::now());

    const rts::SimulationState initial_state = simulation.get_state();
    const std::uint64_t initial_hash = state_hash(initial_state);

    std::vector<double> tick_samples;
    tick_samples.reserve(static_cast<std::size_t>(tick_count));

    for (int tick = 0; tick < tick_count; ++tick) {
        const auto tick_start = Clock::now();
        simulation.update(50.0f);
        tick_samples.push_back(elapsed_ms(tick_start, Clock::now()));
        
    }

    const rts::SimulationState final_state = simulation.get_state();
    const std::uint64_t final_hash = state_hash(final_state);

     size_t initial_live = 0;
     for (size_t i = 0; i < initial_state.entity_ids.size(); ++i) {
         if (!initial_state.is_dead[i]) initial_live++;
     }
     size_t final_live = 0;
     for (size_t i = 0; i < final_state.entity_ids.size(); ++i) {
         if (!final_state.is_dead[i]) final_live++;
     }
    
    // Dead units are removed from SimulationState during the combat phase, so
    // state-local dead counts cannot measure losses. Live-count reduction is
    // the authoritative destruction signal for this scenario.
    const size_t units_destroyed = initial_live >= final_live ? initial_live - final_live : 0;
    const size_t projectiles_spawned = simulation.combat_manager().projectile_manager().total_spawned();

     return {
        units_per_faction,
        tick_count,
        command_enqueue_ms,
        tick_samples.empty() ? 0.0 : tick_samples[0],
        [&tick_samples]() -> double {
            if (tick_samples.size() <= 1) return 0.0;
            std::vector<double> samples(tick_samples.begin() + 1, tick_samples.end());
            double total = 0.0;
            for (double s : samples) total += s;
            return total / static_cast<double>(samples.size());
        }(),
        percentile(tick_samples, 0.50),
        percentile(tick_samples, 0.95),
        tick_samples.empty() ? 0.0 : *std::max_element(tick_samples.begin(), tick_samples.end()),
         initial_live,
         final_live,
         projectiles_spawned,
         units_destroyed,
        0.0,
        initial_hash,
        final_hash
    };
}

void print_result(const CombatResult& result) {
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Combat benchmark results\n"
              << "  units per faction: " << result.units_per_faction << '\n'
              << "  total initial units: " << result.initial_live_factions << '\n'
              << "  total final units: " << result.final_live_factions << '\n'
              << "  units destroyed: " << result.units_destroyed << '\n'
              << "  ticks: " << result.tick_count << '\n'
              << "  projectiles fired: " << result.projectiles_fired << '\n'
              << "  command enqueue: " << result.command_enqueue_ms << " ms\n"
              << "  cold first tick: " << result.cold_first_tick_ms << " ms\n"
              << "  cache-hit tick avg/p50/p95/max: "
              << result.cache_hit_tick_average_ms << " / " << result.cache_hit_tick_p50_ms << " / "
              << result.cache_hit_tick_p95_ms << " / " << result.cache_hit_tick_max_ms << " ms\n"
              << "  initial/final state hash: " << result.initial_hash << " / " << result.final_hash << '\n';
}

bool verify_combat(const CombatResult& result) {
    if (result.initial_live_factions == 0) {
        std::cerr << "FAIL: no units spawned\n";
        return false;
    }
    if (result.units_destroyed == 0) {
        std::cerr << "FAIL: no units destroyed despite combat simulation\n";
        return false;
    }
    if (result.final_live_factions >= result.initial_live_factions) {
        std::cerr << "FAIL: final units not less than initial after combat\n";
        return false;
    }
    if (result.projectiles_fired == 0) {
        std::cerr << "FAIL: no projectiles spawned\n";
        return false;
    }
    if (result.initial_hash == result.final_hash) {
        std::cerr << "FAIL: state hash unchanged after combat\n";
        return false;
    }
    if (result.cache_hit_tick_average_ms > 12.0) {
        std::cerr << "FAIL: tick latency avg too high: " << result.cache_hit_tick_average_ms << " ms\n";
        return false;
    }
    return true;
}

}

int main(int argc, char** argv) {
    try {
        const int units_per_faction = argc > 1 ? parse_positive(argv[1], "units_per_faction", 10000) : 500;
        const int tick_count = argc > 2 ? parse_positive(argv[2], "tick_count", 1000) : 100;
        if (argc > 3) {
            throw std::invalid_argument("Usage: rts_combat_benchmark [units_per_faction] [tick_count]");
        }

        if (units_per_faction > 10000) {
            throw std::invalid_argument("units_per_faction must not exceed 10000");
        }

        CombatResult result = run_combat_scenario(units_per_faction, tick_count);
        print_result(result);
        if (!verify_combat(result)) {
            return 1;
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Combat benchmark failed: " << error.what() << '\n';
        return 1;
    }
}
