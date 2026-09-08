#include "replay_reader.hpp"
#include <cstring>
#include <stdexcept>

namespace rts {

ReplayReader::ReplayReader() : crc_(0xFFFFFFFF), crc_valid_(false) {}

ReplayReader::~ReplayReader() {
    close();
}

bool ReplayReader::open(const std::string& path) {
    file_.exceptions(std::ios::failbit | std::ios::badbit);
    try {
        file_.open(path, std::ios::binary);
    } catch (const std::exception& e) {
        return false;
    }
    
    if (!file_.is_open()) {
        return false;
    }
    
    // Verify magic
    uint32_t magic;
    file_.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    if (magic != kReplayMagic) {
        close();
        return false;
    }
    
    // Verify version
    uint8_t version;
    file_.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (version != kReplayVersion) {
        close();
        return false;
    }
    
    return true;
}

void ReplayReader::close() {
    if (file_.is_open()) {
        // Read and verify CRC32
        uint32_t stored_crc;
        if (file_.gcount() >= 0) {
            file_.seekg(-static_cast<std::streamoff>(sizeof(stored_crc)), std::ios::end);
            if (file_.read(reinterpret_cast<char*>(&stored_crc), sizeof(stored_crc))) {
                crc_valid_ = (crc_ == (stored_crc ^ 0xFFFFFFFF));
            }
        }
        file_.close();
    }
}

bool ReplayReader::read_header(ReplayMetadata& metadata, uint32_t& stored_crc) {
    // Read stored CRC (4 bytes after magic+version)
    file_.read(reinterpret_cast<char*>(&stored_crc), sizeof(stored_crc));
    update_crc(reinterpret_cast<const uint8_t*>(&stored_crc), sizeof(stored_crc));
    
    // Map name length
    uint8_t map_name_len;
    file_.read(reinterpret_cast<char*>(&map_name_len), sizeof(map_name_len));
    update_crc(reinterpret_cast<const uint8_t*>(&map_name_len), sizeof(map_name_len));
    
    // Map name
    std::vector<char> map_name_buf(map_name_len);
    file_.read(map_name_buf.data(), map_name_len);
    update_crc(reinterpret_cast<const uint8_t*>(map_name_buf.data()), map_name_len);
    metadata.map_name = std::string(map_name_buf.data(), map_name_len);
    
    // Timestamp
    file_.read(reinterpret_cast<char*>(&metadata.timestamp), sizeof(metadata.timestamp));
    update_crc(reinterpret_cast<const uint8_t*>(&metadata.timestamp), sizeof(metadata.timestamp));
    
    // Player count
    uint8_t player_count;
    file_.read(reinterpret_cast<char*>(&player_count), sizeof(player_count));
    update_crc(reinterpret_cast<const uint8_t*>(&player_count), sizeof(player_count));
    
    // Player IDs
    metadata.player_ids.resize(player_count);
    for (size_t i = 0; i < player_count; ++i) {
        file_.read(reinterpret_cast<char*>(&metadata.player_ids[i]), sizeof(uint8_t));
        update_crc(reinterpret_cast<const uint8_t*>(&metadata.player_ids[i]), sizeof(uint8_t));
    }
    
    return true;
}

bool ReplayReader::read_snapshot(std::vector<uint8_t>& data) {
    // Read snapshot size
    uint32_t size;
    file_.read(reinterpret_cast<char*>(&size), sizeof(size));
    update_crc(reinterpret_cast<const uint8_t*>(&size), sizeof(size));
    
    if (size == 0 || size > 1024 * 1024) { // Sanity check
        return false;
    }
    
    // Read snapshot data
    data.resize(size);
    file_.read(reinterpret_cast<char*>(data.data()), size);
    update_crc(data.data(), size);
    
    return true;
}

bool ReplayReader::read_command(std::vector<uint8_t>& data) {
    // Commands are always 20 bytes for InputCommand
    data.resize(20);
    file_.read(reinterpret_cast<char*>(data.data()), 20);
    update_crc(data.data(), 20);
    
    return !file_.eof();
}

void ReplayReader::update_crc(const uint8_t* data, size_t size) {
    // Simple CRC32 (same as writer)
    for (size_t i = 0; i < size; ++i) {
        uint8_t byte = data[i];
        for (int j = 0; j < 8; ++j) {
            bool bit = (byte >> (7 - j)) & 1;
            bool crc_bit = (crc_ >> 31) & 1;
            crc_ <<= 1;
            if (bit ^ crc_bit) {
                crc_ ^= 0x04C11DB7;
            }
        }
    }
}

} // namespace rts
