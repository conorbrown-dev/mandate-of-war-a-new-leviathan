#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <vector>

namespace rts {

constexpr size_t PORTABLE_SNAPSHOT_HEADER_SIZE = 32;
constexpr size_t PORTABLE_SNAPSHOT_ENTITY_SIZE = 40;
constexpr uint32_t PORTABLE_SNAPSHOT_MAGIC = 0x53535452;
constexpr uint16_t PORTABLE_SNAPSHOT_VERSION = 1;
constexpr uint8_t PORTABLE_SNAPSHOT_MESSAGE_KIND_FULL = 1;
constexpr size_t PORTABLE_SNAPSHOT_MAX_ENTITIES = 100000;
constexpr size_t PORTABLE_SNAPSHOT_MAX_SIZE = 32 + 100000 * 40;

static_assert(sizeof(float) == 4, "Portable snapshot requires 4-byte floats");
static_assert(std::numeric_limits<float>::is_iec559, "Portable snapshot requires IEEE-754 float");

enum class SnapshotError {
    OK,
    BUFFER_TOO_SHORT,
    INVALID_MAGIC,
    INVALID_VERSION,
    INVALID_MESSAGE_KIND,
    INVALID_FLAGS,
    INVALID_HEADER_SIZE,
    INVALID_RECORD_SIZE,
    INVALID_TOTAL_BYTES,
    INVALID_ENTITY_COUNT,
    INVALID_RESERVED,
    INVALID_ENTITY_ID,
    INVALID_COMPONENT_MASK,
    INVALID_STATE_FLAGS,
    INVALID_SUBTICK_MS,
    INVALID_HEALTH_RANGE,
    NONFINITE_FLOAT,
};

struct PortableSnapshot {
    uint32_t tick;
    float subtick_ms;
    uint32_t entity_count;
    std::vector<uint32_t> entities;
    std::vector<float> positions_x;
    std::vector<float> positions_y;
    std::vector<float> positions_z;
    std::vector<float> velocities_x;
    std::vector<float> velocities_y;
    std::vector<float> velocities_z;
    std::vector<float> health_current;
    std::vector<float> health_max;
    std::vector<uint8_t> is_dead;
};

SnapshotError encode_portable_snapshot(const uint32_t* entity_ids, const float* pos_x, const float* pos_y, const float* pos_z,
                                        const float* vel_x, const float* vel_y, const float* vel_z,
                                        const float* health_curr, const float* health_max, const uint8_t* is_dead,
                                        size_t count, uint32_t tick, uint8_t* buffer, size_t buffer_size, size_t* out_encoded_size);

SnapshotError decode_portable_snapshot(const uint8_t* buffer, size_t buffer_size, PortableSnapshot* out_snapshot);

SnapshotError validate_float(float value);
   SnapshotError validate_subtick_ms(float value);
    SnapshotError validate_health(float current, float max);

} // namespace rts
