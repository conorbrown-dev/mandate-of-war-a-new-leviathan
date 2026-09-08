#include "test_framework.hpp"
#include "replay/replay_writer.hpp"
#include "replay/replay_reader.hpp"
#include "replay/types.hpp"
#include "network/types.hpp"
#include "network/serializer.hpp"
#include "network/portable_snapshot.hpp"
#include "simulation/simulation.hpp"
#include <cstring>

TEST(replay_writer_open_close) {
    using namespace rts;
    
    ReplayWriter writer;
    
    if (writer.is_open()) {
        throw std::runtime_error("Writer should not be open initially");
    }
    
    if (!writer.open("/tmp/test_replay.rts")) {
        throw std::runtime_error("Failed to open replay file");
    }
    
    if (!writer.is_open()) {
        throw std::runtime_error("Writer should be open after open()");
    }
    
    writer.close();
    
    if (writer.is_open()) {
        throw std::runtime_error("Writer should not be open after close()");
    }
}

TEST(replay_writer_header) {
    using namespace rts;
    
    ReplayWriter writer;
    if (!writer.open("/tmp/test_replay2.rts")) {
        throw std::runtime_error("Failed to open replay file");
    }
    
    ReplayMetadata metadata;
    metadata.map_name = "test_map";
    metadata.timestamp = 1234567890;
    metadata.player_ids = {0, 1};
    
    if (!writer.write_header(metadata)) {
        throw std::runtime_error("Failed to write header");
    }
    
    writer.close();
}

TEST(replay_writer_read_roundtrip) {
    using namespace rts;
    
    ReplayWriter writer;
    if (!writer.open("/tmp/test_replay3.rts")) {
        throw std::runtime_error("Failed to open replay file");
    }
    
    ReplayMetadata metadata;
    metadata.map_name = "roundtrip_test";
    metadata.timestamp = 9876543210;
    metadata.player_ids = {0};
    
    if (!writer.write_header(metadata)) {
        throw std::runtime_error("Failed to write header");
    }
    
    InputCommand cmd;
    cmd.tick_id = 100;
    cmd.player_id = 0;
    cmd.cmd_type = static_cast<uint8_t>(CommandType::MOVE);
    cmd.target_x = 123;
    cmd.target_y = 456;
    cmd.extra = 0;
    
    uint8_t cmd_buffer[20];
    serialize_input_command(cmd, cmd_buffer, sizeof(cmd_buffer));
    writer.write_command(cmd_buffer, sizeof(cmd_buffer));
    
    writer.close();
    
    ReplayReader reader;
    if (!reader.open("/tmp/test_replay3.rts")) {
        throw std::runtime_error("Failed to open replay for reading");
    }
    
    ReplayMetadata read_metadata;
    uint32_t stored_crc;
    if (!reader.read_header(read_metadata, stored_crc)) {
        throw std::runtime_error("Failed to read header");
    }
    
    if (read_metadata.map_name != "roundtrip_test") {
        throw std::runtime_error("Map name mismatch");
    }
    if (read_metadata.timestamp != 9876543210ULL) {
        throw std::runtime_error("Timestamp mismatch");
    }
    if (read_metadata.player_ids.size() != 1 || read_metadata.player_ids[0] != 0) {
        throw std::runtime_error("Player IDs mismatch");
    }
    
    std::vector<uint8_t> cmd_data;
    if (!reader.read_command(cmd_data)) {
        throw std::runtime_error("Failed to read command");
    }
    
    if (cmd_data.size() != 20) {
        throw std::runtime_error("Command size should be 20 bytes");
    }
    
    InputCommand decoded;
    deserialize_input_command(cmd_data.data(), cmd_data.size(), decoded);
    
    if (decoded.tick_id != 100) {
        throw std::runtime_error("Decoded tick_id mismatch");
    }
    if (decoded.target_x != 123) {
        throw std::runtime_error("Decoded target_x mismatch");
    }
    if (decoded.target_y != 456) {
        throw std::runtime_error("Decoded target_y mismatch");
    }
    
    reader.close();
}

TEST(replay_writer_portable_snapshot) {
    using namespace rts;
    
    // Create simulation with some units
    Simulation sim;
    sim.start();
    
    for (int i = 0; i < 10; ++i) {
        sim.create_unit(static_cast<float>(i * 10), 50.0f);
    }
    
    // Advance simulation to generate some state
    for (int frame = 0; frame < 5; ++frame) {
        sim.update(50.0f);
    }
    
    // Write portable snapshot to replay using sim's writer
    const char* replay_path = "/tmp/test_replay_portable.rts";
    
    if (!sim.start_replay(replay_path)) {
        throw std::runtime_error("Failed to open replay file");
    }
    
    ReplayMetadata metadata;
    metadata.map_name = "replay_portable_test";
    metadata.timestamp = 1234567890ULL;
    metadata.player_ids = {0};
    
    if (!sim.write_replay_header(metadata)) {
        throw std::runtime_error("Failed to write replay header");
    }
    
    sim.write_replay_portable_snapshot();
    
    sim.close_replay();
    
    // Verify file was created and has content
    std::ifstream file(replay_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Replay file not created");
    }
    
    auto size = file.tellg();
    file.close();
    
    if (size <= 0) {
        throw std::runtime_error("Replay file has no content");
    }
    
    // Verify the portable snapshot format by reading it back
    ReplayReader reader;
    if (!reader.open(replay_path)) {
        throw std::runtime_error("Failed to open replay for reading");
    }
    
    ReplayMetadata read_metadata;
    uint32_t stored_crc;
    if (!reader.read_header(read_metadata, stored_crc)) {
        throw std::runtime_error("Failed to read replay header");
    }
    
    // Read at least one snapshot
    std::vector<uint8_t> snapshot_data;
    bool has_snapshot = reader.read_snapshot(snapshot_data);
    
    if (!has_snapshot) {
        throw std::runtime_error("No snapshot found in replay");
    }
    
    if (snapshot_data.size() < 32) {
        throw std::runtime_error("Snapshot too small (should be >= 32 bytes for header)");
    }
    
    // Verify portable snapshot header
    uint32_t magic = 0;
    uint8_t version = 0;
    std::memcpy(&magic, snapshot_data.data(), sizeof(magic));
    std::memcpy(&version, snapshot_data.data() + 4, sizeof(version));
    
    if (magic != 0x53535452) {
        throw std::runtime_error("Invalid portable snapshot magic number");
    }
    
    if (version != 1) {
        throw std::runtime_error("Invalid portable snapshot version");
    }
    
    reader.close();
}

