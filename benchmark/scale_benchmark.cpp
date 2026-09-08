#include "simulation/simulation.hpp"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <unistd.h>

namespace {

using Clock = std::chrono::steady_clock;

struct ScenarioResult {
    int unit_count{};
    int tick_count{};
    int unique_destinations{};
    double command_enqueue_ms{};
    double field_generation_ms{};
    double tick_average_ms{};
    double tick_p50_ms{};
    double tick_p95_ms{};
    double tick_max_ms{};
    double snapshot_average_ms{};
    std::size_t moved_units{};
    double rss_mebibytes{};
    double rss_delta_mebibytes{};
    std::uint64_t initial_hash{};
    std::uint64_t final_hash{};
};

struct FormationResult {
    int unit_count{};
    int tick_count{};
    double command_enqueue_ms{};
    double cold_first_tick_ms{};
    double cache_hit_tick_average_ms{};
    double cache_hit_tick_p95_ms{};
    double cache_hit_tick_max_ms{};
    std::size_t moved_units{};
    std::size_t generated_fields{};
    std::size_t cached_field_bytes{};
};

double elapsed_ms(Clock::time_point start, Clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

std::size_t resident_bytes() {
    std::ifstream statm("/proc/self/statm");
    std::size_t total_pages = 0;
    std::size_t resident_pages = 0;
    if (!(statm >> total_pages >> resident_pages)) {
        return 0;
    }
    (void)total_pages;
    return resident_pages * static_cast<std::size_t>(::sysconf(_SC_PAGESIZE));
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
        hash = fnv_mix(hash, std::bit_cast<std::uint32_t>(state.positions_x[i]));
        hash = fnv_mix(hash, std::bit_cast<std::uint32_t>(state.positions_y[i]));
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

std::pair<float, float> destination_for(const rts::Pathfinding& pathfinding, int group) {
    return {
        pathfinding.to_world_x(180 + (group % 10) * 2),
        pathfinding.to_world_y(180 + (group / 10) * 2)
    };
}

ScenarioResult run_scenario(int unit_count, int tick_count, int unique_destinations) {
    constexpr int GRID_SIZE = 320;
    constexpr double BYTES_PER_MEBIBYTE = 1024.0 * 1024.0;

    const std::size_t rss_before = resident_bytes();
    rts::Simulation simulation;
    simulation.start();

    // A solid strategic barrier with deterministic gates forces routes to do
    // representative work instead of pointing directly at every destination.
    for (int y = 0; y < GRID_SIZE; ++y) {
        if (y % 20 != 0) {
            simulation.pathfinding().set_cell(160, y, false);
        }
    }

    std::vector<rts::EntityId> entities;
    entities.reserve(static_cast<std::size_t>(unit_count));
    for (int index = 0; index < unit_count; ++index) {
        const float x = simulation.pathfinding().to_world_x(40 + index % 100);
        const float y = simulation.pathfinding().to_world_y(100 + (index / 100) % 100);
        entities.push_back(simulation.create_unit(x, y).id);
    }

    const rts::SimulationState initial_state = simulation.get_state();
    if (initial_state.entity_ids.size() != static_cast<std::size_t>(unit_count)) {
        throw std::runtime_error("Initial entity count does not match the requested workload");
    }

    const auto fields_start = Clock::now();
    for (int group = 0; group < unique_destinations; ++group) {
        const auto destination = destination_for(simulation.pathfinding(), group);
        if (!simulation.pathfinding().prewarm_flow_field(destination.first, destination.second)) {
            throw std::runtime_error("Flow-field generation rejected a valid destination");
        }
    }
    const double field_generation_ms = elapsed_ms(fields_start, Clock::now());
    if (simulation.pathfinding().cached_flow_field_count() != static_cast<std::size_t>(unique_destinations)) {
        throw std::runtime_error("Prewarmed flow-field count does not match the scenario");
    }

    const auto enqueue_start = Clock::now();
    for (int index = 0; index < unit_count; ++index) {
        const auto destination = destination_for(simulation.pathfinding(), index % unique_destinations);
        simulation.move_unit(entities[static_cast<std::size_t>(index)], destination.first, destination.second);
    }
    const double command_enqueue_ms = elapsed_ms(enqueue_start, Clock::now());

    std::vector<double> tick_samples;
    tick_samples.reserve(static_cast<std::size_t>(tick_count));
    for (int tick = 0; tick < tick_count; ++tick) {
        const auto tick_start = Clock::now();
        simulation.update(50.0f);
        tick_samples.push_back(elapsed_ms(tick_start, Clock::now()));
    }

    if (simulation.entity_count() != static_cast<std::size_t>(unit_count)) {
        throw std::runtime_error("Entity count changed during the movement workload");
    }

    constexpr int SNAPSHOT_SAMPLES = 5;
    double snapshot_total_ms = 0.0;
    rts::SimulationState final_state;
    for (int sample = 0; sample < SNAPSHOT_SAMPLES; ++sample) {
        const auto snapshot_start = Clock::now();
        final_state = simulation.get_state();
        snapshot_total_ms += elapsed_ms(snapshot_start, Clock::now());
    }
    if (final_state.entity_ids.size() != static_cast<std::size_t>(unit_count)) {
        throw std::runtime_error("Final snapshot omitted active entities");
    }

    std::size_t moved_units = 0;
    for (rts::EntityId entity : entities) {
        int initial_idx = -1;
        int final_idx = -1;
        for (size_t i = 0; i < initial_state.entity_ids.size(); ++i) {
            if (initial_state.entity_ids[i] == entity) {
                initial_idx = static_cast<int>(i);
            }
            if (final_state.entity_ids[i] == entity) {
                final_idx = static_cast<int>(i);
            }
        }
        if (initial_idx < 0 || final_idx < 0) {
            throw std::runtime_error("Movement verification encountered incomplete state");
        }
        const float initial_x = initial_state.positions_x[initial_idx];
        const float initial_y = initial_state.positions_y[initial_idx];
        const float final_x = final_state.positions_x[final_idx];
        const float final_y = final_state.positions_y[final_idx];
        if (!std::isfinite(final_x) || !std::isfinite(final_y)) {
            throw std::runtime_error("Movement produced a non-finite position");
        }

        const int grid_x = simulation.pathfinding().to_grid_x(final_x);
        const int grid_y = simulation.pathfinding().to_grid_y(final_y);
        if (!simulation.pathfinding().is_walkable(grid_x, grid_y)) {
            throw std::runtime_error("Movement ended in a blocked or out-of-bounds cell");
        }

        const float dx = final_x - initial_x;
        const float dy = final_y - initial_y;
        if (dx * dx + dy * dy >= 1.0f) {
            ++moved_units;
        }
    }
    if (moved_units < static_cast<std::size_t>(unit_count * 95 / 100)) {
        throw std::runtime_error("Fewer than 95 percent of units performed representative movement");
    }

    const std::uint64_t initial_hash = state_hash(initial_state);
    const std::uint64_t final_hash = state_hash(final_state);
    if (initial_hash == final_hash) {
        throw std::runtime_error("State hash did not change during the active workload");
    }

    double tick_total_ms = 0.0;
    for (double sample : tick_samples) {
        tick_total_ms += sample;
    }
    const std::size_t rss_after = resident_bytes();

    return {
        unit_count,
        tick_count,
        unique_destinations,
        command_enqueue_ms,
        field_generation_ms,
        tick_total_ms / static_cast<double>(tick_samples.size()),
        percentile(tick_samples, 0.50),
        percentile(tick_samples, 0.95),
        *std::max_element(tick_samples.begin(), tick_samples.end()),
        snapshot_total_ms / SNAPSHOT_SAMPLES,
        moved_units,
        static_cast<double>(rss_after) / BYTES_PER_MEBIBYTE,
        static_cast<double>(rss_after - std::min(rss_before, rss_after)) / BYTES_PER_MEBIBYTE,
        initial_hash,
        final_hash
    };
}

void print_result(const ScenarioResult& result) {
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Scale benchmark passed\n"
              << "  units: " << result.unit_count << '\n'
              << "  ticks: " << result.tick_count << '\n'
              << "  unique destinations: " << result.unique_destinations << '\n'
              << "  moved units: " << result.moved_units << '\n'
              << "  command enqueue: " << result.command_enqueue_ms << " ms\n"
              << "  cold field generation: " << result.field_generation_ms << " ms\n"
              << "  cache-hit simulation tick avg/p50/p95/max: "
              << result.tick_average_ms << " / " << result.tick_p50_ms << " / "
              << result.tick_p95_ms << " / " << result.tick_max_ms << " ms\n"
              << "  full state snapshot average: " << result.snapshot_average_ms << " ms\n"
              << "  current RSS / scenario delta: " << result.rss_mebibytes << " / "
              << result.rss_delta_mebibytes << " MiB\n"
              << "  initial/final state hash: " << result.initial_hash << " / " << result.final_hash << '\n';

    std::cout << "CSV\n"
              << "units,ticks,destinations,enqueue_ms,field_generation_ms,tick_avg_ms,tick_p50_ms,tick_p95_ms,tick_max_ms,snapshot_avg_ms,moved_units,rss_mib,rss_delta_mib,initial_hash,final_hash\n"
              << result.unit_count << ',' << result.tick_count << ',' << result.unique_destinations << ','
              << result.command_enqueue_ms << ',' << result.field_generation_ms << ','
              << result.tick_average_ms << ',' << result.tick_p50_ms << ',' << result.tick_p95_ms << ','
              << result.tick_max_ms << ',' << result.snapshot_average_ms << ',' << result.moved_units << ','
              << result.rss_mebibytes << ',' << result.rss_delta_mebibytes << ','
              << result.initial_hash << ',' << result.final_hash << '\n';
}

FormationResult run_formation_scenario(int unit_count, int tick_count) {
    rts::Simulation simulation;
    simulation.start();
    auto& pathfinding = simulation.pathfinding();
    for (int y = 0; y < 320; ++y) {
        if (y % 20 != 0) {
            pathfinding.set_cell(160, y, false);
        }
    }

    std::vector<rts::EntityId> entities;
    entities.reserve(static_cast<std::size_t>(unit_count));
    for (int index = 0; index < unit_count; ++index) {
        entities.push_back(simulation.create_unit(
            pathfinding.to_world_x(40 + index % 100),
            pathfinding.to_world_y(100 + (index / 100) % 100)
        ).id);
    }
    const auto initial_state = simulation.get_state();

    const auto enqueue_start = Clock::now();
    simulation.move_units_formation(
        entities,
        pathfinding.to_world_x(220),
        pathfinding.to_world_y(160),
        0.4f
    );
    const double enqueue_ms = elapsed_ms(enqueue_start, Clock::now());

    const auto cold_start = Clock::now();
    simulation.update(50.0f);
    const double cold_first_tick_ms = elapsed_ms(cold_start, Clock::now());

    std::vector<double> cache_hit_ticks;
    cache_hit_ticks.reserve(static_cast<std::size_t>(std::max(0, tick_count - 1)));
    for (int tick = 1; tick < tick_count; ++tick) {
        const auto tick_start = Clock::now();
        simulation.update(50.0f);
        cache_hit_ticks.push_back(elapsed_ms(tick_start, Clock::now()));
    }

    const auto final_state = simulation.get_state();
    if (final_state.entity_ids.size() != static_cast<std::size_t>(unit_count)) {
        throw std::runtime_error("Formation workload changed the entity count");
    }

    std::size_t moved_units = 0;
    for (rts::EntityId entity : entities) {
        int initial_idx = -1;
        int final_idx = -1;
        for (size_t i = 0; i < initial_state.entity_ids.size(); ++i) {
            if (initial_state.entity_ids[i] == entity) {
                initial_idx = static_cast<int>(i);
            }
            if (final_state.entity_ids[i] == entity) {
                final_idx = static_cast<int>(i);
            }
        }
        if (initial_idx < 0 || final_idx < 0) {
            throw std::runtime_error("Formation workload produced incomplete state");
        }
        const float initial_x = initial_state.positions_x[initial_idx];
        const float initial_y = initial_state.positions_y[initial_idx];
        const float final_x = final_state.positions_x[final_idx];
        const float final_y = final_state.positions_y[final_idx];
        const int grid_x = pathfinding.to_grid_x(final_x);
        const int grid_y = pathfinding.to_grid_y(final_y);
        if (!std::isfinite(final_x) || !std::isfinite(final_y) ||
            !pathfinding.is_walkable(grid_x, grid_y)) {
            throw std::runtime_error("Formation workload produced an invalid position");
        }
        const float dx = final_x - initial_x;
        const float dy = final_y - initial_y;
        if (dx * dx + dy * dy >= 1.0f) {
            ++moved_units;
        }
    }
    if (moved_units < static_cast<std::size_t>(unit_count * 95 / 100)) {
        throw std::runtime_error("Formation workload did not move at least 95 percent of units");
    }
    if (pathfinding.flow_field_generation_count() > 1 || pathfinding.cached_flow_field_count() > 1) {
        throw std::runtime_error("One formation order generated more than one strategic field");
    }

    double cache_hit_total = 0.0;
    for (double sample : cache_hit_ticks) {
        cache_hit_total += sample;
    }
    return {
        unit_count,
        tick_count,
        enqueue_ms,
        cold_first_tick_ms,
        cache_hit_total / static_cast<double>(cache_hit_ticks.size()),
        percentile(cache_hit_ticks, 0.95),
        *std::max_element(cache_hit_ticks.begin(), cache_hit_ticks.end()),
        moved_units,
        pathfinding.flow_field_generation_count(),
        pathfinding.cached_flow_field_bytes()
    };
}

void print_formation_result(const FormationResult& result) {
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Formation benchmark passed\n"
              << "  units/moved: " << result.unit_count << " / " << result.moved_units << '\n'
              << "  ticks: " << result.tick_count << '\n'
              << "  batch command enqueue: " << result.command_enqueue_ms << " ms\n"
              << "  cold command-to-first-tick: " << result.cold_first_tick_ms << " ms\n"
              << "  cache-hit tick avg/p95/max: " << result.cache_hit_tick_average_ms << " / "
              << result.cache_hit_tick_p95_ms << " / " << result.cache_hit_tick_max_ms << " ms\n"
              << "  generated fields / cached bytes: " << result.generated_fields << " / "
              << result.cached_field_bytes << '\n';
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc > 1 && std::string(argv[1]) == "--formation") {
            const int unit_count = argc > 2 ? parse_positive(argv[2], "unit_count", 100000) : 1000;
            const int tick_count = argc > 3 ? parse_positive(argv[3], "tick_count", 10000) : 100;
            if (argc > 4 || tick_count < 2) {
                throw std::invalid_argument("Usage: rts_scale_benchmark --formation [unit_count] [tick_count>=2]");
            }
            print_formation_result(run_formation_scenario(unit_count, tick_count));
            return 0;
        }

        const int unit_count = argc > 1 ? parse_positive(argv[1], "unit_count", 100000) : 1000;
        const int tick_count = argc > 2 ? parse_positive(argv[2], "tick_count", 10000) : 100;
        const int unique_destinations = argc > 3
            ? parse_positive(argv[3], "unique_destinations", static_cast<int>(rts::Pathfinding::MAX_CACHED_FLOW_FIELDS))
            : 100;
        if (argc > 4) {
            throw std::invalid_argument("Usage: rts_scale_benchmark [unit_count] [tick_count] [unique_destinations]");
        }

        print_result(run_scenario(unit_count, tick_count, unique_destinations));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Scale benchmark failed: " << error.what() << '\n';
        return 1;
    }
}
