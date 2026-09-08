#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "types.hpp"
#include "serializer.hpp"

namespace rts {

struct ServerInfo {
    std::string server_name;
    std::vector<uint8_t> map_hash;
    uint32_t player_count;
    uint32_t max_players;
    uint32_t version_major;
    uint32_t version_minor;
    uint32_t version_patch;
};

class DiscoveryServer {
public:
    DiscoveryServer();
    ~DiscoveryServer();
    
    bool init();
    void shutdown();
    
    void set_server_info(const ServerInfo& info);
    void update(int elapsed_ms);
    
    bool handle_packet(const uint8_t* data, size_t size);

private:
    int socket_fd_;
    bool initialized_;
    ServerInfo server_info_;
    int update_interval_ms_;
    int accumulator_ms_;
};

class DiscoveryClient {
public:
    DiscoveryClient();
    ~DiscoveryClient();
    
    bool init();
    void shutdown();
    
    void broadcast_request();
    
    bool handle_packet(const uint8_t* data, size_t size, ServerInfo& info);
    
    const std::vector<ServerInfo>& discovered_servers() const { return discovered_servers_; }
    
    int socket_fd() const { return socket_fd_; }
    
    void update(int elapsed_ms);

private:
    int socket_fd_;
    bool initialized_;
    std::vector<ServerInfo> discovered_servers_;
    int discovery_timeout_ms_;
    int broadcast_interval_ms_;
    int broadcast_accumulator_ms_;
};

} // namespace rts
