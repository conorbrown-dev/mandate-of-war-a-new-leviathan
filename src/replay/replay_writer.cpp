#include "replay_writer.hpp"
#include <iostream>
#include <ostream>
#include <cstring>

namespace rts {

ReplayWriter::ReplayWriter() : crc_(0xFFFFFFFF) {}

ReplayWriter::~ReplayWriter() {
    close();
}

bool ReplayWriter::open(const std::string& path) {
    file_.exceptions(std::ios::failbit | std::ios::badbit);
    try {
        file_.open(path, std::ios::binary | std::ios::trunc);
    } catch (const std::exception& e) {
        return false;
    }
    return file_.is_open();
}

void ReplayWriter::close() {
    if (file_.is_open()) {
        // Write CRC32 at end
        uint32_t final_crc = crc_ ^ 0xFFFFFFFF;
        file_.write(reinterpret_cast<const char*>(&final_crc), sizeof(final_crc));
        file_.close();
    }
}

bool ReplayWriter::write_header(const ReplayMetadata& metadata) {
    // Magic
    uint32_t magic = kReplayMagic;
    file_.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    update_crc(reinterpret_cast<const uint8_t*>(&magic), sizeof(magic));
    
    // Version
    uint8_t version = kReplayVersion;
    file_.write(reinterpret_cast<const char*>(&version), sizeof(version));
    update_crc(reinterpret_cast<const uint8_t*>(&version), sizeof(version));
    
    // Reserve space for CRC (will be filled at close)
    uint32_t crc_placeholder = 0;
    file_.write(reinterpret_cast<const char*>(&crc_placeholder), sizeof(crc_placeholder));
    
    // Map name
    uint8_t map_name_len = static_cast<uint8_t>(metadata.map_name.length());
    file_.write(reinterpret_cast<const char*>(&map_name_len), sizeof(map_name_len));
    update_crc(reinterpret_cast<const uint8_t*>(&map_name_len), sizeof(map_name_len));
    file_.write(metadata.map_name.data(), map_name_len);
    update_crc(reinterpret_cast<const uint8_t*>(metadata.map_name.data()), map_name_len);
    
    // Timestamp
    file_.write(reinterpret_cast<const char*>(&metadata.timestamp), sizeof(metadata.timestamp));
    update_crc(reinterpret_cast<const uint8_t*>(&metadata.timestamp), sizeof(metadata.timestamp));
    
    // Player count
    uint8_t player_count = static_cast<uint8_t>(metadata.player_ids.size());
    file_.write(reinterpret_cast<const char*>(&player_count), sizeof(player_count));
    update_crc(reinterpret_cast<const uint8_t*>(&player_count), sizeof(player_count));
    
    // Player IDs
    for (uint8_t id : metadata.player_ids) {
        file_.write(reinterpret_cast<const char*>(&id), sizeof(id));
        update_crc(reinterpret_cast<const uint8_t*>(&id), sizeof(id));
    }
    
    return true;
}

bool ReplayWriter::write_snapshot(const uint8_t* data, size_t size) {
    // Write snapshot size (4 bytes)
    uint32_t size32 = static_cast<uint32_t>(size);
    std::cerr << "write_snapshot: size = " << size << ", size32 = " << size32 << ", initial pos = " << file_.tellp() << "\n";
    
    std::streampos before_size = file_.tellp();
    file_.write(reinterpret_cast<const char*>(&size32), sizeof(size32));
    std::streampos after_size = file_.tellp();
    
    std::cerr << "After size write: before=" << before_size << " after=" << after_size << " diff=" << (after_size - before_size) << "\n";
    
    if (file_.fail()) {
        std::cerr << "ERROR: write of size failed\n";
        return false;
    }
    
    update_crc(reinterpret_cast<const uint8_t*>(&size32), sizeof(size32));
    
    // Write snapshot data
    std::streampos before_data = file_.tellp();
    file_.write(reinterpret_cast<const char*>(data), size);
    std::streampos after_data = file_.tellp();
    
    std::cerr << "After data write: before=" << before_data << " after=" << after_data << " diff=" << (after_data - before_data) << "\n";
    
    if (file_.fail()) {
        std::cerr << "ERROR: write of data failed\n";
        return false;
    }
    
    update_crc(data, size);
    
    file_.flush();
    
    std::cerr << "After flush, pos = " << file_.tellp() << "\n";
    if (file_.fail()) {
        std::cerr << "ERROR: flush failed\n";
        return false;
    }
    
    return true;
}

bool ReplayWriter::write_command(const uint8_t* data, size_t size) {
    // Commands are now 20 bytes for InputCommand
    file_.write(reinterpret_cast<const char*>(data), size);
    update_crc(data, size);
    return true;
}

void ReplayWriter::update_crc(const uint8_t* data, size_t size) {
    // Simple CRC32 implementation (placeholder)
    // In production, use a proper CRC32 library (e.g., zlib,Boost)
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
