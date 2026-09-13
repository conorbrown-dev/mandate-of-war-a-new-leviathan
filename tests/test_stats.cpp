#include "test_framework.hpp"
#include "stats/stats_manager.hpp"
#include <filesystem>

TEST(stats_persist_and_summarize_match_history) {
    const auto directory = std::filesystem::temp_directory_path() / "rts-goal06-stats";
    std::filesystem::remove_all(directory);
    rts::StatsManager stats(directory);
    stats.record_match({"broken_strait", 100, "player-a", "elite", true, 400, 12, 3, 7, 90, 80, 10, 9});
    stats.record_match({"broken_strait", 101, "player-b", "mass", false, 600, 8, 7, 3, 40, 30, 5, 8});
    const auto summary = stats.get_summary();
    if (summary.total_matches != 2 || summary.total_wins != 1 || summary.total_losses != 1 ||
        summary.average_match_duration_ticks != 500.0 || summary.map_stats.size() != 1 ||
        summary.map_stats.front().matches != 2) {
        throw std::runtime_error("Persisted match history summary is incorrect");
    }
    if (!std::filesystem::is_regular_file(directory / "match_100.txt") ||
        !std::filesystem::is_regular_file(directory / "summary.txt")) {
        throw std::runtime_error("Stats persistence files were not written");
    }
}
