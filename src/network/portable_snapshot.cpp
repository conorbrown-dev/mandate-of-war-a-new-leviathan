#include "portable_snapshot.hpp"

#include <cstring>
#include <algorithm>
#include <vector>

#include "types.hpp"

namespace {

constexpr uint32_t read_uint32_le(const uint8_t* ptr) {
    uint32_t val = 0;
    val |= static_cast<uint32_t>(ptr[0]) << 0;
    val |= static_cast<uint32_t>(ptr[1]) << 8;
    val |= static_cast<uint32_t>(ptr[2]) << 16;
    val |= static_cast<uint32_t>(ptr[3]) << 24;
    return val;
}

constexpr uint16_t read_uint16_le(const uint8_t* ptr) {
    uint16_t val = 0;
    val |= static_cast<uint16_t>(ptr[0]) << 0;
    val |= static_cast<uint16_t>(ptr[1]) << 8;
    return val;
}

void write_uint32_le(uint8_t* ptr, uint32_t val) {
    ptr[0] = static_cast<uint8_t>(val & 0xFF);
    ptr[1] = static_cast<uint8_t>((val >> 8) & 0xFF);
    ptr[2] = static_cast<uint8_t>((val >> 16) & 0xFF);
    ptr[3] = static_cast<uint8_t>((val >> 24) & 0xFF);
}

void write_uint16_le(uint8_t* ptr, uint16_t val) {
    ptr[0] = static_cast<uint8_t>(val & 0xFF);
    ptr[1] = static_cast<uint8_t>((val >> 8) & 0xFF);
}

void write_uint8(uint8_t* ptr, uint8_t val) {
    ptr[0] = val;
}

rts::SnapshotError validate_float_impl(float value) {
    if (std::isnan(value) || !std::isfinite(value)) {
        return rts::SnapshotError::NONFINITE_FLOAT;
    }
    return rts::SnapshotError::OK;
}

} 

rts::SnapshotError rts::validate_float(float value) {
    return validate_float_impl(value);
}

rts::SnapshotError rts::validate_subtick_ms(float value) {
    if (validate_float_impl(value) != SnapshotError::OK) {
        return SnapshotError::NONFINITE_FLOAT;
    }
    if (value < 0.0f || value >= 50.0f) {
        return SnapshotError::INVALID_SUBTICK_MS;
    }
    return SnapshotError::OK;
}

rts::SnapshotError rts::validate_health(float current, float max) {
    if (validate_float_impl(current) != SnapshotError::OK) {
        return SnapshotError::NONFINITE_FLOAT;
    }
    if (validate_float_impl(max) != SnapshotError::OK) {
        return SnapshotError::NONFINITE_FLOAT;
    }
    if (max < 0.0f) {
        return SnapshotError::INVALID_HEALTH_RANGE;
    }
    if (current < 0.0f || current > max) {
        return SnapshotError::INVALID_HEALTH_RANGE;
    }
    return SnapshotError::OK;
}

