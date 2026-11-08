#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "network/types.hpp"

namespace rts {

// One side's state at a sample point. Enough to study how a player plays: how
// big the army is, how rich they are and where their fighting units stand.
struct FactionSummary {
    int units = 0;
    int fighters = 0;
    float materials = 0.0f;
    float army_x = 0.0f;
    float army_y = 0.0f;
};

struct MatchSample {
    uint32_t tick = 0;
    std::array<FactionSummary, 3> factions{};
};

// A call from the presentation layer that changes the match without going
// through the command queue, such as queueing a structure or calling in a
// delivery. Kept so a match can be rebuilt in full later.
struct RecordedAction {
    uint32_t tick = 0;
    std::string name;
    std::vector<double> args;
};

// Everything needed to study a match and, with the same setup, to play it back:
// every order that ran, in the order it ran, plus regular state hashes that a
// playback must reproduce exactly.
class MatchRecord {
public:
    static constexpr int kFormatVersion = 1;
    static constexpr uint32_t kHashEveryTicks = 20;
    static constexpr uint32_t kSampleEveryTicks = 200;

    void clear();
    void begin(uint32_t tick, float command_position_scale);
    bool recording() const { return recording_; }
    bool started() const { return started_; }

    void set_info(const std::string& key, const std::string& value) { info_[key] = value; }
    void add_command(const InputCommand& command) { if (recording_) commands_.push_back(command); }
    void add_action(uint32_t tick, std::string name, std::vector<double> args);
    void add_hash(uint32_t tick, uint64_t hash) { hashes_.emplace_back(tick, hash); }
    void add_sample(const MatchSample& sample) { samples_.push_back(sample); }
    void note_first_contact(size_t faction, uint32_t tick);
    void finish(uint32_t tick, int result, float seconds);

    const std::vector<InputCommand>& commands() const { return commands_; }
    const std::vector<RecordedAction>& actions() const { return actions_; }
    const std::vector<std::pair<uint32_t, uint64_t>>& hashes() const { return hashes_; }
    const std::vector<MatchSample>& samples() const { return samples_; }
    const std::map<std::string, std::string>& info() const { return info_; }
    std::optional<uint32_t> first_contact(size_t faction) const { return first_contact_[faction]; }
    int result() const { return result_; }
    uint32_t start_tick() const { return start_tick_; }
    uint32_t end_tick() const { return end_tick_; }

    std::string to_json() const;
    // Reads the orders back out of a saved record, ready for playback.
    // Returns false and leaves the output empty when the record is unreadable.
    static bool commands_from_json(const std::string& json, std::vector<InputCommand>& commands);

private:
    bool started_ = false;
    bool recording_ = false;
    uint32_t start_tick_ = 0;
    uint32_t end_tick_ = 0;
    int result_ = -1;
    float seconds_ = 0.0f;
    float command_position_scale_ = 100.0f;
    std::map<std::string, std::string> info_;
    std::vector<InputCommand> commands_;
    std::vector<RecordedAction> actions_;
    std::vector<std::pair<uint32_t, uint64_t>> hashes_;
    std::vector<MatchSample> samples_;
    std::array<std::optional<uint32_t>, 3> first_contact_{};
};

} // namespace rts
