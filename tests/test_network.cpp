#include "test_framework.hpp"
#include "network/types.hpp"
#include "network/serializer.hpp"
#include "network/buffer.hpp"
#include "simulation/command_manager.hpp"

#include <cmath>
#include <cstring>
#include <limits>

TEST(input_command_serialization) {
    using namespace rts;
    
    InputCommand cmd{};
    cmd.tick_id = 12345;
    cmd.entity_id = 42;
    cmd.player_id = 1;
    cmd.cmd_type = static_cast<uint8_t>(CommandType::MOVE);
    if (!encode_input_command_position(-105.25f, cmd.target_x) ||
        !encode_input_command_position(47.5f, cmd.target_y)) {
        throw std::runtime_error("Test command coordinates should encode");
    }
    cmd.extra = 0x12345678U;
    
    uint8_t buffer[64];
    size_t serialized = serialize_input_command(cmd, buffer, sizeof(buffer));
    
    if (serialized != sizeof(InputCommand)) {
        throw std::runtime_error("Serialization should produce exactly sizeof(InputCommand) bytes");
    }
    
    InputCommand decoded;
    size_t deserialized = deserialize_input_command(buffer, sizeof(buffer), decoded);
    
    if (deserialized != sizeof(InputCommand)) {
        throw std::runtime_error("Deserialization should read exactly sizeof(InputCommand) bytes");
    }
    
    if (decoded.tick_id != cmd.tick_id) {
        throw std::runtime_error("Decoded tick_id should match original");
    }
    if (decoded.player_id != cmd.player_id) {
        throw std::runtime_error("Decoded player_id should match original");
    }
    if (decoded.entity_id != cmd.entity_id) {
        throw std::runtime_error("Decoded entity_id should match original");
    }
    if (decoded.cmd_type != cmd.cmd_type) {
        throw std::runtime_error("Decoded cmd_type should match original");
    }
    if (decoded.target_x != cmd.target_x) {
        throw std::runtime_error("Decoded target_x should match original");
    }
    if (decoded.target_y != cmd.target_y) {
        throw std::runtime_error("Decoded target_y should match original");
    }
    if (decoded.extra != cmd.extra) {
        throw std::runtime_error("Decoded extra should match original");
    }
    if (buffer[18] != 0 || buffer[19] != 0) {
        throw std::runtime_error("Reserved wire bytes must be deterministically initialized");
    }
}

TEST(connection_handshake_visual_pack_round_trip) {
    using namespace rts;

    ConnectionHandshake handshake{};
    handshake.protocol_version = NETWORK_PROTOCOL_VERSION;
    handshake.client_version = 7;
    handshake.content_version_major = 1;
    handshake.content_version_minor = 2;
    handshake.content_version_patch = 3;
    handshake.visual_pack_version = 4;
    std::memcpy(handshake.visual_pack_id, "mandate_of_war.prototype_visuals", 32);
    for (size_t i = 0; i < VISUAL_PACK_HASH_LENGTH; ++i) {
        handshake.visual_pack_hash[i] = static_cast<uint8_t>(i);
    }

    uint8_t buffer[sizeof(ConnectionHandshake)]{};
    if (serialize_connection_handshake(handshake, buffer, sizeof(buffer)) != sizeof(ConnectionHandshake)) {
        throw std::runtime_error("Visual-pack handshake should serialize at its fixed wire size");
    }
    ConnectionHandshake decoded{};
    if (deserialize_connection_handshake(buffer, sizeof(buffer), decoded) != sizeof(ConnectionHandshake) ||
        decoded.visual_pack_version != handshake.visual_pack_version ||
        std::memcmp(decoded.visual_pack_id, handshake.visual_pack_id, VISUAL_PACK_ID_LENGTH) != 0 ||
        std::memcmp(decoded.visual_pack_hash, handshake.visual_pack_hash, VISUAL_PACK_HASH_LENGTH) != 0) {
        throw std::runtime_error("Visual-pack handshake fields must round-trip exactly");
    }
}

TEST(connection_handshake_visual_pack_compatibility_rejects_mismatch) {
    using namespace rts;

    ConnectionHandshake local{};
    ConnectionHandshake remote{};
    local.visual_pack_version = remote.visual_pack_version = 1;
    std::memcpy(local.visual_pack_id, "pack", 4);
    std::memcpy(remote.visual_pack_id, "pack", 4);
    std::memset(local.visual_pack_hash, 0xAB, VISUAL_PACK_HASH_LENGTH);
    std::memcpy(remote.visual_pack_hash, local.visual_pack_hash, VISUAL_PACK_HASH_LENGTH);
    if (!connection_handshake_visual_pack_compatible(local, remote)) {
        throw std::runtime_error("Matching visual-pack identities should be accepted");
    }
    remote.visual_pack_version++;
    if (connection_handshake_visual_pack_compatible(local, remote)) {
        throw std::runtime_error("Mismatched visual-pack versions must be rejected");
    }
    remote.visual_pack_version = local.visual_pack_version;
    remote.visual_pack_hash[0]++;
    if (connection_handshake_visual_pack_compatible(local, remote)) {
        throw std::runtime_error("Mismatched visual-pack hashes must be rejected");
    }
    std::memset(remote.visual_pack_hash, 0, VISUAL_PACK_HASH_LENGTH);
    if (connection_handshake_visual_pack_compatible(local, remote)) {
        throw std::runtime_error("Empty visual-pack hashes must be rejected");
    }
}

