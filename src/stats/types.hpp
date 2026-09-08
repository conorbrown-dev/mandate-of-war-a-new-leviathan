#ifndef STATS_TYPES_HPP
#define STATS_TYPES_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace rts {

struct MatchStats {
    std::string map_name;
    uint64_t timestamp;
    std::string player_id;
    std::string faction;
    bool win;
    uint32_t duration_ticks;
    uint64_t units_built;
    uint64_t units_lost;
    uint64_t units_destroyed;
    uint64_t material_collected;
    uint64_t energy_collected;
    uint64_t research_generated;
    uint32_t peak_army_size;
};

struct FactionStats {
    std::string faction;
    uint32_t wins;
    uint32_t losses;
    uint32_t matches;
    double win_rate;
};

struct MapStats {
    std::string map_name;
    uint32_t matches;
    double average_duration_ticks;
};

struct GlobalStats {
    uint64_t total_matches;
    uint64_t total_wins;
    uint64_t total_losses;
    double average_match_duration_ticks;
    std::vector<FactionStats> faction_stats;
    std::vector<MapStats> map_stats;
};

}

#endif // STATS_TYPES_HPP
