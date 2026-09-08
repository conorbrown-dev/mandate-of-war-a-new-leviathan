#pragma once

#include <cstdint>
#include <string>
#include <fstream>
#include <vector>

namespace rts {

// Header (64 bytes)
struct ReplayHeader {
    uint32_t magic;           // 'RTSR' (0x52545352)
    uint16_t version;         // 0x0001
    uint16_t reserved;        // must be zero
    uint64_t min_tick;        // first tick in replay
    uint64_t max_tick;        // last tick in replay
    uint64_t command_offset;  // file offset to command data
    uint64_t command_length;  // bytes of command data
    uint64_t rng_offset;      // file offset to RNG states
    uint64_t rng_length;      // bytes of RNG data
    uint64_t snapshot_offset; // file offset to keyframes
    uint64_t snapshot_length; // bytes of snapshot data
    char map_hash[64];        // SHA-256 as lowercase hex
};

// File constants
constexpr uint32_t REPLAY_MAGIC = 0x52545352;
constexpr uint16_t REPLAY_VERSION = 0x0001;
constexpr size_t MAP_HASH_LENGTH = 64;  // SHA-256 hex string

class ReplayFile {
public:
    ReplayFile();
    ~ReplayFile();
    
    // Recording
    bool open_for_recording(const std::string& path);
    bool write_header(const ReplayHeader& header);
    bool write_commands(const uint8_t* data, size_t size);
    bool write_rng_states(const uint8_t* data, size_t size);
    bool write_snapshots(const uint8_t* data, size_t size);
    bool close();
    
    // Playback
    bool open_for_playback(const std::string& path);
    bool read_header(ReplayHeader& header);
    bool read_commands(std::vector<uint8_t>& data);
    bool read_rng_states(std::vector<uint8_t>& data);
    bool read_snapshots(std::vector<uint8_t>& data);
    
    // Validation
    bool is_valid() const;
    bool verify_map_hash(const std::string& map_hash);
    
    // Position
    uint64_t tell() const { return pos_; }
    bool seek(uint64_t offset) { file_.seekg(static_cast<std::streamoff>(offset), std::ios::beg); pos_ = static_cast<uint64_t>(file_.tellg()); return file_.good(); }
    
private:
    std::fstream file_;
    bool recording_{false};
    mutable uint64_t pos_{0};
    
    bool validate_header(const ReplayHeader& header);
};

} // namespace rts
