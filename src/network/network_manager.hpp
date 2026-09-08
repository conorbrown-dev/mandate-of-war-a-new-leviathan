#pragma once

#include <deque>
#include <vector>

#include "buffer.hpp"
#include "serializer.hpp"
#include "portable_snapshot.hpp"
#include "types.hpp"
#include "discovery.hpp"
#include "tcp_transport.hpp"

namespace rts {

class Simulation;

class NetworkManager {
public:
    NetworkManager();
    
    void set_simulation(Simulation* sim);
    
    void update(float delta_ms);
    
    bool start_discovery_server(const std::string& server_name, const std::vector<uint8_t>& map_hash);
    bool start_discovery_client();
    void update_discovery(float delta_ms);
    
    void send_command(const InputCommand& cmd);
    bool receive_command(InputCommand& cmd);
    
    void send_snapshot();
    bool receive_snapshot(Snapshot& snapshot);
    bool receive_snapshot(uint8_t* buffer, size_t buffer_size, size_t& out_encoded_size);
    
    void send_delta_snapshot(uint32_t reference_tick);
    bool receive_delta_snapshot(DeltaSnapshot& snapshot);
    
    void set_remote_position(EntityId entity_id, float x, float y);
    bool get_remote_position(EntityId entity_id, float& x, float& y);
    
    uint32_t current_tick() const { return current_tick_; }
    
    uint64_t bytes_sent() const { return bytes_sent_; }
    uint64_t bytes_received() const { return bytes_received_; }
    uint32_t packets_sent() const { return packets_sent_; }
    uint32_t packets_received() const { return packets_received_; }
    
    float ping_ms() const { return ping_ms_; }
    float jitter_ms() const { return jitter_ms_; }
    float packet_loss_pct() const { return packet_loss_pct_; }
     uint32_t packets_lost() const { return packets_lost_; }
     
     InputBuffer& input_buffer() { return input_buffer_; }
     
     bool connect(const std::string& host, uint16_t port = TCP_PORT);
     bool listen(uint16_t port = TCP_PORT);
     bool accept();
     
     void send_handshake(const ConnectionHandshake& handshake);
     bool receive_handshake(ConnectionHandshake& handshake);
     
     void send_frame_command_batch(const FrameCommandBatch& batch);
     bool receive_frame_command_batch(FrameCommandBatch& batch);
     
      void send_snapshot_checksum(const SnapshotChecksum& checksum);
      bool receive_snapshot_checksum(SnapshotChecksum& checksum);
      
      void capture_snapshot_checksum();
      
      void receive_packets();
     
      void reset();
     
     void validate_checksums();
     
     ~NetworkManager();

private:
    Simulation* simulation_{nullptr};
    uint32_t current_tick_{0};
    
    InputBuffer input_buffer_;
    SnapshotBuffer snapshot_buffer_;
    
    std::vector<float> remote_positions_x_;
    std::vector<float> remote_positions_y_;
    
    uint64_t bytes_sent_{0};
    uint64_t bytes_received_{0};
    uint32_t packets_sent_{0};
    uint32_t packets_received_{0};
    
    float ping_ms_{0.0f};
    float jitter_ms_{0.0f};
    float packet_loss_pct_{0.0f};
    uint32_t packets_lost_{0};
    
    void capture_snapshot();
    void capture_delta_snapshot(uint32_t reference_tick);
    DeltaSnapshot get_delta_snapshot() const;
    
    void update_diagnostics(float delta_ms);
    struct RetransmissionEntry {
        InputCommand cmd;
        uint32_t send_tick;
        uint32_t ack_tick;
        bool acked;
        float timeout;
    };
    std::deque<RetransmissionEntry> unacked_commands_;
    static constexpr float RETRANSMISSION_TIMEOUT_MS = 100.0f;
    
     float last_ping_time_{0.0f};
     float ping_samples_[10]{0};
     uint32_t ping_sample_count_{0};
     
      DiscoveryServer* discovery_server_{nullptr};
      DiscoveryClient* discovery_client_{nullptr};
      std::vector<ServerInfo> discovered_servers_;
      
      TcpTransport tcp_transport_;
      
     SnapshotChecksum pending_remote_checksum_{};
     bool has_pending_remote_checksum_{false};
      uint32_t desync_count_{0};
      uint32_t last_desync_tick_{0};
      
      void set_desync_stats(uint32_t count, uint32_t tick);
      
      uint32_t desync_count() const { return desync_count_; }
      uint32_t last_desync_tick() const { return last_desync_tick_; }
 };

} // namespace rts
