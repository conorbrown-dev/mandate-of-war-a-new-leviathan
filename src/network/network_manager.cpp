#include "network/network_manager.hpp"
#include "simulation/simulation.hpp"
#include "network/portable_snapshot.hpp"
#include "network/types.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <openssl/sha.h>

namespace rts {

NetworkManager::NetworkManager()
    :     input_buffer_(MAX_COMMANDS_PER_TICK * 2),
      snapshot_buffer_(SNAPSHOT_HISTORY_SIZE),
      remote_positions_x_(1024, 0.0f),
      remote_positions_y_(1024, 0.0f) {
    for (uint32_t i = 0; i < 10; ++i) {
        ping_samples_[i] = 0.0f;
    }
}

NetworkManager::~NetworkManager() {
    if (discovery_server_) {
        discovery_server_->shutdown();
        delete discovery_server_;
    }
    if (discovery_client_) {
        discovery_client_->shutdown();
        delete discovery_client_;
    }
}

void NetworkManager::set_simulation(Simulation* sim) {
    simulation_ = sim;
}

bool NetworkManager::start_discovery_server(const std::string& server_name, const std::vector<uint8_t>& map_hash) {
    if (!discovery_server_) {
        discovery_server_ = new DiscoveryServer();
    }
    
    if (discovery_server_->init()) {
        ServerInfo info{};
        info.server_name = server_name;
        info.map_hash = map_hash;
        info.player_count = 0;
        info.max_players = 32;
        info.version_major = 1;
        info.version_minor = 0;
        info.version_patch = 0;
        discovery_server_->set_server_info(info);
        return true;
    }
    return false;
}

bool NetworkManager::start_discovery_client() {
    if (!discovery_client_) {
        discovery_client_ = new DiscoveryClient();
    }
    return discovery_client_->init();
}

void NetworkManager::update_discovery(float delta_ms) {
    int elapsed_ms = static_cast<int>(delta_ms);
    
    if (discovery_server_) {
        discovery_server_->update(elapsed_ms);
    }
    
    if (discovery_client_) {
        discovery_client_->broadcast_request();
        
        uint8_t buffer[1024];
        ssize_t bytes_recv;
        sockaddr_in sender_addr;
        socklen_t sender_len = sizeof(sender_addr);
        
        while ((bytes_recv = recvfrom(discovery_client_->socket_fd(), buffer, sizeof(buffer), 0,
                                      reinterpret_cast<sockaddr*>(&sender_addr), &sender_len)) > 0) {
            ServerInfo info{};
            if (discovery_client_->handle_packet(buffer, bytes_recv, info)) {
                bool found = false;
                for (const auto& existing : discovered_servers_) {
                    if (existing.server_name == info.server_name) {
                        found = true;
                         break;
                     }
                 }
                 if (!found) {
                     discovered_servers_.push_back(info);
                 }
             }
         }
         
         discovery_client_->update(elapsed_ms);
     }
}

void NetworkManager::update(float delta_ms) {
    (void)delta_ms;
    current_tick_ = simulation_ ? simulation_->get_state().tick_number : 0;
    
     if (simulation_ && simulation_->entity_count() > 0) {
         capture_snapshot();
         receive_packets();
     }
     
     update_diagnostics(delta_ms);
     validate_checksums();
    
    std::deque<RetransmissionEntry> retransmit_queue;
    for (auto& entry : unacked_commands_) {
        entry.timeout -= delta_ms;
        if (entry.timeout <= 0.0f && !entry.acked) {
            retransmit_queue.push_back(entry);
            entry.timeout = RETRANSMISSION_TIMEOUT_MS;
        }
    }
    
    for (const auto& entry : retransmit_queue) {
        input_buffer_.push(entry.cmd);
        bytes_sent_ += sizeof(InputCommand);
        packets_sent_++;
        packets_lost_++;
    }
    
    unacked_commands_.erase(
        std::remove_if(unacked_commands_.begin(), unacked_commands_.end(),
            [](const RetransmissionEntry& e) { return e.acked; }),
        unacked_commands_.end()
    );
}

void NetworkManager::send_command(const InputCommand& cmd) {
    input_buffer_.push(cmd);
    
    RetransmissionEntry entry;
    entry.cmd = cmd;
    entry.send_tick = current_tick_;
    entry.ack_tick = 0;
    entry.acked = false;
    entry.timeout = RETRANSMISSION_TIMEOUT_MS;
    unacked_commands_.push_back(entry);
    
    bytes_sent_ += sizeof(InputCommand);
    packets_sent_++;
}

bool NetworkManager::receive_command(InputCommand& cmd) {
    bool result = input_buffer_.pop(cmd);
    if (result) {
        bytes_received_ += sizeof(InputCommand);
        packets_received_++;
        
        for (auto& entry : unacked_commands_) {
            if (entry.cmd.tick_id == cmd.tick_id) {
                entry.acked = true;
                entry.ack_tick = current_tick_;
                float rtt = static_cast<float>(current_tick_ - entry.send_tick) * 50.0f;
                
                if (ping_sample_count_ < 10) {
                    ping_samples_[ping_sample_count_] = rtt;
                    ping_sample_count_++;
                } else {
                    for (uint32_t i = 0; i < 9; ++i) {
                        ping_samples_[i] = ping_samples_[i + 1];
                    }
                    ping_samples_[9] = rtt;
                }
                
                ping_ms_ = 0.0f;
                for (uint32_t i = 0; i < ping_sample_count_; ++i) {
                    ping_ms_ += ping_samples_[i];
                }
                ping_ms_ /= static_cast<float>(std::max(1U, ping_sample_count_));
             }
         }
     }
     return result;
 }

