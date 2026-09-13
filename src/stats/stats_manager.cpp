#include "stats_manager.hpp"
#include <fstream>
#include <algorithm>
#include <sstream>

namespace rts {

StatsManager::StatsManager(const std::filesystem::path& stats_dir)
    : stats_dir_(stats_dir) {
    if (!std::filesystem::exists(stats_dir_)) {
        std::filesystem::create_directories(stats_dir_);
    }
}

void StatsManager::record_match(const MatchStats& stats) {
    std::filesystem::path file = stats_dir_ / 
        ("match_" + std::to_string(stats.timestamp) + ".txt");
    
    std::ofstream ofs(file);
    ofs << "map_name=" << stats.map_name << "\n";
    ofs << "timestamp=" << stats.timestamp << "\n";
    ofs << "player_id=" << stats.player_id << "\n";
    ofs << "faction=" << stats.faction << "\n";
    ofs << "win=" << (stats.win ? "true" : "false") << "\n";
    ofs << "duration_ticks=" << stats.duration_ticks << "\n";
    ofs << "units_built=" << stats.units_built << "\n";
    ofs << "units_lost=" << stats.units_lost << "\n";
    ofs << "units_destroyed=" << stats.units_destroyed << "\n";
    ofs << "material_collected=" << stats.material_collected << "\n";
    ofs << "energy_collected=" << stats.energy_collected << "\n";
    ofs << "research_generated=" << stats.research_generated << "\n";
    ofs << "peak_army_size=" << stats.peak_army_size << "\n";
    ofs.close();
}

GlobalStats StatsManager::get_summary() const {
    std::vector<MatchStats> matches;
    load_all_matches(matches);
    
    GlobalStats summary{};
    summary.total_matches = static_cast<uint64_t>(matches.size());
    
    std::unordered_map<std::string, uint32_t> faction_wins;
    std::unordered_map<std::string, uint32_t> faction_losses;
    std::unordered_map<std::string, uint32_t> map_matches;
    uint64_t total_duration = 0;
    
    for (const auto& m : matches) {
        total_duration += m.duration_ticks;
        if (m.win) {
            faction_wins[m.faction]++;
            summary.total_wins++;
        } else {
            faction_losses[m.faction]++;
            summary.total_losses++;
        }
        map_matches[m.map_name]++;
    }
    
    summary.average_match_duration_ticks = 
        summary.total_matches > 0 ? 
            static_cast<double>(total_duration) / summary.total_matches : 0.0;
    
    for (const auto& [faction, wins] : faction_wins) {
        FactionStats fs;
        fs.faction = faction;
        fs.wins = wins;
        fs.losses = faction_losses.count(faction) > 0 ? 
            faction_losses[faction] : 0;
        fs.matches = fs.wins + fs.losses;
        fs.win_rate = fs.matches > 0 ? 
            static_cast<double>(fs.wins) / fs.matches : 0.0;
        summary.faction_stats.push_back(fs);
    }
    
    for (const auto& [map, matches_count] : map_matches) {
        MapStats ms;
        ms.map_name = map;
        ms.matches = matches_count;
        ms.average_duration_ticks = 0.0;
        
        double total_map_duration = 0.0;
        uint32_t map_match_count = 0;
        for (const auto& m : matches) {
            if (m.map_name == map) {
                total_map_duration += m.duration_ticks;
                map_match_count++;
            }
        }
        if (map_match_count > 0) {
            ms.average_duration_ticks = total_map_duration / map_match_count;
        }
        summary.map_stats.push_back(ms);
    }
    
    save_summary(summary);
    return summary;
}

void StatsManager::export_stats(const std::filesystem::path& output_file) const {
    GlobalStats summary = get_summary();
    
    std::ofstream ofs(output_file);
    ofs << "=== Match Statistics Summary ===\n\n";
    ofs << "Total Matches: " << summary.total_matches << "\n";
    ofs << "Total Wins: " << summary.total_wins << "\n";
    ofs << "Total Losses: " << summary.total_losses << "\n";
    ofs << "Average Match Duration: " << summary.average_match_duration_ticks << " ticks\n\n";
    
    ofs << "=== Faction Stats ===\n";
    for (const auto& fs : summary.faction_stats) {
        ofs << "  " << fs.faction << ": " << fs.wins << " wins, " 
            << fs.losses << " losses, " << fs.matches << " matches, "
            << (fs.win_rate * 100) << "% win rate\n";
    }
    ofs << "\n";
    
    ofs << "=== Map Stats ===\n";
    for (const auto& ms : summary.map_stats) {
        ofs << "  " << ms.map_name << ": " << ms.matches << " matches, "
            << ms.average_duration_ticks << " avg duration\n";
    }
    ofs.close();
}

void StatsManager::load_all_matches(std::vector<MatchStats>& matches) const {
    matches.clear();
    
    for (const auto& entry : std::filesystem::directory_iterator(stats_dir_)) {
        if (entry.is_regular_file() && 
            entry.path().filename().string().rfind("match_", 0) == 0 &&
            entry.path().extension() == ".txt") {
            std::ifstream ifs(entry.path());
            
            MatchStats m{};
            std::string line;
            while (std::getline(ifs, line)) {
                size_t pos = line.find('=');
                if (pos != std::string::npos) {
                    std::string key = line.substr(0, pos);
                    std::string value = line.substr(pos + 1);
                    
                    if (key == "map_name") m.map_name = value;
                    else if (key == "timestamp") m.timestamp = std::stoull(value);
                    else if (key == "player_id") m.player_id = value;
                    else if (key == "faction") m.faction = value;
                    else if (key == "win") m.win = (value == "true");
                    else if (key == "duration_ticks") m.duration_ticks = std::stoul(value);
                    else if (key == "units_built") m.units_built = std::stoull(value);
                    else if (key == "units_lost") m.units_lost = std::stoull(value);
                    else if (key == "units_destroyed") m.units_destroyed = std::stoull(value);
                    else if (key == "material_collected") m.material_collected = std::stoull(value);
                    else if (key == "energy_collected") m.energy_collected = std::stoull(value);
                    else if (key == "research_generated") m.research_generated = std::stoull(value);
                    else if (key == "peak_army_size") m.peak_army_size = std::stoul(value);
                }
            }
            matches.push_back(m);
        }
    }
    
    std::sort(matches.begin(), matches.end(), 
              [](const MatchStats& a, const MatchStats& b) {
                  return a.timestamp < b.timestamp;
              });
}

void StatsManager::save_summary(const GlobalStats& summary) const {
    std::filesystem::path summary_file = stats_dir_ / "summary.txt";
    
    std::ofstream ofs(summary_file);
    ofs << "=== Match Statistics Summary ===\n";
    ofs << "total_matches=" << summary.total_matches << "\n";
    ofs << "total_wins=" << summary.total_wins << "\n";
    ofs << "total_losses=" << summary.total_losses << "\n";
    ofs << "average_duration=" << summary.average_match_duration_ticks << "\n";
    ofs.close();
}

}
