#include "test_framework.hpp"
#include "network/types.hpp"
#include "network/serializer.hpp"
#include "network/buffer.hpp"
#include "network/network_manager.hpp"
#include "simulation/skirmish.hpp"
#include "simulation/command_manager.hpp"

#include <cmath>
#include <cstring>
#include <limits>
#include <thread>
#include <filesystem>

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

TEST(network_loopback_frame_batch_round_trip) {
    using namespace rts;
    constexpr uint16_t port = 51234;
    NetworkManager server;
    NetworkManager client;
    if (!server.listen(port)) { std::cout << "Loopback TCP unavailable in this environment; external transport proof required\n"; return; }
    bool connected = false;
    std::thread connector([&] { connected = client.connect("127.0.0.1", port); });
    if (!server.accept()) {
        connector.join();
        throw std::runtime_error("Loopback server failed to accept");
    }
    connector.join();
    if (!connected) throw std::runtime_error("Loopback client failed to connect");
    FrameCommandBatch sent{};
    sent.tick = 4;
    sent.command_count = 1;
    sent.commands[0].tick_id = 4;
    sent.commands[0].entity_id = 7;
    sent.commands[0].player_id = 1;
    sent.commands[0].cmd_type = static_cast<uint8_t>(CommandType::MOVE);
    client.send_frame_command_batch(sent);
    FrameCommandBatch received{};
    if (!server.receive_frame_command_batch(received) || received.tick != 4 || received.command_count != 1 ||
        received.commands[0].entity_id != 7 || received.commands[0].cmd_type != sent.commands[0].cmd_type) {
        throw std::runtime_error("Loopback command batch did not round-trip");
    }
}

TEST(network_loopback_batch_drives_identical_local_match_tick) {
    using namespace rts;
    Simulation client_simulation, server_simulation;
    Skirmish client_match(client_simulation), server_match(server_simulation);
    const auto scenario = (std::filesystem::current_path() / "godot/project/scenarios/two_landmass_skirmish.json").string();
    if (!client_match.load(scenario) || !server_match.load(scenario)) throw std::runtime_error("Local matches failed to load");
    constexpr uint16_t port = 51235;
    auto& server = server_simulation.network_manager();
    auto& client = client_simulation.network_manager();
    if (!server.listen(port)) { std::cout << "Loopback TCP unavailable in this environment; external match proof required\n"; return; }
    bool connected = false;
    std::thread connector([&] { connected = client.connect("127.0.0.1", port); });
    if (!server.accept()) { connector.join(); throw std::runtime_error("Match server failed to accept"); }
    connector.join();
    if (!connected) throw std::runtime_error("Match client failed to connect");
    InputCommand command{1, 2, 0, static_cast<uint8_t>(CommandType::MOVE), -10000, 0, 0};
    const float initial_x = client_simulation.get_unit_x(command.entity_id);
    if (!client_simulation.command_manager().inject_local_command(command)) throw std::runtime_error("Client could not queue command");
    client_match.update(50); server_match.update(50);
    if (client_simulation.command_log().size() != 1 || server_simulation.command_log().size() != 1 ||
        client_simulation.command_log().front().entity_id != command.entity_id ||
        server_simulation.command_log().front().entity_id != command.entity_id) {
        throw std::runtime_error("Networked command was not recorded by both local matches");
    }
    const auto a = client_simulation.get_state(), b = server_simulation.get_state();
    if (a.entity_ids != b.entity_ids || a.positions_x != b.positions_x || a.positions_y != b.positions_y ||
        a.health_current != b.health_current || client_match.checksums() != server_match.checksums()) {
        throw std::runtime_error("Networked local matches diverged after the exchanged command");
    }
    if (std::abs(client_simulation.get_unit_x(command.entity_id) - initial_x) < 0.0001f ||
        std::abs(server_simulation.get_unit_x(command.entity_id) - initial_x) < 0.0001f) {
        throw std::runtime_error("Networked MOVE command did not alter both authoritative matches");
    }
    client.reset();
    server.reset();
    while (client_match.result() == -1) client_match.update(50);
    const auto replay_path = (std::filesystem::temp_directory_path() / "g06-networked-match.replay").string();
    if (!client_match.save_replay(replay_path) || !client_match.replay(replay_path)) {
        throw std::runtime_error("Replay did not reproduce the networked local match: " + client_match.error());
    }
    std::filesystem::remove(replay_path);
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
