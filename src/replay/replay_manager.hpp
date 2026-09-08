#include "file_format.hpp"
#include "../network/serializer.hpp"
#include "../network/portable_snapshot.hpp"

#include <cstring>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace rts {

class ReplayRecorder {
public:
    ReplayRecorder();
    ~ReplayRecorder();
    
    bool start_recording(const std::string& path, const std::string& map_hash);
    bool record_command_batch(uint64_t tick, uint16_t player_id, const std::vector<InputCommand>& commands);
    bool record_snapshot_keyframe(uint64_t tick, const PortableSnapshot& snapshot);
    bool stop_recording();
    
private:
    ReplayFile file_;
    std::vector<uint8_t> command_buffer_;
    std::vector<uint8_t> rng_buffer_;
    std::vector<uint8_t> snapshot_buffer_;
    uint64_t min_tick_{0};
    uint64_t max_tick_{0};
    bool recording_{false};
};

class ReplayPlayer {
public:
    ReplayPlayer();
    ~ReplayPlayer();
    
    bool load_replay(const std::string& path);
    bool verify_map_hash(const std::string& expected_hash);
    bool initialize();
    bool next_frame(uint64_t& tick, std::vector<InputCommand>& commands);
    bool seek_to_tick(uint64_t target_tick);
    bool is_at_end() const;
    uint64_t get_min_tick() const { return header_.min_tick; }
    uint64_t get_max_tick() const { return header_.max_tick; }
    
private:
    ReplayFile file_;
    ReplayHeader header_;
    std::vector<uint8_t> command_data_;
    std::vector<uint8_t> current_batch_;
    size_t command_offset_{0};
    bool initialized_{false};
};

} // namespace rts