TEST(input_command_signed_position_codec) {
    using namespace rts;

    int16_t encoded_x = 0;
    int16_t encoded_y = 0;
    if (!encode_input_command_position(-105.25f, encoded_x) ||
        !encode_input_command_position(47.5f, encoded_y)) {
        throw std::runtime_error("Valid signed world coordinates should encode");
    }
    if (std::abs(decode_input_command_position(encoded_x) + 105.25f) > 0.001f ||
        std::abs(decode_input_command_position(encoded_y) - 47.5f) > 0.001f) {
        throw std::runtime_error("Signed command coordinates should round-trip at centiunit precision");
    }
    if (encode_input_command_position(std::numeric_limits<float>::quiet_NaN(), encoded_x) ||
        encode_input_command_position(400.0f, encoded_x)) {
        throw std::runtime_error("Non-finite or unrepresentable command coordinates must be rejected");
    }
}

TEST(command_manager_capacity_is_fail_closed) {
    using namespace rts;

    CommandManager manager(1);
    InputCommand first{};
    InputCommand second{};
    if (!manager.inject_local_command(first) || manager.inject_local_command(second) ||
        manager.local_command_count() != 1) {
        throw std::runtime_error("Command queue must reject overflow without partial mutation");
    }
}

TEST(snapshot_deserialization_rejects_oversized_counts) {
    using namespace rts;

    SnapshotHeader header{1, MAX_SNAPSHOT_ENTITIES + 1};
    uint8_t buffer[sizeof(SnapshotHeader)];
    std::memcpy(buffer, &header, sizeof(header));
    Snapshot snapshot{};

    if (deserialize_snapshot(buffer, sizeof(buffer), snapshot) != 0) {
        throw std::runtime_error("Oversized snapshot entity counts must be rejected");
    }
}

TEST(delta_snapshot_deserialization_rejects_oversized_counts) {
    using namespace rts;

    uint32_t header[4] = {1, 1, 0, MAX_SNAPSHOT_ENTITIES + 1};
    DeltaSnapshot snapshot{};

    if (deserialize_delta_snapshot(reinterpret_cast<const uint8_t*>(header), sizeof(header), snapshot) != 0) {
        throw std::runtime_error("Oversized delta snapshot entity counts must be rejected");
    }
}

TEST(buffer_basic_operations) {
    using namespace rts;
    
    InputBuffer buffer(10);
    
    if (!buffer.empty()) {
        throw std::runtime_error("New buffer should be empty");
    }
    if (buffer.size() != 0) {
        throw std::runtime_error("New buffer should have size 0");
    }
    
    InputCommand cmd1, cmd2;
    cmd1.tick_id = 1;
    cmd2.tick_id = 2;
    
    buffer.push(cmd1);
    buffer.push(cmd2);
    
    if (buffer.size() != 2) {
        throw std::runtime_error("Buffer should have size 2 after 2 pushes");
    }
    
    InputCommand out;
    if (!buffer.pop(out)) {
        throw std::runtime_error("Should be able to pop from non-empty buffer");
    }
    if (out.tick_id != 1) {
        throw std::runtime_error("First pop should return first command");
    }
    
    if (buffer.size() != 1) {
        throw std::runtime_error("Buffer should have size 1 after 1 pop");
    }
    
    buffer.pop(out);
    if (!buffer.empty()) {
        throw std::runtime_error("Buffer should be empty after popping all items");
    }
    
    if (buffer.pop(out)) {
        throw std::runtime_error("Pop from empty buffer should return false");
    }
}

TEST(buffer_fifo_order) {
    using namespace rts;
    
    InputBuffer buffer(100);
    
    for (int i = 0; i < 50; ++i) {
        InputCommand cmd;
        cmd.tick_id = i;
        buffer.push(cmd);
    }
    
    for (int i = 0; i < 50; ++i) {
        InputCommand cmd;
        if (!buffer.pop(cmd)) {
            throw std::runtime_error("Failed to pop command " + std::to_string(i));
        }
        if (cmd.tick_id != i) {
            throw std::runtime_error("FIFO order violated at index " + std::to_string(i));
        }
    }
}

TEST(buffer_wraparound) {
    using namespace rts;
    
    InputBuffer buffer(5);
    
    for (int i = 0; i < 10; ++i) {
        InputCommand cmd;
        cmd.tick_id = i;
        buffer.push(cmd);
    }
    
    if (buffer.size() != 4) {
        throw std::runtime_error("Buffer should wrap and keep last 4 items");
    }
    
    for (int i = 6; i < 10; ++i) {
        InputCommand cmd;
        if (!buffer.pop(cmd)) {
            throw std::runtime_error("Failed to pop wrapped command " + std::to_string(i));
        }
        if (cmd.tick_id != i) {
            throw std::runtime_error("Wrapped command has wrong tick_id");
        }
    }
}

TEST(snapshot_buffer_clear_and_reuse) {
    using namespace rts;

    SnapshotBuffer buffer(2);
    Snapshot first{};
    first.tick = 1;
    buffer.store(first);
    buffer.clear();

    Snapshot out{};
    if (buffer.get(1, out) || buffer.size() != 0) {
        throw std::runtime_error("Clearing snapshot history must discard stored snapshots");
    }

    Snapshot second{};
    second.tick = 2;
    buffer.store(second);
    if (!buffer.get(2, out) || buffer.size() != 1) {
        throw std::runtime_error("Snapshot history must remain reusable after reset");
    }
}
