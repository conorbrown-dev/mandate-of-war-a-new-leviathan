#pragma once

#include <cstdint>
#include <array>
#include <cmath>
#include <limits>
#include <cstring>

#include "../ecs/entity.hpp"
#include "../spatial/spatial_grid.hpp"

namespace rts {

// Version 2 assigns signed centiunit semantics to InputCommand target coordinates.
constexpr uint32_t NETWORK_PROTOCOL_VERSION = 3;
constexpr uint32_t MAX_COMMANDS_PER_TICK = 256;
constexpr uint32_t SNAPSHOT_HISTORY_SIZE = 32;
constexpr uint32_t MAX_SNAPSHOT_ENTITIES = 1024;

enum class MessageType : uint8_t {
    COMMAND = 0,
    SNAPSHOT = 1,
    ACK = 2,
    SYNC_REQUEST = 3,
    SYNC_RESPONSE = 4
};

enum class DiscoveryType : uint8_t {
    BROADCAST = 0,
    JOIN_REQUEST = 1,
    JOIN_RESPONSE = 2
};

enum class TransportType : uint8_t {
    HANDSHAKE = 0,
    FRAME_COMMAND_BATCH = 1,
    SNAPSHOT_CHECKSUM = 2,
    DESYNC_REPORT = 3
};

constexpr size_t MAX_SERVER_NAME_LENGTH = 64;
constexpr size_t MAP_HASH_LENGTH = 64;
constexpr size_t VISUAL_PACK_ID_LENGTH = 64;
constexpr size_t VISUAL_PACK_HASH_LENGTH = 64;
constexpr uint32_t DISCOVERY_PORT = 50000;
constexpr uint32_t DISCOVERY_INTERVAL_MS = 500;
constexpr size_t MAX_TRANSPORT_PACKET_SIZE = 65536;
constexpr uint32_t TCP_PORT = 50001;

enum class CommandType : uint8_t {
    MOVE = 0,
    ATTACK = 1,
    STOP = 2,
    BUILD = 3,
    HARVEST = 4,
    RETURN = 5,
    DEFEND = 6,
    PATROL = 7,
    RESEARCH = 8,
    INSTALL = 9,
    REQUISITION = 10
};

constexpr size_t INPUT_COMMAND_WIRE_SIZE = 20;
constexpr float INPUT_COMMAND_POSITION_SCALE = 100.0f;

inline bool encode_input_command_position(float value, int16_t& encoded) {
    if (!std::isfinite(value)) {
        return false;
    }

    const float scaled = std::round(value * INPUT_COMMAND_POSITION_SCALE);
    if (scaled < static_cast<float>(std::numeric_limits<int16_t>::min()) ||
        scaled > static_cast<float>(std::numeric_limits<int16_t>::max())) {
        return false;
    }

    encoded = static_cast<int16_t>(scaled);
    return true;
}

inline float decode_input_command_position(int16_t encoded) {
    return static_cast<float>(encoded) / INPUT_COMMAND_POSITION_SCALE;
}

struct alignas(4) InputCommand {
    uint32_t tick_id;
    EntityId entity_id;
    uint8_t player_id;
    uint8_t cmd_type;
    int16_t target_x;
    int16_t target_y;
    uint32_t extra;
};

struct PositionDelta {
    int16_t dx;
    int16_t dy;
};

struct VelocityDelta {
    int16_t dvx;
    int16_t dvy;
};

struct HealthDelta {
    int8_t dh;
};

struct DeltaEntitySnapshot {
    uint16_t entity_id;
    uint16_t flags;
    PositionDelta position_delta;
    VelocityDelta velocity_delta;
    HealthDelta health_delta;
};

struct DeltaSnapshot {
    uint32_t tick;
    uint32_t sequence_id;
    uint32_t reference_tick;
    uint32_t entity_count;
    DeltaEntitySnapshot entities[MAX_SNAPSHOT_ENTITIES];
};

static_assert(sizeof(InputCommand) == 20, "InputCommand should be 20 bytes");
static_assert(sizeof(PositionDelta) == 4, "PositionDelta should be 4 bytes");
static_assert(sizeof(VelocityDelta) == 4, "VelocityDelta should be 4 bytes");
static_assert(sizeof(HealthDelta) == 1, "HealthDelta should be 1 byte");
static_assert(sizeof(DeltaEntitySnapshot) <= 24, "DeltaEntitySnapshot should fit in 24 bytes");
static_assert(sizeof(DeltaSnapshot) <= 24584, "DeltaSnapshot should fit in reasonable buffer");

struct ConnectionHandshake {
    uint32_t protocol_version;
    uint32_t client_version;
    uint8_t map_hash[MAP_HASH_LENGTH];
    uint32_t content_version_major;
    uint32_t content_version_minor;
    uint32_t content_version_patch;
    uint8_t visual_pack_id[VISUAL_PACK_ID_LENGTH];
    uint32_t visual_pack_version;
    uint8_t visual_pack_hash[VISUAL_PACK_HASH_LENGTH];
    uint8_t padding[4];
};

inline bool connection_handshake_visual_pack_compatible(
    const ConnectionHandshake& local,
    const ConnectionHandshake& remote) {
    if (local.visual_pack_version != remote.visual_pack_version ||
        std::memcmp(local.visual_pack_id, remote.visual_pack_id, VISUAL_PACK_ID_LENGTH) != 0 ||
        std::memcmp(local.visual_pack_hash, remote.visual_pack_hash, VISUAL_PACK_HASH_LENGTH) != 0) {
        return false;
    }
    bool has_id = false;
    bool has_hash = false;
    for (size_t i = 0; i < VISUAL_PACK_ID_LENGTH; ++i) {
        has_id = has_id || local.visual_pack_id[i] != 0;
    }
    for (size_t i = 0; i < VISUAL_PACK_HASH_LENGTH; ++i) {
        has_hash = has_hash || local.visual_pack_hash[i] != 0;
    }
    return has_id && has_hash;
}

struct FrameCommandBatch {
    uint32_t tick;
    uint16_t command_count;
    uint16_t padding;
    InputCommand commands[MAX_COMMANDS_PER_TICK];
};

struct alignas(8) SnapshotChecksum {
    uint32_t tick;
    uint64_t snapshot_hash;
    uint8_t padding[8];
};

static_assert(sizeof(ConnectionHandshake) == 220, "ConnectionHandshake should be 220 bytes");
static_assert(sizeof(FrameCommandBatch) == 5128, "FrameCommandBatch should be 5128 bytes");
static_assert(sizeof(SnapshotChecksum) == 24, "SnapshotChecksum should be 24 bytes");

struct EntitySnapshot {
    EntityId entity_id;
    Position position;
    Velocity velocity;
    Health health;
    uint8_t alive;
};

static_assert(sizeof(EntitySnapshot) <= 48, "EntitySnapshot should fit in 48 bytes");

struct SnapshotHeader {
    uint32_t tick;
    uint32_t entity_count;
};

struct Snapshot {
    uint32_t tick;
    uint32_t entity_count;
    EntitySnapshot entities[MAX_SNAPSHOT_ENTITIES];
};

static_assert(sizeof(SnapshotHeader) == 8, "SnapshotHeader should be 8 bytes");
static_assert(sizeof(Snapshot) <= 45064, "Snapshot should fit in reasonable buffer");

struct alignas(4) NetworkHeader {
    uint32_t protocol_version;
    MessageType type;
    uint32_t timestamp_ms;
    uint32_t sequence_id;
};

static_assert(sizeof(NetworkHeader) == 16, "NetworkHeader should be 16 bytes");

struct DiscoveryPacket {
    NetworkHeader header;
    DiscoveryType discovery_type;
    char server_name[MAX_SERVER_NAME_LENGTH];
    uint8_t map_hash[MAP_HASH_LENGTH];
    uint32_t player_count;
    uint32_t max_players;
    uint32_t version_major;
    uint32_t version_minor;
    uint32_t version_patch;
    uint8_t padding[4];
};

struct JoinRequest {
    NetworkHeader header;
    char player_name[MAX_SERVER_NAME_LENGTH];
    uint32_t version_major;
    uint32_t version_minor;
    uint32_t version_patch;
    uint8_t accepted_map_hash[MAP_HASH_LENGTH];
    uint8_t padding[4];
};

struct JoinResponse {
    NetworkHeader header;
    uint32_t slot_id;
    uint8_t accepted_map_hash[MAP_HASH_LENGTH];
    uint32_t tick;
    uint64_t snapshot_hash;
    uint8_t padding[8];
};

static_assert(sizeof(DiscoveryPacket) == 172, "DiscoveryPacket should be 172 bytes");
static_assert(sizeof(JoinRequest) == 160, "JoinRequest should be 160 bytes");
static_assert(sizeof(JoinResponse) == 104, "JoinResponse should be 104 bytes");

} // namespace rts