rts::SnapshotError rts::decode_portable_snapshot(const uint8_t* buffer, size_t buffer_size, PortableSnapshot* out_snapshot) {
    if (!buffer || !out_snapshot) {
        return SnapshotError::BUFFER_TOO_SHORT;
    }
    
    if (buffer_size < PORTABLE_SNAPSHOT_HEADER_SIZE) {
        return SnapshotError::BUFFER_TOO_SHORT;
    }
    
    uint32_t magic = read_uint32_le(buffer + 0);
    if (magic != PORTABLE_SNAPSHOT_MAGIC) {
        return SnapshotError::INVALID_MAGIC;
    }
    
    uint16_t version = read_uint16_le(buffer + 4);
    if (version != PORTABLE_SNAPSHOT_VERSION) {
        return SnapshotError::INVALID_VERSION;
    }
    
    uint8_t kind = buffer[6];
    if (kind != PORTABLE_SNAPSHOT_MESSAGE_KIND_FULL) {
        return SnapshotError::INVALID_MESSAGE_KIND;
    }
    
    uint16_t header_bytes = read_uint16_le(buffer + 8);
    if (header_bytes != PORTABLE_SNAPSHOT_HEADER_SIZE) {
        return SnapshotError::INVALID_HEADER_SIZE;
    }
    
    uint16_t record_bytes = read_uint16_le(buffer + 10);
    if (record_bytes != PORTABLE_SNAPSHOT_ENTITY_SIZE) {
        return SnapshotError::INVALID_RECORD_SIZE;
    }
    
    uint32_t total_bytes = read_uint32_le(buffer + 12);
    uint32_t entity_count_header = read_uint32_le(buffer + 24);
    
    size_t expected_total = PORTABLE_SNAPSHOT_HEADER_SIZE + entity_count_header * PORTABLE_SNAPSHOT_ENTITY_SIZE;
    if (total_bytes != expected_total) {
        return SnapshotError::INVALID_TOTAL_BYTES;
    }
    
    if (buffer_size != total_bytes) {
        return SnapshotError::INVALID_TOTAL_BYTES;
    }
    
    uint32_t tick = read_uint32_le(buffer + 16);
    float subtick_ms = std::bit_cast<float>(read_uint32_le(buffer + 20));
    uint32_t reserved = read_uint32_le(buffer + 28);
    
    if (reserved != 0) {
        return SnapshotError::INVALID_RESERVED;
    }
    
    if (validate_subtick_ms(subtick_ms) != SnapshotError::OK) {
        return SnapshotError::INVALID_SUBTICK_MS;
    }
    
    if (entity_count_header > PORTABLE_SNAPSHOT_MAX_ENTITIES) {
        return SnapshotError::INVALID_ENTITY_COUNT;
    }
    
    uint8_t flags = buffer[7];
    if (flags != 0) {
        return SnapshotError::INVALID_FLAGS;
    }
    
    out_snapshot->tick = tick;
    out_snapshot->subtick_ms = subtick_ms;
    out_snapshot->entity_count = entity_count_header;
    
    out_snapshot->entities.resize(entity_count_header);
    out_snapshot->positions_x.resize(entity_count_header);
    out_snapshot->positions_y.resize(entity_count_header);
    out_snapshot->positions_z.resize(entity_count_header);
    out_snapshot->velocities_x.resize(entity_count_header);
    out_snapshot->velocities_y.resize(entity_count_header);
    out_snapshot->velocities_z.resize(entity_count_header);
    out_snapshot->health_current.resize(entity_count_header);
    out_snapshot->health_max.resize(entity_count_header);
    out_snapshot->is_dead.resize(entity_count_header);
    
    const uint8_t* entity_ptr = buffer + PORTABLE_SNAPSHOT_HEADER_SIZE;
    
    for (uint32_t i = 0; i < entity_count_header; ++i) {
        uint32_t id = read_uint32_le(entity_ptr + 0);
        if (id == 0xFFFFFFFF) {
            return SnapshotError::INVALID_ENTITY_ID;
        }
        
        uint8_t component_mask = entity_ptr[4];
        if (component_mask != (0x07)) {
            return SnapshotError::INVALID_COMPONENT_MASK;
        }
        
        uint8_t state_flags = entity_ptr[5];
        if ((state_flags & ~0x03) != 0 || (state_flags & 0x01) == 0) {
            return SnapshotError::INVALID_STATE_FLAGS;
        }
        
        if (read_uint16_le(entity_ptr + 6) != 0) {
            return SnapshotError::INVALID_RESERVED;
        }
        
        uint8_t alive = state_flags & 0x01;
        uint8_t dead = (state_flags >> 1) & 0x01;
        
        int32_t pos_x = read_uint32_le(entity_ptr + 8);
        int32_t pos_y = read_uint32_le(entity_ptr + 12);
        int32_t pos_z = read_uint32_le(entity_ptr + 16);
        int32_t vel_x = read_uint32_le(entity_ptr + 20);
        int32_t vel_y = read_uint32_le(entity_ptr + 24);
        int32_t vel_z = read_uint32_le(entity_ptr + 28);
        
        float pos_x_f = std::bit_cast<float>(pos_x);
        float pos_y_f = std::bit_cast<float>(pos_y);
        float pos_z_f = std::bit_cast<float>(pos_z);
        float vel_x_f = std::bit_cast<float>(vel_x);
        float vel_y_f = std::bit_cast<float>(vel_y);
        float vel_z_f = std::bit_cast<float>(vel_z);
        
        if (validate_float_impl(pos_x_f) != SnapshotError::OK ||
            validate_float_impl(pos_y_f) != SnapshotError::OK ||
            validate_float_impl(pos_z_f) != SnapshotError::OK) {
            return SnapshotError::NONFINITE_FLOAT;
        }
        
        if (validate_float_impl(vel_x_f) != SnapshotError::OK ||
            validate_float_impl(vel_y_f) != SnapshotError::OK ||
            validate_float_impl(vel_z_f) != SnapshotError::OK) {
            return SnapshotError::NONFINITE_FLOAT;
        }
        
        int32_t hp_curr = read_uint32_le(entity_ptr + 32);
        int32_t hp_max = read_uint32_le(entity_ptr + 36);
        
        float current_hp = std::bit_cast<float>(hp_curr);
        float max_hp = std::bit_cast<float>(hp_max);
        
        if (validate_health(current_hp, max_hp) != SnapshotError::OK) {
            return SnapshotError::INVALID_HEALTH_RANGE;
        }
        
        out_snapshot->entities[i] = id;
        out_snapshot->positions_x[i] = pos_x_f;
        out_snapshot->positions_y[i] = pos_y_f;
        out_snapshot->positions_z[i] = pos_z_f;
        out_snapshot->velocities_x[i] = vel_x_f;
        out_snapshot->velocities_y[i] = vel_y_f;
        out_snapshot->velocities_z[i] = vel_z_f;
        out_snapshot->health_current[i] = current_hp;
        out_snapshot->health_max[i] = max_hp;
        out_snapshot->is_dead[i] = dead;
        
        entity_ptr += PORTABLE_SNAPSHOT_ENTITY_SIZE;
    }
    
    return SnapshotError::OK;
}

