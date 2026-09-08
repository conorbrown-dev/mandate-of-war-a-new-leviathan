#include "test_framework.hpp"
#include "network/portable_snapshot.hpp"

#include <cstring>
#include <vector>
#include <limits>
#include <memory>

#include <iostream>

int main() {
    using namespace rts::test;
    
    try {
        test_runner.run_all();
        std::cout << "All portable_snapshot tests passed" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}

TEST(portable_snapshot_header_constants) {
    using namespace rts;
    
    if (PORTABLE_SNAPSHOT_HEADER_SIZE != 32) {
        throw std::runtime_error("Header size must be 32 bytes");
    }
    if (PORTABLE_SNAPSHOT_ENTITY_SIZE != 40) {
        throw std::runtime_error("Entity record size must be 40 bytes");
    }
    if (PORTABLE_SNAPSHOT_MAGIC != 0x53535452) {
        throw std::runtime_error("Magic must be 'RTSS'");
    }
    if (PORTABLE_SNAPSHOT_VERSION != 1) {
        throw std::runtime_error("Version must be 1");
    }
    if (PORTABLE_SNAPSHOT_MESSAGE_KIND_FULL != 1) {
        throw std::runtime_error("Message kind must be 1 for full snapshot");
    }
    if (PORTABLE_SNAPSHOT_MAX_ENTITIES != 100000) {
        throw std::runtime_error("Max entities must be 100000");
    }
}

TEST(portable_snapshot_encode_empty) {
    using namespace rts;
    
    uint8_t buffer[1024];
    size_t encoded_size = 0;
    
    SnapshotError err = encode_portable_snapshot(nullptr, nullptr, nullptr, nullptr,
                                                   nullptr, nullptr, nullptr,
                                                   nullptr, nullptr, nullptr,
                                                   0, 0, buffer, sizeof(buffer), &encoded_size);
    
    if (err != SnapshotError::OK) {
        throw std::runtime_error("Empty snapshot encode must succeed");
    }
    if (encoded_size != 32) {
        throw std::runtime_error("Empty snapshot must be exactly 32 bytes");
    }
    if (buffer[0] != 'R' || buffer[1] != 'T' || buffer[2] != 'S' || buffer[3] != 'S') {
        throw std::runtime_error("Magic must be 'RTSS'");
    }
    if (buffer[4] != 1 || buffer[5] != 0) {
        throw std::runtime_error("Version must be 1 (little-endian)");
    }
    if (buffer[6] != 1) {
        throw std::runtime_error("Message kind must be 1");
    }
    if (buffer[7] != 0) {
        throw std::runtime_error("Flags must be 0");
    }
    if (buffer[8] != 32 || buffer[9] != 0) {
        throw std::runtime_error("Header bytes must be 32");
    }
    if (buffer[10] != 40 || buffer[11] != 0) {
        throw std::runtime_error("Record bytes must be 40");
    }
}

TEST(portable_snapshot_encode_one_entity) {
    using namespace rts;
    
    uint32_t entity_id = 42;
    float pos_x = 100.0f, pos_y = 200.0f, pos_z = 0.0f;
    float vel_x = 10.0f, vel_y = 20.0f, vel_z = 0.0f;
    float hp_curr = 100.0f, hp_max = 100.0f;
    uint8_t is_dead = 0;
    
    uint8_t buffer[256];
    size_t encoded_size = 0;
    
    SnapshotError err = encode_portable_snapshot(
&entity_id, &pos_x, &pos_y, &pos_z,
                                                  &vel_x, &vel_y, &vel_z,
                                                  &hp_curr, &hp_max, &is_dead,
                                                  1, 0, buffer, sizeof(buffer), &encoded_size);
    
    if (err != SnapshotError::OK) {
        throw std::runtime_error("Single entity encode must succeed");
    }
    if (encoded_size != 72) {
        throw std::runtime_error("Single entity snapshot must be 72 bytes (32 + 40)");
    }
    
    PortableSnapshot snapshot{};
    SnapshotError dec_err = decode_portable_snapshot(buffer, encoded_size, &snapshot);
    if (dec_err != SnapshotError::OK) {
        throw std::runtime_error("Decode must succeed");
    }
    if (snapshot.tick != 0 || snapshot.subtick_ms != 0.0f) {
        throw std::runtime_error("Default tick and subtick_ms must be 0");
    }
    if (snapshot.entity_count != 1) {
        throw std::runtime_error("Entity count must be 1");
    }
    if (snapshot.entities[0] != 42) {
        throw std::runtime_error("Entity ID must match");
    }
}

TEST(portable_snapshot_rejects_oversized_entities) {
    using namespace rts;
    
    uint8_t buffer[1024];
    size_t encoded_size = 0;
    
    constexpr size_t TOO_MANY = PORTABLE_SNAPSHOT_MAX_ENTITIES + 1;
    std::vector<uint32_t> entity_ids(TOO_MANY, 1);
    std::vector<float> pos_x(TOO_MANY, 0.0f), pos_y(TOO_MANY, 0.0f), pos_z(TOO_MANY, 0.0f);
    std::vector<float> vel_x(TOO_MANY, 0.0f), vel_y(TOO_MANY, 0.0f), vel_z(TOO_MANY, 0.0f);
    std::vector<float> hp_curr(TOO_MANY, 100.0f), hp_max(TOO_MANY, 100.0f);
    std::vector<uint8_t> is_dead(TOO_MANY, 0);
    
     SnapshotError err = encode_portable_snapshot(entity_ids.data(), pos_x.data(), pos_y.data(), pos_z.data(),
         vel_x.data(), vel_y.data(), vel_z.data(),
         hp_curr.data(), hp_max.data(), is_dead.data(),
         TOO_MANY, 0, buffer, sizeof(buffer), &encoded_size);
    
    if (err != SnapshotError::INVALID_ENTITY_COUNT) {
        throw std::runtime_error("Must reject entity count > 100000");
    }
}

TEST(portable_snapshot_rejects_trailing_bytes) {
    using namespace rts;
    
    uint32_t entity_id = 1;
    float pos_x = 0.0f, pos_y = 0.0f, pos_z = 0.0f;
    float vel_x = 0.0f, vel_y = 0.0f, vel_z = 0.0f;
    float hp_curr = 100.0f, hp_max = 100.0f;
    uint8_t is_dead = 0;
    
    uint8_t buffer[256];
    size_t encoded_size = 0;
    
    SnapshotError enc_err = encode_portable_snapshot(
&entity_id, &pos_x, &pos_y, &pos_z,
                                                      &vel_x, &vel_y, &vel_z,
                                                      &hp_curr, &hp_max, &is_dead,
                                                      1, 0, buffer, sizeof(buffer), &encoded_size);
    if (enc_err != SnapshotError::OK) {
        throw std::runtime_error("Encode must succeed");
    }
    
    PortableSnapshot snapshot{};
    SnapshotError dec_err = decode_portable_snapshot(buffer, encoded_size + 1, &snapshot);
    if (dec_err != SnapshotError::INVALID_TOTAL_BYTES) {
        throw std::runtime_error("Trailing bytes must be rejected");
    }
}

TEST(portable_snapshot_rejects_nan) {
    using namespace rts;
    
    uint32_t entity_id = 1;
    float nan_val = std::numeric_limits<float>::quiet_NaN();
    float valid = 100.0f;
    uint8_t is_dead = 0;
    
    uint8_t buffer[256];
    size_t encoded_size = 0;
    
    SnapshotError err = encode_portable_snapshot(
&entity_id, &nan_val, &valid, &valid,
                                                  &valid, &valid, &valid,
                                                  &valid, &valid, &is_dead,
                                                  1, 0, buffer, sizeof(buffer), &encoded_size);
    if (err != SnapshotError::NONFINITE_FLOAT) {
        throw std::runtime_error("NaN position must be rejected");
    }
    
    err = encode_portable_snapshot(
&entity_id, &valid, &valid, &valid,
                                    &valid, &valid, &nan_val,
                                    &valid, &valid, &is_dead,
                                    1, 0, buffer, sizeof(buffer), &encoded_size);
    if (err != SnapshotError::NONFINITE_FLOAT) {
        throw std::runtime_error("NaN velocity must be rejected");
    }
    
    err = encode_portable_snapshot(
&entity_id, &valid, &valid, &valid,
                                    &valid, &valid, &valid,
                                    &nan_val, &valid, &is_dead,
                                    1, 0, buffer, sizeof(buffer), &encoded_size);
    if (err != SnapshotError::NONFINITE_FLOAT) {
        throw std::runtime_error("NaN health_current must be rejected, got " + std::to_string((int)err));
    }
    
    err = encode_portable_snapshot(
&entity_id, &valid, &valid, &valid,
                                    &valid, &valid, &valid,
                                    &valid, &nan_val, &is_dead,
                                    1, 0, buffer, sizeof(buffer), &encoded_size);
    if (err != SnapshotError::NONFINITE_FLOAT) {
        throw std::runtime_error("NaN health_max must be rejected");
    }
}

TEST(portable_snapshot_rejects_invalid_health) {
    using namespace rts;
    
    uint32_t entity_id = 1;
    float pos_x = 0.0f, pos_y = 0.0f, pos_z = 0.0f;
    float vel_x = 0.0f, vel_y = 0.0f, vel_z = 0.0f;
    uint8_t is_dead = 0;
    
    uint8_t buffer[256];
    size_t encoded_size = 0;
    
    float hp_curr = -1.0f, hp_max = 100.0f;
    SnapshotError err = encode_portable_snapshot(
&entity_id, &pos_x, &pos_y, &pos_z,
                                                  &vel_x, &vel_y, &vel_z,
                                                  &hp_curr, &hp_max, &is_dead,
                                                  1, 0, buffer, sizeof(buffer), &encoded_size);
    if (err != SnapshotError::INVALID_HEALTH_RANGE) {
        throw std::runtime_error("Negative health_current must be rejected");
    }
    
    hp_curr = 150.0f; hp_max = 100.0f;
    uint8_t is_dead_u8 = is_dead;
    err = encode_portable_snapshot(
&entity_id, &pos_x, &pos_y, &pos_z,
                                    &vel_x, &vel_y, &vel_z,
                                    &hp_curr, &hp_max, &is_dead_u8,
                                    1, 0, buffer, sizeof(buffer), &encoded_size);
    if (err != SnapshotError::INVALID_HEALTH_RANGE) {
        throw std::runtime_error("health_current > health_max must be rejected");
    }
}

TEST(portable_snapshot_sorts_duplicate_entity_ids) {
    using namespace rts;
    
    uint32_t entity_ids[] = {1, 1};
    float pos_x[] = {10.0f, 20.0f}, pos_y[] = {0.0f, 0.0f}, pos_z[] = {0.0f, 0.0f};
    float vel_x[] = {0.0f, 0.0f}, vel_y[] = {0.0f, 0.0f}, vel_z[] = {0.0f, 0.0f};
    float hp_curr[] = {100.0f, 80.0f}, hp_max[] = {100.0f, 100.0f};
    uint8_t is_dead[] = {0, 1};
    
    uint8_t buffer[256];
    size_t encoded_size = 0;
    
    SnapshotError err = encode_portable_snapshot(
            entity_ids, pos_x, pos_y, pos_z,
                                                  vel_x, vel_y, vel_z,
                                                  hp_curr, hp_max, is_dead,
                                                  2, 0, buffer, sizeof(buffer), &encoded_size);
    if (err != SnapshotError::OK) {
        throw std::runtime_error("Duplicate entity IDs should be sorted, not rejected");
    }
    
    PortableSnapshot snapshot{};
    if (decode_portable_snapshot(buffer, encoded_size, &snapshot) != SnapshotError::OK) {
        throw std::runtime_error("Decoding sorted snapshot should succeed");
    }
    
    if (snapshot.entity_count != 2) {
        throw std::runtime_error("Snapshot should have 2 entities after sorting");
    }
    
    if (snapshot.entities[0] != 1 || snapshot.entities[1] != 1) {
        throw std::runtime_error("Entities should be sorted by entity_id");
    }
    
    if (snapshot.positions_x[0] != 10.0f || snapshot.positions_x[1] != 20.0f) {
        throw std::runtime_error("Entity data should be preserved in sorted order");
    }
    
    if (snapshot.health_current[0] != 100.0f || snapshot.health_current[1] != 80.0f) {
        throw std::runtime_error("Health data should be preserved in sorted order");
    }
}

TEST(portable_snapshot_sorts_out_of_order_entity_ids) {
    using namespace rts;
    
    uint32_t entity_ids[] = {3, 1, 2};
    float pos_x[] = {30.0f, 10.0f, 20.0f}, pos_y[] = {0.0f, 0.0f, 0.0f}, pos_z[] = {0.0f, 0.0f, 0.0f};
    float vel_x[] = {0.0f, 0.0f, 0.0f}, vel_y[] = {0.0f, 0.0f, 0.0f}, vel_z[] = {0.0f, 0.0f, 0.0f};
    float hp_curr[] = {75.0f, 50.0f, 60.0f}, hp_max[] = {100.0f, 100.0f, 100.0f};
    uint8_t is_dead[] = {0, 0, 0};
    
    uint8_t buffer[256];
    size_t encoded_size = 0;
    
    SnapshotError err = encode_portable_snapshot(
            entity_ids, pos_x, pos_y, pos_z,
                                                  vel_x, vel_y, vel_z,
                                                  hp_curr, hp_max, is_dead,
                                                  3, 0, buffer, sizeof(buffer), &encoded_size);
    if (err != SnapshotError::OK) {
        throw std::runtime_error("Out of order entity IDs should be sorted, not rejected");
    }
    
    PortableSnapshot snapshot{};
    if (decode_portable_snapshot(buffer, encoded_size, &snapshot) != SnapshotError::OK) {
        throw std::runtime_error("Decoding sorted snapshot should succeed");
    }
    
    if (snapshot.entity_count != 3) {
        throw std::runtime_error("Snapshot should have 3 entities after sorting");
    }
    
    if (snapshot.entities[0] != 1 || snapshot.entities[1] != 2 || snapshot.entities[2] != 3) {
        throw std::runtime_error("Entities should be sorted ascending by entity_id");
    }
    
    if (snapshot.positions_x[0] != 10.0f || snapshot.positions_x[1] != 20.0f || snapshot.positions_x[2] != 30.0f) {
        throw std::runtime_error("Positions should follow entity_id sorting");
    }
    
     if (snapshot.health_current[0] != 50.0f || snapshot.health_current[1] != 60.0f || snapshot.health_current[2] != 75.0f) {
         throw std::runtime_error("Health data should follow entity_id sorting");
     }
}

TEST(portable_snapshot_rejects_invalid_magic) {
    using namespace rts;
    
    uint8_t buffer[32];
    std::memset(buffer, 0, sizeof(buffer));
    buffer[0] = 'X'; buffer[1] = 'X'; buffer[2] = 'X'; buffer[3] = 'X';
    
    PortableSnapshot snapshot{};
    SnapshotError err = decode_portable_snapshot(buffer, 32, &snapshot);
    if (err != SnapshotError::INVALID_MAGIC) {
        throw std::runtime_error("Invalid magic must be rejected");
    }
}

TEST(portable_snapshot_rejects_unknown_version) {
    using namespace rts;
    
    uint8_t buffer[32];
    std::memset(buffer, 0, sizeof(buffer));
    buffer[0] = 'R'; buffer[1] = 'T'; buffer[2] = 'S'; buffer[3] = 'S';
    buffer[4] = 2; buffer[5] = 0;
    
    PortableSnapshot snapshot{};
    SnapshotError err = decode_portable_snapshot(buffer, 32, &snapshot);
    if (err != SnapshotError::INVALID_VERSION) {
        throw std::runtime_error("Unknown version must be rejected");
    }
}

TEST(portable_snapshot_rejects_invalid_subtick_ms) {
    using namespace rts;
    
    uint8_t buffer[32];
    std::memset(buffer, 0, sizeof(buffer));
    buffer[0] = 'R'; buffer[1] = 'T'; buffer[2] = 'S'; buffer[3] = 'S';
    buffer[4] = 0x01; buffer[5] = 0x00;  // version = 1 (little-endian)
    buffer[6] = 0x01;  // message_kind = 1 (FULL)
    buffer[8] = 0x20; buffer[9] = 0x00;  // header_bytes = 32
    buffer[10] = 0x28; buffer[11] = 0x00;  // record_bytes = 40
    buffer[12] = 0x20; buffer[13] = 0x00; buffer[14] = 0x00; buffer[15] = 0x00;  // total_bytes = 32
    buffer[20] = 0x00; buffer[21] = 0x00; buffer[22] = 0x80; buffer[23] = 0x42;  // subtick_ms = 50.0f (IEEE-754, 0x42800000 little-endian)
    buffer[24] = 0x00; buffer[25] = 0x00; buffer[26] = 0x00; buffer[27] = 0x00; 
    
    PortableSnapshot snapshot{};
    SnapshotError err = decode_portable_snapshot(buffer, 32, &snapshot);
    if (err != SnapshotError::INVALID_SUBTICK_MS) {
        throw std::runtime_error("subtick_ms >= 50 must be rejected, got " + std::to_string((int)err));
    }
}

TEST(portable_snapshot_decode_validation_does_not_modify_output_on_failure) {
    using namespace rts;
    
    uint8_t buffer[32];
    std::memset(buffer, 0, sizeof(buffer));
    buffer[0] = 'X';
    
    PortableSnapshot snapshot{};
    snapshot.tick = 999;
    snapshot.entity_count = 999;
    
    SnapshotError err = decode_portable_snapshot(buffer, 32, &snapshot);
    if (err == SnapshotError::OK) {
        throw std::runtime_error("Decode with invalid magic must fail");
    }
    if (snapshot.tick != 999 || snapshot.entity_count != 999) {
        throw std::runtime_error("Failed decode must not modify output");
    }
}

TEST(portable_snapshot_subtick_ms_validation_upper_bound) {
    using namespace rts;
    
    float valid_subtick = 49.0f;
    uint8_t is_dead = 0;
    
    uint32_t entity_id = 1;
    float pos_x = 0.0f, pos_y = 0.0f, pos_z = 0.0f;
    float vel_x = 0.0f, vel_y = 0.0f, vel_z = 0.0f;
    float hp_curr = 100.0f, hp_max = 100.0f;
    
    uint8_t buffer[256];
    size_t encoded_size = 0;
    
    SnapshotError err = encode_portable_snapshot(
&entity_id, &pos_x, &pos_y, &pos_z,
                                                  &vel_x, &vel_y, &vel_z,
                                                  &hp_curr, &hp_max, &is_dead,
                                                  1, 0, buffer, sizeof(buffer), &encoded_size);
    if (err != SnapshotError::OK) {
        throw std::runtime_error("Encode with valid subtick_ms must succeed");
    }
    
    PortableSnapshot snapshot{};
    err = decode_portable_snapshot(buffer, encoded_size, &snapshot);
    if (err != SnapshotError::OK) {
        throw std::runtime_error("Decode with valid subtick_ms must succeed");
    }
}

TEST(portable_snapshot_roundtrip_preserves_state) {
    using namespace rts;
    
    uint32_t entity_id = 42;
    float pos_x = 100.0f, pos_y = 200.0f, pos_z = 300.0f;
    float vel_x = 10.0f, vel_y = 20.0f, vel_z = 30.0f;
    float hp_curr = 75.5f, hp_max = 100.0f;
    uint8_t is_dead = 0;
    
    uint8_t buffer[256];
    size_t encoded_size = 0;
    
    SnapshotError err = encode_portable_snapshot(
&entity_id, &pos_x, &pos_y, &pos_z,
                                                  &vel_x, &vel_y, &vel_z,
                                                  &hp_curr, &hp_max, &is_dead,
                                                  1, 0, buffer, sizeof(buffer), &encoded_size);
    if (err != SnapshotError::OK) {
        throw std::runtime_error("Encode must succeed");
    }
    
    PortableSnapshot snapshot{};
    err = decode_portable_snapshot(buffer, encoded_size, &snapshot);
    if (err != SnapshotError::OK) {
        throw std::runtime_error("Decode must succeed");
    }
    
    if (snapshot.entities[0] != entity_id) {
        throw std::runtime_error("Entity ID must match");
    }
    if (snapshot.positions_x[0] != pos_x || snapshot.positions_y[0] != pos_y || snapshot.positions_z[0] != pos_z) {
        throw std::runtime_error("Position must match");
    }
    if (snapshot.velocities_x[0] != vel_x || snapshot.velocities_y[0] != vel_y || snapshot.velocities_z[0] != vel_z) {
        throw std::runtime_error("Velocity must match");
    }
    if (snapshot.health_current[0] != hp_curr || snapshot.health_max[0] != hp_max) {
        throw std::runtime_error("Health must match");
    }
     if (snapshot.is_dead[0] != is_dead) {
          throw std::runtime_error("is_dead flag must match");
      }
 }

TEST(portable_snapshot_rejects_nonzero_reserved_bytes) {
    using namespace rts;
    
    uint32_t entity_id = 1;
    float pos_x = 0.0f, pos_y = 0.0f, pos_z = 0.0f;
    float vel_x = 0.0f, vel_y = 0.0f, vel_z = 0.0f;
    float hp_curr = 100.0f, hp_max = 100.0f;
    uint8_t is_dead = 0;
    
    uint8_t buffer[72];
    size_t encoded_size = 0;
    
    SnapshotError err = encode_portable_snapshot(
        &entity_id, &pos_x, &pos_y, &pos_z,
        &vel_x, &vel_y, &vel_z,
        &hp_curr, &hp_max, &is_dead,
        1, 0, buffer, sizeof(buffer), &encoded_size);
    
    if (err != SnapshotError::OK) {
        throw std::runtime_error("Encode must succeed");
    }
    
    buffer[32 + 6] = 1;
    
    PortableSnapshot snapshot{};
    err = decode_portable_snapshot(buffer, encoded_size, &snapshot);
    
    if (err != SnapshotError::INVALID_RESERVED) {
        throw std::runtime_error("Decode with nonzero reserved bytes must fail with INVALID_RESERVED");
    }
}

TEST(portable_snapshot_rejects_missing_active_flag) {
    using namespace rts;
    
    uint32_t entity_id = 1;
    float pos_x = 0.0f, pos_y = 0.0f, pos_z = 0.0f;
    float vel_x = 0.0f, vel_y = 0.0f, vel_z = 0.0f;
    float hp_curr = 100.0f, hp_max = 100.0f;
    uint8_t is_dead = 0;
    
    uint8_t buffer[72];
    size_t encoded_size = 0;
    
    SnapshotError err = encode_portable_snapshot(
        &entity_id, &pos_x, &pos_y, &pos_z,
        &vel_x, &vel_y, &vel_z,
        &hp_curr, &hp_max, &is_dead,
        1, 0, buffer, sizeof(buffer), &encoded_size);
    
    if (err != SnapshotError::OK) {
        throw std::runtime_error("Encode must succeed");
    }
    
    buffer[32 + 5] = 0x02;
    
    PortableSnapshot snapshot{};
    err = decode_portable_snapshot(buffer, encoded_size, &snapshot);
    
    if (err != SnapshotError::INVALID_STATE_FLAGS) {
        throw std::runtime_error("Decode with missing ACTIVE flag must fail with INVALID_STATE_FLAGS");
    }
}

TEST(portable_snapshot_golden_vector_roundtrip) {
    using namespace rts;
    
    constexpr size_t COUNT = 3;
    uint32_t entity_ids[COUNT] = {10, 20, 30};
    float pos_x[COUNT] = {1.0f, 2.0f, 3.0f};
    float pos_y[COUNT] = {4.0f, 5.0f, 6.0f};
    float pos_z[COUNT] = {7.0f, 8.0f, 9.0f};
    float vel_x[COUNT] = {0.1f, 0.2f, 0.3f};
    float vel_y[COUNT] = {0.4f, 0.5f, 0.6f};
    float vel_z[COUNT] = {0.7f, 0.8f, 0.9f};
    float hp_curr[COUNT] = {50.0f, 75.0f, 100.0f};
    float hp_max[COUNT] = {100.0f, 100.0f, 100.0f};
    uint8_t is_dead[COUNT] = {0, 0, 0};
    
    uint8_t buffer[2048];
    size_t encoded_size = 0;
    
    SnapshotError err = encode_portable_snapshot(
        entity_ids, pos_x, pos_y, pos_z,
        vel_x, vel_y, vel_z,
        hp_curr, hp_max, is_dead,
        COUNT, 12345, buffer, sizeof(buffer), &encoded_size);
    
    if (err != SnapshotError::OK) {
        throw std::runtime_error("Encode must succeed");
    }
    
    if (encoded_size != 32 + COUNT * 40) {
        throw std::runtime_error("Encoded size must match header + records");
    }
    
    PortableSnapshot snapshot{};
    err = decode_portable_snapshot(buffer, encoded_size, &snapshot);
    
    if (err != SnapshotError::OK) {
        throw std::runtime_error("Decode must succeed");
    }
    
    if (snapshot.entity_count != COUNT) {
        throw std::runtime_error("Entity count must match");
    }
    
    for (size_t i = 0; i < COUNT; ++i) {
        if (snapshot.entities[i] != entity_ids[i]) {
            throw std::runtime_error("Entity IDs must match");
        }
        if (std::abs(snapshot.positions_x[i] - pos_x[i]) > 1e-6f) {
            throw std::runtime_error("Positions must match");
        }
        if (std::abs(snapshot.positions_y[i] - pos_y[i]) > 1e-6f) {
            throw std::runtime_error("Positions must match");
        }
        if (std::abs(snapshot.positions_z[i] - pos_z[i]) > 1e-6f) {
            throw std::runtime_error("Positions must match");
        }
        if (std::abs(snapshot.velocities_x[i] - vel_x[i]) > 1e-6f) {
            throw std::runtime_error("Velocities must match");
        }
        if (std::abs(snapshot.velocities_y[i] - vel_y[i]) > 1e-6f) {
            throw std::runtime_error("Velocities must match");
        }
        if (std::abs(snapshot.velocities_z[i] - vel_z[i]) > 1e-6f) {
            throw std::runtime_error("Velocities must match");
        }
        if (std::abs(snapshot.health_current[i] - hp_curr[i]) > 1e-6f) {
            throw std::runtime_error("Health must match");
        }
        if (std::abs(snapshot.health_max[i] - hp_max[i]) > 1e-6f) {
            throw std::runtime_error("Health max must match");
        }
        if (snapshot.is_dead[i] != is_dead[i]) {
            throw std::runtime_error("is_dead must match");
        }
    }
    
    if (snapshot.tick != 12345) {
        throw std::runtime_error("Tick must match");
    }
}

TEST(portable_snapshot_roundtrip_zero_entities) {
    using namespace rts;
    
    uint8_t buffer[256];
    size_t encoded_size = 0;
    
    SnapshotError err = encode_portable_snapshot(
        nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr,
        0, 0, buffer, sizeof(buffer), &encoded_size);
    
    if (err != SnapshotError::OK) {
        throw std::runtime_error("Encode zero entities must succeed");
    }
    
    if (encoded_size != 32) {
        throw std::runtime_error("Encoded size for zero entities must be 32 bytes");
    }
    
    PortableSnapshot snapshot{};
    err = decode_portable_snapshot(buffer, encoded_size, &snapshot);
    
    if (err != SnapshotError::OK) {
        throw std::runtime_error("Decode zero entities must succeed");
    }
    
    if (snapshot.entity_count != 0) {
        throw std::runtime_error("Zero entities must decode to count 0");
    }
}

TEST(portable_snapshot_roundtrip_max_entities) {
    using namespace rts;
    
    constexpr size_t COUNT = 100000;
    std::vector<uint32_t> entity_ids(COUNT);
    std::vector<float> pos_x(COUNT), pos_y(COUNT), pos_z(COUNT);
    std::vector<float> vel_x(COUNT), vel_y(COUNT), vel_z(COUNT);
    std::vector<float> hp_curr(COUNT), hp_max(COUNT);
    std::vector<uint8_t> is_dead(COUNT);
    
    for (size_t i = 0; i < COUNT; ++i) {
        entity_ids[i] = static_cast<uint32_t>(i + 1);
        pos_x[i] = static_cast<float>(i * 0.1);
        pos_y[i] = static_cast<float>(i * 0.2);
        pos_z[i] = static_cast<float>(i * 0.3);
        vel_x[i] = 0.0f;
        vel_y[i] = 0.0f;
        vel_z[i] = 0.0f;
        hp_curr[i] = 100.0f;
        hp_max[i] = 100.0f;
        is_dead[i] = 0;
    }
    
    size_t buffer_size = 32 + COUNT * 40;
    std::unique_ptr<uint8_t[]> buffer(new uint8_t[buffer_size]);
    size_t encoded_size = 0;
    
    SnapshotError err = encode_portable_snapshot(
        entity_ids.data(), pos_x.data(), pos_y.data(), pos_z.data(),
        vel_x.data(), vel_y.data(), vel_z.data(),
        hp_curr.data(), hp_max.data(), is_dead.data(),
        COUNT, 99999, buffer.get(), buffer_size, &encoded_size);
    
    if (err != SnapshotError::OK) {
        throw std::runtime_error("Encode 100,000 entities must succeed");
    }
    
    if (encoded_size != buffer_size) {
        throw std::runtime_error("Encoded size must match expected");
    }
    
    PortableSnapshot snapshot{};
    err = decode_portable_snapshot(buffer.get(), encoded_size, &snapshot);
    
    if (err != SnapshotError::OK) {
        throw std::runtime_error("Decode 100,000 entities must succeed");
    }
    
    if (snapshot.entity_count != COUNT) {
        throw std::runtime_error("Entity count must match");
    }
    
    for (size_t i = 0; i < COUNT; ++i) {
        if (snapshot.entities[i] != static_cast<uint32_t>(i + 1)) {
            throw std::runtime_error("Entity IDs must match for large snapshot");
        }
    }
}

TEST(portable_snapshot_rejects_over_max_entities) {
    using namespace rts;
    
    constexpr size_t COUNT = 100001;
    std::vector<uint32_t> entity_ids(COUNT, 1);
    std::vector<float> pos_x(COUNT, 0.0f), pos_y(COUNT, 0.0f), pos_z(COUNT, 0.0f);
    std::vector<float> vel_x(COUNT, 0.0f), vel_y(COUNT, 0.0f), vel_z(COUNT, 0.0f);
    std::vector<float> hp_curr(COUNT, 100.0f), hp_max(COUNT, 100.0f);
    std::vector<uint8_t> is_dead(COUNT, 0);
    
    size_t buffer_size = 32 + COUNT * 40;
    std::unique_ptr<uint8_t[]> buffer(new uint8_t[buffer_size]);
    size_t encoded_size = 0;
    
    SnapshotError err = encode_portable_snapshot(
        entity_ids.data(), pos_x.data(), pos_y.data(), pos_z.data(),
        vel_x.data(), vel_y.data(), vel_z.data(),
        hp_curr.data(), hp_max.data(), is_dead.data(),
        COUNT, 0, buffer.get(), buffer_size, &encoded_size);
    
    if (err != SnapshotError::INVALID_ENTITY_COUNT) {
        throw std::runtime_error("Encode 100,001 entities must fail with INVALID_ENTITY_COUNT");
    }
}
