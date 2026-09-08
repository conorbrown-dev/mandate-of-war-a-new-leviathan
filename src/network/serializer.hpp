#pragma once

#include <cstddef>
#include <cstdint>

#include "types.hpp"

namespace rts {

size_t serialize_input_command(const InputCommand& cmd, uint8_t* buffer, size_t buffer_size);
size_t deserialize_input_command(const uint8_t* buffer, size_t buffer_size, InputCommand& cmd);

size_t serialize_snapshot(const Snapshot& snapshot, uint8_t* buffer, size_t buffer_size);
size_t deserialize_snapshot(const uint8_t* buffer, size_t buffer_size, Snapshot& snapshot);

size_t serialize_delta_snapshot(const DeltaSnapshot& snapshot, uint8_t* buffer, size_t buffer_size);
size_t deserialize_delta_snapshot(const uint8_t* buffer, size_t buffer_size, DeltaSnapshot& snapshot);

size_t serialize_discovery_packet(const DiscoveryPacket& packet, uint8_t* buffer, size_t buffer_size);
size_t deserialize_discovery_packet(const uint8_t* buffer, size_t buffer_size, DiscoveryPacket& packet);

size_t serialize_join_request(const JoinRequest& request, uint8_t* buffer, size_t buffer_size);
size_t deserialize_join_request(const uint8_t* buffer, size_t buffer_size, JoinRequest& request);

size_t serialize_join_response(const JoinResponse& response, uint8_t* buffer, size_t buffer_size);
size_t deserialize_join_response(const uint8_t* buffer, size_t buffer_size, JoinResponse& response);

size_t serialize_connection_handshake(const ConnectionHandshake& handshake, uint8_t* buffer, size_t buffer_size);
size_t deserialize_connection_handshake(const uint8_t* buffer, size_t buffer_size, ConnectionHandshake& handshake);

size_t serialize_frame_command_batch(const FrameCommandBatch& batch, uint8_t* buffer, size_t buffer_size);
size_t deserialize_frame_command_batch(const uint8_t* buffer, size_t buffer_size, FrameCommandBatch& batch);

size_t serialize_snapshot_checksum(const SnapshotChecksum& checksum, uint8_t* buffer, size_t buffer_size);
size_t deserialize_snapshot_checksum(const uint8_t* buffer, size_t buffer_size, SnapshotChecksum& checksum);

} // namespace rts
