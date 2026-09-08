#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "types.hpp"

namespace rts {

class TcpTransport {
public:
    TcpTransport();
    ~TcpTransport();
    
    bool connect(const std::string& host, uint16_t port);
    bool listen(uint16_t port);
    bool accept();
    
    void disconnect();
    
    bool is_connected() const { return connected_; }
    bool is_server() const { return is_server_; }
    
    int client_socket() const { return client_socket_; }
    
    bool send_handshake(const ConnectionHandshake& handshake);
    bool recv_handshake(ConnectionHandshake& handshake);
    
    bool send_frame_command_batch(const FrameCommandBatch& batch);
    bool recv_frame_command_batch(FrameCommandBatch& batch);
    
    bool send_snapshot_checksum(const SnapshotChecksum& checksum);
    bool recv_snapshot_checksum(SnapshotChecksum& checksum);
    
    size_t available() const;
    size_t read(uint8_t* buffer, size_t size);
    size_t write(const uint8_t* data, size_t size);
    
    bool poll(float timeout_ms);
    
    void update(float delta_ms);
    
    uint64_t bytes_sent() const { return bytes_sent_; }
    uint64_t bytes_received() const { return bytes_received_; }
    
    void reset();

private:
    int socket_{-1};
    int client_socket_{-1};
    bool connected_{false};
    bool is_server_{false};
    
    std::vector<uint8_t> recv_buffer_;
    size_t recv_buffer_pos_{0};
    size_t recv_buffer_count_{0};
    
    uint64_t bytes_sent_{0};
    uint64_t bytes_received_{0};
    
    float last_activity_time_{0.0f};
    
    bool send_all(const uint8_t* data, size_t size);
    bool recv_all(uint8_t* data, size_t size);
};

} // namespace rts