rts::SnapshotError rts::encode_portable_snapshot(const uint32_t* entity_ids, const float* pos_x, const float* pos_y, const float* pos_z,
                                                   const float* vel_x, const float* vel_y, const float* vel_z,
                                                   const float* health_curr, const float* health_max, const uint8_t* is_dead,
                                                   size_t count, uint32_t tick, uint8_t* buffer, size_t buffer_size, size_t* out_encoded_size) {
    if (!buffer || !out_encoded_size) {
        return SnapshotError::BUFFER_TOO_SHORT;
    }
    
    if (count > PORTABLE_SNAPSHOT_MAX_ENTITIES) {
        return SnapshotError::INVALID_ENTITY_COUNT;
    }
    
    if (buffer_size < PORTABLE_SNAPSHOT_HEADER_SIZE + count * PORTABLE_SNAPSHOT_ENTITY_SIZE) {
        return SnapshotError::BUFFER_TOO_SHORT;
    }
    
    auto normalize_zero = [](float value) -> float {
        if (value == 0.0f) {
            return 0.0f;
        }
        return value;
    };
    
    std::vector<uint32_t> sorted_ids(entity_ids, entity_ids + count);
    std::vector<float> sorted_pos_x(count), sorted_pos_y(count), sorted_pos_z(count);
    std::vector<float> sorted_vel_x(count), sorted_vel_y(count), sorted_vel_z(count);
    std::vector<float> sorted_health_curr(count), sorted_health_max(count);
     std::vector<uint8_t> sorted_is_dead(count);
     
     for (size_t i = 0; i < count; ++i) {
         sorted_pos_x[i] = normalize_zero(pos_x[i]);
         sorted_pos_y[i] = normalize_zero(pos_y[i]);
         sorted_pos_z[i] = normalize_zero(pos_z ? pos_z[i] : 0.0f);
         sorted_vel_x[i] = normalize_zero(vel_x ? vel_x[i] : 0.0f);
         sorted_vel_y[i] = normalize_zero(vel_y ? vel_y[i] : 0.0f);
         sorted_vel_z[i] = normalize_zero(vel_z ? vel_z[i] : 0.0f);
         sorted_health_curr[i] = normalize_zero(health_curr[i]);
         sorted_health_max[i] = normalize_zero(health_max[i]);
          sorted_is_dead[i] = is_dead ? is_dead[i] : 0;
      }
     
     for (size_t i = 0; i < count; ++i) {
         if (sorted_ids[i] == 0xFFFFFFFF) {
             return SnapshotError::INVALID_ENTITY_ID;
         }
         
         if (validate_float_impl(sorted_pos_x[i]) != SnapshotError::OK ||
             validate_float_impl(sorted_pos_y[i]) != SnapshotError::OK ||
             validate_float_impl(sorted_pos_z[i]) != SnapshotError::OK) {
             return SnapshotError::NONFINITE_FLOAT;
         }
         
         if (validate_float_impl(sorted_vel_x[i]) != SnapshotError::OK ||
             validate_float_impl(sorted_vel_y[i]) != SnapshotError::OK ||
             validate_float_impl(sorted_vel_z[i]) != SnapshotError::OK) {
             return SnapshotError::NONFINITE_FLOAT;
         }
           
             SnapshotError health_err = validate_health(sorted_health_curr[i], sorted_health_max[i]);
             if (health_err != SnapshotError::OK) {
                 return health_err;
             }
         }
        
        size_t n = sorted_ids.size();
      uint32_t total_bytes = PORTABLE_SNAPSHOT_HEADER_SIZE + static_cast<uint32_t>(count * PORTABLE_SNAPSHOT_ENTITY_SIZE);
     
     uint8_t* ptr = buffer;
     
     write_uint32_le(ptr + 0, PORTABLE_SNAPSHOT_MAGIC);
     write_uint16_le(ptr + 4, PORTABLE_SNAPSHOT_VERSION);
     write_uint8(ptr + 6, PORTABLE_SNAPSHOT_MESSAGE_KIND_FULL);
     write_uint8(ptr + 7, 0);
     write_uint16_le(ptr + 8, PORTABLE_SNAPSHOT_HEADER_SIZE);
     write_uint16_le(ptr + 10, PORTABLE_SNAPSHOT_ENTITY_SIZE);
     write_uint32_le(ptr + 12, total_bytes);
     write_uint32_le(ptr + 16, tick);
     write_uint32_le(ptr + 20, 0);
     write_uint32_le(ptr + 24, static_cast<uint32_t>(count));
     write_uint32_le(ptr + 28, 0);
     
     if (n > 1) {
         for (size_t i = 0; i < n - 1; ++i) {
             for (size_t j = 0; j < n - i - 1; ++j) {
                 if (sorted_ids[j] > sorted_ids[j + 1]) {
                     std::swap(sorted_ids[j], sorted_ids[j + 1]);
                     std::swap(sorted_pos_x[j], sorted_pos_x[j + 1]);
                     std::swap(sorted_pos_y[j], sorted_pos_y[j + 1]);
                     std::swap(sorted_pos_z[j], sorted_pos_z[j + 1]);
                     std::swap(sorted_vel_x[j], sorted_vel_x[j + 1]);
                     std::swap(sorted_vel_y[j], sorted_vel_y[j + 1]);
                     std::swap(sorted_vel_z[j], sorted_vel_z[j + 1]);
                     std::swap(sorted_health_curr[j], sorted_health_curr[j + 1]);
                     std::swap(sorted_health_max[j], sorted_health_max[j + 1]);
                     std::swap(sorted_is_dead[j], sorted_is_dead[j + 1]);
                 }
              }
          }
       }
       
       ptr = buffer + PORTABLE_SNAPSHOT_HEADER_SIZE;
     
     for (size_t i = 0; i < count; ++i) {
         write_uint32_le(ptr + 0, sorted_ids[i]);
         write_uint8(ptr + 4, 0x07);
         write_uint8(ptr + 5, 0x01 | (sorted_is_dead[i] ? 0x02 : 0x00));
         std::memset(ptr + 6, 0, 2);
         
         int32_t pos_x_bits = std::bit_cast<int32_t>(sorted_pos_x[i]);
         int32_t pos_y_bits = std::bit_cast<int32_t>(sorted_pos_y[i]);
         int32_t pos_z_bits = std::bit_cast<int32_t>(sorted_pos_z[i]);
         int32_t vel_x_bits = std::bit_cast<int32_t>(sorted_vel_x[i]);
         int32_t vel_y_bits = std::bit_cast<int32_t>(sorted_vel_y[i]);
         int32_t vel_z_bits = std::bit_cast<int32_t>(sorted_vel_z[i]);
         int32_t hp_curr_bits = std::bit_cast<int32_t>(sorted_health_curr[i]);
         int32_t hp_max_bits = std::bit_cast<int32_t>(sorted_health_max[i]);
         
         write_uint32_le(ptr + 8, pos_x_bits);
         write_uint32_le(ptr + 12, pos_y_bits);
         write_uint32_le(ptr + 16, pos_z_bits);
         write_uint32_le(ptr + 20, vel_x_bits);
         write_uint32_le(ptr + 24, vel_y_bits);
         write_uint32_le(ptr + 28, vel_z_bits);
         write_uint32_le(ptr + 32, hp_curr_bits);
         write_uint32_le(ptr + 36, hp_max_bits);
         
         ptr += PORTABLE_SNAPSHOT_ENTITY_SIZE;
     }
     
     *out_encoded_size = total_bytes;
     return SnapshotError::OK;
 }
