#ifndef STATS_STATS_MANAGER_HPP
#define STATS_STATS_MANAGER_HPP

#include "types.hpp"
#include <string>
#include <vector>
#include <filesystem>

namespace rts {

class StatsManager {
public:
    StatsManager(const std::filesystem::path& stats_dir);
    
    void record_match(const MatchStats& stats);
    GlobalStats get_summary() const;
    
    void export_stats(const std::filesystem::path& output_file) const;
    
    const std::filesystem::path& get_stats_dir() const { return stats_dir_; }

private:
    std::filesystem::path stats_dir_;
    
    void load_all_matches(std::vector<MatchStats>& matches) const;
    void save_summary(const GlobalStats& summary) const;
};

}

#endif