 void NetworkManager::validate_checksums() {
     if (!has_pending_remote_checksum_) {
         return;
     }
     
     if (simulation_) {
         auto state = simulation_->get_state();
         if (pending_remote_checksum_.tick == current_tick_) {
             uint8_t buffer[1024 * 1024];
             size_t buffer_size = sizeof(buffer);
             size_t encoded_size = 0;
             
             SnapshotError error = encode_portable_snapshot(
                 state.entity_ids.data(),
                 state.positions_x.data(),
                 state.positions_y.data(),
                 nullptr,
                 state.velocities_x.data(),
                 state.velocities_y.data(),
                 nullptr,
                 state.health_current.data(),
                 state.health_max.data(),
                 state.is_dead.data(),
                 static_cast<size_t>(state.entity_ids.size()),
                 current_tick_,
                 buffer,
                 buffer_size,
                 &encoded_size
             );
             
             if (error == SnapshotError::OK) {
                 uint8_t hash_buffer[SHA256_DIGEST_LENGTH];
                 SHA256(buffer, encoded_size, hash_buffer);
                 
                 uint64_t local_hash = 0;
                 std::memcpy(&local_hash, hash_buffer, sizeof(uint64_t));
                 
                 if (local_hash != pending_remote_checksum_.snapshot_hash) {
                     desync_count_++;
                     last_desync_tick_ = current_tick_;
                 }
             }
         }
     }
     
     has_pending_remote_checksum_ = false;
 }

