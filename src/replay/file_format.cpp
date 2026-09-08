#include "file_format.hpp"

#include <cstring>
#include <stdexcept>

namespace rts {

ReplayFile::ReplayFile() = default;

ReplayFile::~ReplayFile() {
    if (file_.is_open()) {
        file_.close();
    }
}

bool ReplayFile::open_for_recording(const std::string& path) {
    file_.open(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file_.is_open()) {
        return false;
    }
    recording_ = true;
    pos_ = 0;
    pos_ = 0;
    return true;
}

bool ReplayFile::open_for_playback(const std::string& path) {
    file_.open(path, std::ios::in | std::ios::binary);
    if (!file_.is_open()) {
        return false;
    }
    recording_ = false;
    pos_ = 0;
    pos_ = 0;
    return true;
}

bool ReplayFile::write_header(const ReplayHeader& header) {
    if (!recording_) {
        return false;
    }
    
    file_.seekp(0, std::ios::beg);
    file_.write(reinterpret_cast<const char*>(&header), sizeof(header));
    
    return file_.good();
}

bool ReplayFile::write_commands(const uint8_t* data, size_t size) {
    if (!recording_) {
        return false;
    }
    
    file_.write(reinterpret_cast<const char*>(data), size);
    return file_.good();
}

bool ReplayFile::write_rng_states(const uint8_t* data, size_t size) {
    if (!recording_) {
        return false;
    }
    
    file_.write(reinterpret_cast<const char*>(data), size);
    return file_.good();
}

bool ReplayFile::write_snapshots(const uint8_t* data, size_t size) {
    if (!recording_) {
        return false;
    }
    
    file_.write(reinterpret_cast<const char*>(data), size);
    return file_.good();
}

bool ReplayFile::close() {
    if (!file_.is_open()) {
        return true;
    }
    
    file_.close();
    return !file_.fail();
}

bool ReplayFile::read_header(ReplayHeader& header) {
    if (recording_) {
        return false;
    }
    
    file_.seekg(0, std::ios::beg);
    file_.read(reinterpret_cast<char*>(&header), sizeof(header));
    
    if (file_.gcount() != sizeof(header)) {
        return false;
    }
    
    return validate_header(header);
}

bool ReplayFile::read_commands(std::vector<uint8_t>& data) {
    if (recording_) {
        return false;
    }
    
    ReplayHeader header;
    if (!read_header(header)) {
        return false;
    }
    
    if (header.command_length == 0) {
        data.clear();
        return true;
    }
    
    data.resize(header.command_length);
    file_.seekg(header.command_offset, std::ios::beg);
    file_.read(reinterpret_cast<char*>(data.data()), header.command_length);
    
    return file_.gcount() == static_cast<std::streamsize>(header.command_length);
}

bool ReplayFile::read_rng_states(std::vector<uint8_t>& data) {
    if (recording_) {
        return false;
    }
    
    ReplayHeader header;
    if (!read_header(header)) {
        return false;
    }
    
    if (header.rng_length == 0) {
        data.clear();
        return true;
    }
    
    data.resize(header.rng_length);
    file_.seekg(header.rng_offset, std::ios::beg);
    file_.read(reinterpret_cast<char*>(data.data()), header.rng_length);
    
    return file_.gcount() == static_cast<std::streamsize>(header.rng_length);
}

bool ReplayFile::read_snapshots(std::vector<uint8_t>& data) {
    if (recording_) {
        return false;
    }
    
    ReplayHeader header;
    if (!read_header(header)) {
        return false;
    }
    
    if (header.snapshot_length == 0) {
        data.clear();
        return true;
    }
    
    data.resize(header.snapshot_length);
    file_.seekg(header.snapshot_offset, std::ios::beg);
    file_.read(reinterpret_cast<char*>(data.data()), header.snapshot_length);
    
    return file_.gcount() == static_cast<std::streamsize>(header.snapshot_length);
}

bool ReplayFile::validate_header(const ReplayHeader& header) {
    if (header.magic != REPLAY_MAGIC) {
        return false;
    }
    
    if (header.version != REPLAY_VERSION) {
        return false;
    }
    
    if (header.reserved != 0) {
        return false;
    }
    
    pos_ = 0;
    return true;
}

bool ReplayFile::is_valid() const {
    return file_.is_open();
}

bool ReplayFile::verify_map_hash(const std::string& map_hash) {
    if (map_hash.length() != MAP_HASH_LENGTH) {
        return false;
    }
    
    ReplayHeader header;
    if (!read_header(header)) {
        return false;
    }
    
    return std::memcmp(header.map_hash, map_hash.c_str(), MAP_HASH_LENGTH) == 0;
}

} // namespace rts
