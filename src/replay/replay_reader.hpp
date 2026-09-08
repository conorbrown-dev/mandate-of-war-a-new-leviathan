#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "replay/types.hpp"

namespace rts {

class ReplayReader {
public:
    ReplayReader();
    ~ReplayReader();
    
    bool open(const std::string& path);
    void close();
    
    bool read_header(ReplayMetadata& metadata, uint32_t& stored_crc);
    bool read_snapshot(std::vector<uint8_t>& data);
    bool read_command(std::vector<uint8_t>& data);
    
    bool is_open() const { return file_.is_open(); }
    uint32_t crc32() const { return crc_; }
    bool check_crc() const { return crc_valid_; }
    
private:
    std::ifstream file_;
    uint32_t crc_;
    bool crc_valid_;
    
    void update_crc(const uint8_t* data, size_t size);
};

} // namespace rts