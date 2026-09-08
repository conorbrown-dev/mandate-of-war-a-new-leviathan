#include "network/discovery.hpp"
#include <cstring>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <openssl/sha.h>

namespace rts {

DiscoveryServer::DiscoveryServer()
    : socket_fd_(-1)
    , initialized_(false)
    , server_info_{}
    , update_interval_ms_(DISCOVERY_INTERVAL_MS)
    , accumulator_ms_(0)
{
}

DiscoveryServer::~DiscoveryServer() {
    shutdown();
}

bool DiscoveryServer::init() {
    if (initialized_) {
        return true;
    }

    socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd_ < 0) {
        return false;
    }

    int flags = fcntl(socket_fd_, F_GETFL, 0);
    fcntl(socket_fd_, F_SETFL, flags | O_NONBLOCK);

    int broadcast = 1;
    if (setsockopt(socket_fd_, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        close(socket_fd_);
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(DISCOVERY_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(socket_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(socket_fd_);
        return false;
    }

    initialized_ = true;
    return true;
}

void DiscoveryServer::shutdown() {
    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
    initialized_ = false;
}

void DiscoveryServer::set_server_info(const ServerInfo& info) {
    server_info_ = info;
}

void DiscoveryServer::update(int elapsed_ms) {
    accumulator_ms_ += elapsed_ms;
    if (accumulator_ms_ >= update_interval_ms_ && initialized_) {
        DiscoveryPacket packet{};
        packet.header.protocol_version = NETWORK_PROTOCOL_VERSION;
        packet.header.type = MessageType::SYNC_REQUEST;
        packet.header.timestamp_ms = static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count() / 1000000);
        packet.header.sequence_id = 0;
        packet.discovery_type = DiscoveryType::BROADCAST;
        
        std::memset(packet.server_name, 0, MAX_SERVER_NAME_LENGTH);
        std::strncpy(packet.server_name, server_info_.server_name.c_str(), MAX_SERVER_NAME_LENGTH - 1);
        
        std::memset(packet.map_hash, 0, MAP_HASH_LENGTH);
        if (server_info_.map_hash.size() <= MAP_HASH_LENGTH) {
            std::memcpy(packet.map_hash, server_info_.map_hash.data(), server_info_.map_hash.size());
        }
        
        packet.player_count = server_info_.player_count;
        packet.max_players = server_info_.max_players;
    packet.version_major = server_info_.version_major;
    packet.version_minor = server_info_.version_minor;
    packet.version_patch = server_info_.version_patch;

        sockaddr_in broadcast_addr{};
        broadcast_addr.sin_family = AF_INET;
        broadcast_addr.sin_port = htons(DISCOVERY_PORT);
        broadcast_addr.sin_addr.s_addr = inet_addr("255.255.255.255");

        uint8_t buffer[sizeof(DiscoveryPacket)];
        size_t packet_size = serialize_discovery_packet(packet, buffer, sizeof(buffer));
        
        if (packet_size > 0) {
            sendto(socket_fd_, buffer, packet_size, 0,
                   reinterpret_cast<sockaddr*>(&broadcast_addr), sizeof(broadcast_addr));
        }

        accumulator_ms_ = 0;
    }
}

bool DiscoveryServer::handle_packet(const uint8_t* data, size_t size) {
    if (!data || size < sizeof(NetworkHeader)) {
        return false;
    }

    NetworkHeader header{};
    std::memcpy(&header, data, sizeof(NetworkHeader));

    if (header.type != MessageType::SYNC_REQUEST) {
        return false;
    }

    if (size < sizeof(DiscoveryPacket)) {
        return false;
    }

    DiscoveryPacket request{};
    deserialize_discovery_packet(data, size, request);

    if (request.discovery_type != DiscoveryType::JOIN_REQUEST) {
        return false;
    }

    JoinResponse response{};
    response.header.protocol_version = NETWORK_PROTOCOL_VERSION;
    response.header.type = MessageType::SYNC_RESPONSE;
    response.header.timestamp_ms = header.timestamp_ms;
    response.header.sequence_id = header.sequence_id + 1;
    response.slot_id = 0;
    std::memcpy(response.accepted_map_hash, server_info_.map_hash.data(), MAP_HASH_LENGTH);
    response.tick = 0;
    response.snapshot_hash = 0;

    sockaddr_in client_addr{};
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(request.header.sequence_id);
    client_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    uint8_t buffer[sizeof(JoinResponse)];
    size_t packet_size = serialize_join_response(response, buffer, sizeof(buffer));
    
    if (packet_size > 0) {
        sendto(socket_fd_, buffer, packet_size, 0,
               reinterpret_cast<sockaddr*>(&client_addr), sizeof(client_addr));
    }

    return true;
}

DiscoveryClient::DiscoveryClient()
    : socket_fd_(-1)
    , initialized_(false)
    , discovered_servers_{}
    , discovery_timeout_ms_(5000)
    , broadcast_interval_ms_(500)
    , broadcast_accumulator_ms_(0)
{
}

DiscoveryClient::~DiscoveryClient() {
    shutdown();
}

bool DiscoveryClient::init() {
    if (initialized_) {
        return true;
    }

    socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd_ < 0) {
        return false;
    }

    int flags = fcntl(socket_fd_, F_GETFL, 0);
    fcntl(socket_fd_, F_SETFL, flags | O_NONBLOCK);

    int broadcast = 1;
    if (setsockopt(socket_fd_, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        close(socket_fd_);
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(DISCOVERY_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(socket_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(socket_fd_);
        return false;
    }

    initialized_ = true;
    return true;
}

void DiscoveryClient::shutdown() {
    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
    initialized_ = false;
    discovered_servers_.clear();
}

void DiscoveryClient::broadcast_request() {
    if (!initialized_) {
        return;
    }

    JoinRequest request{};
    request.header.protocol_version = NETWORK_PROTOCOL_VERSION;
    request.header.type = MessageType::SYNC_REQUEST;
    request.header.timestamp_ms = static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count() / 1000000);
    request.header.sequence_id = 0;
    std::memset(request.player_name, 0, MAX_SERVER_NAME_LENGTH);
    std::strncpy(request.player_name, "Player1", MAX_SERVER_NAME_LENGTH - 1);
    request.version_major = 1;
    request.version_minor = 0;
    request.version_patch = 0;
    std::memset(request.accepted_map_hash, 0, MAP_HASH_LENGTH);

    sockaddr_in broadcast_addr{};
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(DISCOVERY_PORT);
    broadcast_addr.sin_addr.s_addr = inet_addr("255.255.255.255");

    uint8_t buffer[sizeof(JoinRequest)];
    size_t packet_size = serialize_join_request(request, buffer, sizeof(buffer));
    
    if (packet_size > 0) {
        sendto(socket_fd_, buffer, packet_size, 0,
               reinterpret_cast<sockaddr*>(&broadcast_addr), sizeof(broadcast_addr));
    }
}

bool DiscoveryClient::handle_packet(const uint8_t* data, size_t size, ServerInfo& info) {
    if (!data || size < sizeof(DiscoveryPacket)) {
        return false;
    }

    DiscoveryPacket packet{};
    deserialize_discovery_packet(data, size, packet);

    if (packet.discovery_type != DiscoveryType::BROADCAST) {
        return false;
    }

    info.server_name = std::string(packet.server_name);
    info.map_hash.resize(MAP_HASH_LENGTH);
    std::memcpy(info.map_hash.data(), packet.map_hash, MAP_HASH_LENGTH);
    info.player_count = packet.player_count;
    info.max_players = packet.max_players;
    info.version_major = packet.version_major;
    info.version_minor = packet.version_minor;
    info.version_patch = packet.version_patch;

    return true;
}

void DiscoveryClient::update(int elapsed_ms) {
    broadcast_accumulator_ms_ += elapsed_ms;
    if (broadcast_accumulator_ms_ >= broadcast_interval_ms_) {
        broadcast_request();
        broadcast_accumulator_ms_ = 0;
    }
}

} // namespace rts

