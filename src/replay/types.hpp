#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace rts {

constexpr uint32_t kReplayMagic = 0x30535452;
constexpr uint8_t kReplayVersion = 0x01;

struct ReplayMetadata {
    std::string map_name;
    uint64_t timestamp;
    std::vector<uint8_t> player_ids;
};

} // namespace rts