   void NetworkManager::send_snapshot() {
      if (!simulation_) return;
      
      auto state = simulation_->get_state();
      
      Snapshot snapshot{};
      snapshot.tick = current_tick_;
      snapshot.entity_count = 0;

      for (size_t i = 0; i < state.entity_ids.size(); ++i) {
          if (snapshot.entity_count >= MAX_SNAPSHOT_ENTITIES) {
              break;
          }

          if (state.is_dead[i]) {
              continue;
          }

          auto& entity_snapshot = snapshot.entities[snapshot.entity_count++];
          entity_snapshot.entity_id = state.entity_ids[i];
          entity_snapshot.position.x = state.positions_x[i];
          entity_snapshot.position.y = state.positions_y[i];
          entity_snapshot.position.z = 0.0f;
          entity_snapshot.velocity.x = state.velocities_x[i];
          entity_snapshot.velocity.y = state.velocities_y[i];
          entity_snapshot.velocity.z = 0.0f;
          entity_snapshot.health.current = state.health_current[i];
          entity_snapshot.health.max = state.health_max[i];
          entity_snapshot.alive = 1;
      }
      
        snapshot_buffer_.store(snapshot);
        
        if (state.entity_ids.empty()) {
            return;
        }
        
        std::vector<uint8_t> buffer(4000032);
        size_t buffer_size = buffer.size();
        size_t encoded_size = 0;
        
        SnapshotError error = encode_portable_snapshot(
            state.entity_ids.data(),
            state.positions_x.data(),
            state.positions_y.data(),
            nullptr,
            state.velocities_x.data(),
            state.velocities_y.data(),
            nullptr,
            state.health_current.data(),
            state.health_max.data(),
            state.is_dead.data(),
            static_cast<uint32_t>(state.entity_ids.size()),
            current_tick_,
            buffer.data(),
            buffer_size,
            &encoded_size
        );
        
         if (error == SnapshotError::OK) {
             snapshot_buffer_.store_legacy_buffer(buffer.data(), encoded_size, current_tick_);
             bytes_sent_ += encoded_size;
             packets_sent_++;
             
             uint8_t hash_buffer[SHA256_DIGEST_LENGTH];
             SHA256(buffer.data(), encoded_size, hash_buffer);
             
             uint64_t snapshot_hash = 0;
             std::memcpy(&snapshot_hash, hash_buffer, sizeof(uint64_t));
             
             SnapshotChecksum checksum{};
             checksum.tick = current_tick_;
             checksum.snapshot_hash = snapshot_hash;
             std::memset(checksum.padding, 0, 8);
             
             tcp_transport_.send_snapshot_checksum(checksum);
             bytes_sent_ += sizeof(SnapshotChecksum);
             packets_sent_++;
        }
    }

 void NetworkManager::receive_packets() {
     if (!tcp_transport_.is_connected()) {
         return;
     }
     
     uint8_t buffer[65536];
     ssize_t n = ::recv(tcp_transport_.client_socket(), buffer, sizeof(buffer), MSG_DONTWAIT);
     if (n <= 0) {
         return;
     }
     
     bytes_received_ += n;
     
     size_t offset = 0;
     while (offset + 8 <= static_cast<size_t>(n)) {
         uint8_t* packet_data = buffer + offset;
         uint32_t packet_type = static_cast<uint32_t>(packet_data[0]) |
                                (static_cast<uint32_t>(packet_data[1]) << 8U) |
                                (static_cast<uint32_t>(packet_data[2]) << 16U) |
                                (static_cast<uint32_t>(packet_data[3]) << 24U);
         uint32_t packet_size = static_cast<uint32_t>(packet_data[4]) |
                                (static_cast<uint32_t>(packet_data[5]) << 8U) |
                                (static_cast<uint32_t>(packet_data[6]) << 16U) |
                                (static_cast<uint32_t>(packet_data[7]) << 24U);
         
         if (offset + 8 + packet_size > static_cast<size_t>(n)) {
             break;
         }
         
         TransportType type = static_cast<TransportType>(packet_type);
         switch (type) {
             case TransportType::FRAME_COMMAND_BATCH: {
                 FrameCommandBatch batch;
                 size_t consumed = deserialize_frame_command_batch(packet_data + 8, packet_size, batch);
                 if (consumed > 0) {
                     for (uint32_t i = 0; i < batch.command_count; ++i) {
                         input_buffer_.push(batch.commands[i]);
                         bytes_received_ += sizeof(InputCommand);
                         packets_received_++;
                     }
                 }
                 break;
             }
              case TransportType::SNAPSHOT_CHECKSUM: {
                  SnapshotChecksum checksum;
                  if (receive_snapshot_checksum(checksum)) {
                      pending_remote_checksum_ = checksum;
                      has_pending_remote_checksum_ = true;
                  }
                  break;
              }
             default:
                 break;
         }
         
         offset += 8 + packet_size;
     }
 }

