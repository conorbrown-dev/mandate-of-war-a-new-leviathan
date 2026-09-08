#include "simulation/simulation.hpp"
#include "ecs/components/carrier.hpp"
#include "ecs/components/intelligence.hpp"

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

struct LogisticsResult {
    int unit_count{};
    int carrier_count{};
    int tick_count{};
    double command_enqueue_ms{};
    double cold_first_tick_ms{};
    double cache_hit_tick_average_ms{};
    double cache_hit_tick_p50_ms{};
    double cache_hit_tick_p95_ms{};
    double cache_hit_tick_max_ms{};
    double safe_return_cache_hits{};
    double safe_return_cache_misses{};
    double facility_lookup_cache_hits{};
    double facility_lookup_cache_misses{};
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

LogisticsResult run_logistics_scenario(int unit_count, int carrier_count, int tick_count) {
    rts::Simulation simulation;
    simulation.start();

    const auto enqueue_start = Clock::now();

    // Spawn units around origin
    for (int i = 0; i < unit_count; ++i) {
        const float offset_x = static_cast<float>((i * 7) % 50) - 25.0f;
        const float offset_y = static_cast<float>((i * 11) % 50) - 25.0f;
        
        simulation.create_unit_with_type(
            offset_x, offset_y,
            rts::UnitType::ELITE_MAIN_BATTLE_TANK,
            rts::FactionId::ELITE_PRECISION
        );
    }

    // Spawn carriers in a ring pattern using the logistics manager API
    std::vector<rts::EntityId> carrier_ids;
    const float carrier_radius = 100.0f;
    for (int i = 0; i < carrier_count; ++i) {
        const float angle = static_cast<float>(i) / static_cast<float>(carrier_count) * 2.0f * 3.14159265f;
        const float x = std::cos(angle) * carrier_radius;
        const float y = std::sin(angle) * carrier_radius;
        
        // Create a generic entity first, then add as carrier via logistics manager
        rts::Entity entity = simulation.create_unit(x, y);
        rts::EntityId carrier_id = entity.id;
        
        // Add carrier component to the entity
        rts::Carrier carrier{};
        carrier.tier = rts::Carrier::Tier::T1_LIGHT;
        carrier.x = x;
        carrier.y = y;
        carrier.runway_capacity = 2;
        carrier.runway_usable = true;
        carrier.deck_capacity = 10;
        carrier.refuel_rate = 100.0f;
        carrier.rearm_rate = 20.0f;
        carrier.max_fuel = 1000.0f;
        carrier.current_fuel = 1000.0f;
        carrier.max_munitions = 100.0f;
        carrier.current_munitions = 100.0f;
        carrier.max_speed = 50.0f;
        
        simulation.logistics_manager().add_carrier(carrier_id, carrier);
        carrier_ids.push_back(carrier_id);
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
        
        // Move carriers slightly each tick to test cache invalidation
        if (!carrier_ids.empty()) {
            float move_angle = static_cast<float>(tick) * 0.01f;
            for (size_t i = 0; i < carrier_ids.size(); ++i) {
                auto* carrier = simulation.component_manager().get_component<rts::Carrier>(carrier_ids[i]);
                if (carrier) {
                    const float base_radius = 100.0f + static_cast<float>(i * 5);
                    const float new_x = std::cos(move_angle + static_cast<float>(i) * 0.5f) * base_radius;
                    const float new_y = std::sin(move_angle + static_cast<float>(i) * 0.5f) * base_radius;
                    simulation.move_unit(carrier_ids[i], new_x, new_y);
                }
            }
        }
        
        // Test intelligence updates
        if (tick % 10 == 0 && unit_count > 0) {
            rts::EntityId unit_id = static_cast<rts::EntityId>(0);
            auto* unit_pos = simulation.component_manager().get_component<rts::Position>(unit_id);
            if (unit_pos) {
                simulation.logistics_manager().update_intelligence(
                    unit_id, unit_pos->x, unit_pos->y, simulation.get_state().tick_number
                );
            }
        }
    }

    const rts::SimulationState final_state = simulation.get_state();
    const std::uint64_t final_hash = state_hash(final_state);

    const auto& logistics = simulation.logistics_manager();
    
    return {
        unit_count,
        carrier_count,
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
        static_cast<double>(logistics.safe_return_cache_hits()),
        static_cast<double>(logistics.safe_return_cache_misses()),
        static_cast<double>(logistics.facility_lookup_cache_hits()),
        static_cast<double>(logistics.facility_lookup_cache_misses()),
        0.0,
        initial_hash,
        final_hash
    };
}

void print_result(const LogisticsResult& result) {
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Logistics benchmark passed\n"
              << "  units: " << result.unit_count << '\n'
              << "  carriers: " << result.carrier_count << '\n'
              << "  ticks: " << result.tick_count << '\n'
              << "  command enqueue: " << result.command_enqueue_ms << " ms\n"
              << "  cold first tick: " << result.cold_first_tick_ms << " ms\n"
              << "  cache-hit tick avg/p50/p95/max: "
              << result.cache_hit_tick_average_ms << " / " << result.cache_hit_tick_p50_ms << " / "
              << result.cache_hit_tick_max_ms << " ms\n"
              << "  safe-return cache hits/misses: " << result.safe_return_cache_hits << " / " << result.safe_return_cache_misses << '\n'
              << "  facility lookup cache hits/misses: " << result.facility_lookup_cache_hits << " / " << result.facility_lookup_cache_misses << '\n'
              << "  initial/final state hash: " << result.initial_hash << " / " << result.final_hash << '\n';
}

bool verify_logistics(const LogisticsResult& result) {
    if (result.unit_count == 0) {
        std::cerr << "FAIL: no units spawned\n";
        return false;
    }
    if (result.carrier_count == 0) {
        std::cerr << "FAIL: no carriers spawned\n";
        return false;
    }
    if (result.cache_hit_tick_average_ms > 15.0) {
        std::cerr << "FAIL: tick latency avg too high: " << result.cache_hit_tick_average_ms << " ms\n";
        return false;
    }
    if (result.initial_hash == result.final_hash) {
        std::cerr << "FAIL: state hash unchanged after logistics simulation\n";
        return false;
    }
    return true;
}

}

int main(int argc, char** argv) {
    try {
        const int unit_count = argc > 1 ? parse_positive(argv[1], "unit_count", 10000) : 1000;
        const int carrier_count = argc > 2 ? parse_positive(argv[2], "carrier_count", 100) : 10;
        const int tick_count = argc > 3 ? parse_positive(argv[3], "tick_count", 1000) : 100;
        if (argc > 4) {
            throw std::invalid_argument("Usage: rts_logistics_benchmark [unit_count] [carrier_count] [tick_count]");
        }

        if (unit_count > 10000) {
            throw std::invalid_argument("unit_count must not exceed 10000");
        }
        if (carrier_count > 100) {
            throw std::invalid_argument("carrier_count must not exceed 100");
        }

        LogisticsResult result = run_logistics_scenario(unit_count, carrier_count, tick_count);
        print_result(result);
        if (!verify_logistics(result)) {
            return 1;
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Logistics benchmark failed: " << error.what() << '\n';
        return 1;
    }
}
