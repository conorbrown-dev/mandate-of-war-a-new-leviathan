#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "replay/types.hpp"

namespace rts {

class ReplayWriter {
public:
    ReplayWriter();
    ~ReplayWriter();
    
    bool open(const std::string& path);
    void close();
    
    bool write_header(const ReplayMetadata& metadata);
    bool write_snapshot(const uint8_t* data, size_t size);
    bool write_command(const uint8_t* data, size_t size);
    
    bool is_open() const { return file_.is_open(); }
    uint32_t crc32() const { return crc_; }
    std::streampos tellp() { return file_.tellp(); }
    
private:
    std::ofstream file_;
    uint32_t crc_;
    
    void update_crc(const uint8_t* data, size_t size);
};

} // namespace rts