 bool NetworkManager::receive_snapshot(Snapshot& snapshot) {
    bool result = snapshot_buffer_.get(current_tick_, snapshot);
    if (result) {
        bytes_received_ += sizeof(Snapshot);
        packets_received_++;
    }
    return result;
}

bool NetworkManager::receive_snapshot(uint8_t* buffer, size_t buffer_size, size_t& out_encoded_size) {
    bool result = snapshot_buffer_.get_legacy_buffer(current_tick_, buffer, buffer_size, out_encoded_size);
    if (result) {
        bytes_received_ += out_encoded_size;
        packets_received_++;
    }
    return result;
}

void NetworkManager::send_delta_snapshot(uint32_t reference_tick) {
    if (!simulation_) return;
    
    auto state = simulation_->get_state();
    
    // Build delta snapshot - mark all entities as changed since reference state
    // TODO: Implement proper delta compression against reference_tick
    DeltaSnapshot delta;
    delta.tick = state.tick_number;
    delta.sequence_id = current_tick_;
    delta.reference_tick = reference_tick;
    delta.entity_count = 0;
    
    for (size_t i = 0; i < state.entity_ids.size() && delta.entity_count < MAX_SNAPSHOT_ENTITIES; ++i) {
        EntityId entity_id = state.entity_ids[i];
        
        bool changed = (reference_tick == 0) || (delta.entity_count == 0);
        
        if (changed) {
            delta.entities[delta.entity_count].entity_id = static_cast<uint16_t>(entity_id);
            delta.entities[delta.entity_count].flags = 0;
            delta.entity_count++;
        }
    }
    
    snapshot_buffer_.store_delta(delta);
    bytes_sent_ += sizeof(DeltaSnapshot);
    packets_sent_++;
}

bool NetworkManager::receive_delta_snapshot(DeltaSnapshot& snapshot) {
    bool result = snapshot_buffer_.get_delta(current_tick_, snapshot);
    if (result) {
        bytes_received_ += sizeof(DeltaSnapshot);
        packets_received_++;
    }
    return result;
}

void NetworkManager::capture_delta_snapshot(uint32_t reference_tick) {
    send_delta_snapshot(reference_tick);
}

DeltaSnapshot NetworkManager::get_delta_snapshot() const {
    DeltaSnapshot snapshot;
    snapshot_buffer_.get_delta(current_tick_, snapshot);
    return snapshot;
}

void NetworkManager::set_remote_position(EntityId entity_id, float x, float y) {
    if (entity_id < remote_positions_x_.size()) {
        remote_positions_x_[entity_id] = x;
        remote_positions_y_[entity_id] = y;
    }
}

bool NetworkManager::get_remote_position(EntityId entity_id, float& x, float& y) {
    if (entity_id >= remote_positions_x_.size()) {
        return false;
    }
    x = remote_positions_x_[entity_id];
    y = remote_positions_y_[entity_id];
    return true;
}

void NetworkManager::capture_snapshot() {
    send_snapshot();
}

void NetworkManager::capture_snapshot_checksum() {
    if (!simulation_) return;
    
    auto state = simulation_->get_state();
    
    uint8_t buffer[1024 * 1024];
    size_t buffer_size = sizeof(buffer);
    size_t encoded_size = 0;
    
    SnapshotError error = encode_portable_snapshot(
        state.entity_ids.data(),
        state.positions_x.data(),
        state.positions_y.data(),
        nullptr,
        state.velocities_x.data(),
        state.velocities_y.data(),
        nullptr,
        state.health_current.data(),
        state.health_max.data(),
        state.is_dead.data(),
        static_cast<size_t>(state.entity_ids.size()),
        current_tick_,
        buffer,
        buffer_size,
        &encoded_size
    );
    
    if (error == SnapshotError::OK) {
        uint8_t hash_buffer[SHA256_DIGEST_LENGTH];
        SHA256(buffer, encoded_size, hash_buffer);
        
        SnapshotChecksum checksum;
        checksum.tick = current_tick_;
        std::memcpy(&checksum.snapshot_hash, hash_buffer, sizeof(uint64_t));
        
        send_snapshot_checksum(checksum);
    }
}

void NetworkManager::update_diagnostics(float delta_ms) {
    (void)delta_ms;
    
    if (packets_sent_ > 0) {
        packet_loss_pct_ = (static_cast<float>(packets_lost_) / static_cast<float>(packets_sent_)) * 100.0f;
    } else {
        packet_loss_pct_ = 0.0f;
    }
    
    if (packets_received_ > 1) {
        jitter_ms_ = std::abs(ping_ms_ * 0.1f);
    }
    
    unacked_commands_.erase(
        std::remove_if(unacked_commands_.begin(), unacked_commands_.end(),
            [](const RetransmissionEntry& e) { return e.acked; }),
        unacked_commands_.end()
    );
}

void NetworkManager::reset() {
    current_tick_ = 0;
    input_buffer_.clear();
    snapshot_buffer_.clear();
    remote_positions_x_.assign(1024, 0.0f);
    remote_positions_y_.assign(1024, 0.0f);
    bytes_sent_ = 0;
    bytes_received_ = 0;
    packets_sent_ = 0;
    packets_received_ = 0;
    ping_ms_ = 0.0f;
    jitter_ms_ = 0.0f;
    packet_loss_pct_ = 0.0f;
    packets_lost_ = 0;
    unacked_commands_.clear();
    last_ping_time_ = 0.0f;
    ping_sample_count_ = 0;
    tcp_transport_.reset();
}

bool NetworkManager::connect(const std::string& host, uint16_t port) {
    return tcp_transport_.connect(host, port);
}

bool NetworkManager::listen(uint16_t port) {
    return tcp_transport_.listen(port);
}

bool NetworkManager::accept() {
    return tcp_transport_.accept();
}

void NetworkManager::send_handshake(const ConnectionHandshake& handshake) {
    tcp_transport_.send_handshake(handshake);
    bytes_sent_ += sizeof(ConnectionHandshake) + 8;
    packets_sent_++;
}

bool NetworkManager::receive_handshake(ConnectionHandshake& handshake) {
    bool result = tcp_transport_.recv_handshake(handshake);
    if (result) {
        bytes_received_ += sizeof(ConnectionHandshake) + 8;
        packets_received_++;
    }
    return result;
}

void NetworkManager::send_frame_command_batch(const FrameCommandBatch& batch) {
    tcp_transport_.send_frame_command_batch(batch);
    size_t packet_size = sizeof(FrameCommandBatch) + 8;
    bytes_sent_ += packet_size;
    packets_sent_++;
}

bool NetworkManager::receive_frame_command_batch(FrameCommandBatch& batch) {
    bool result = tcp_transport_.recv_frame_command_batch(batch);
    if (result) {
        bytes_received_ += sizeof(FrameCommandBatch) + 8;
        packets_received_++;
    }
    return result;
}

void NetworkManager::send_snapshot_checksum(const SnapshotChecksum& checksum) {
    tcp_transport_.send_snapshot_checksum(checksum);
    bytes_sent_ += sizeof(SnapshotChecksum) + 8;
    packets_sent_++;
}

bool NetworkManager::receive_snapshot_checksum(SnapshotChecksum& checksum) {
    bool result = tcp_transport_.recv_snapshot_checksum(checksum);
    if (result) {
        bytes_received_ += sizeof(SnapshotChecksum) + 8;
        packets_received_++;
     }
     return result;
 }

 void NetworkManager::set_desync_stats(uint32_t count, uint32_t tick) {
     desync_count_ = count;
     last_desync_tick_ = tick;
 }

 } // namespace rts
