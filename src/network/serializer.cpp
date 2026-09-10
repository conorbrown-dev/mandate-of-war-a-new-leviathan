#include "network/serializer.hpp"
#include <cstring>

namespace rts {

namespace {

void write_u16_le(uint8_t* destination, uint16_t value) {
    destination[0] = static_cast<uint8_t>(value & 0xFFU);
    destination[1] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
}

void write_u32_le(uint8_t* destination, uint32_t value) {
    destination[0] = static_cast<uint8_t>(value & 0xFFU);
    destination[1] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
    destination[2] = static_cast<uint8_t>((value >> 16U) & 0xFFU);
    destination[3] = static_cast<uint8_t>((value >> 24U) & 0xFFU);
}

uint16_t read_u16_le(const uint8_t* source) {
    return static_cast<uint16_t>(source[0]) |
           static_cast<uint16_t>(static_cast<uint16_t>(source[1]) << 8U);
}

uint32_t read_u32_le(const uint8_t* source) {
    return static_cast<uint32_t>(source[0]) |
           (static_cast<uint32_t>(source[1]) << 8U) |
           (static_cast<uint32_t>(source[2]) << 16U) |
           (static_cast<uint32_t>(source[3]) << 24U);
}

} // namespace

size_t serialize_input_command(const InputCommand& cmd, uint8_t* buffer, size_t buffer_size) {
    if (!buffer || buffer_size < INPUT_COMMAND_WIRE_SIZE) {
        return 0;
    }

    std::memset(buffer, 0, INPUT_COMMAND_WIRE_SIZE);
    write_u32_le(buffer, cmd.tick_id);
    write_u32_le(buffer + 4, cmd.entity_id);
    buffer[8] = cmd.player_id;
    buffer[9] = cmd.cmd_type;
    write_u16_le(buffer + 10, static_cast<uint16_t>(cmd.target_x));
    write_u16_le(buffer + 12, static_cast<uint16_t>(cmd.target_y));
    write_u32_le(buffer + 14, cmd.extra);
    return INPUT_COMMAND_WIRE_SIZE;
}

size_t deserialize_input_command(const uint8_t* buffer, size_t buffer_size, InputCommand& cmd) {
    if (!buffer || buffer_size < INPUT_COMMAND_WIRE_SIZE) {
        return 0;
    }

    cmd.tick_id = read_u32_le(buffer);
    cmd.entity_id = read_u32_le(buffer + 4);
    cmd.player_id = buffer[8];
    cmd.cmd_type = buffer[9];
    cmd.target_x = static_cast<int16_t>(read_u16_le(buffer + 10));
    cmd.target_y = static_cast<int16_t>(read_u16_le(buffer + 12));
    cmd.extra = read_u32_le(buffer + 14);
    return INPUT_COMMAND_WIRE_SIZE;
}

size_t serialize_snapshot(const Snapshot& snapshot, uint8_t* buffer, size_t buffer_size) {
    if (!buffer || snapshot.entity_count > MAX_SNAPSHOT_ENTITIES) {
        return 0;
    }
    size_t required_size = sizeof(SnapshotHeader) + snapshot.entity_count * sizeof(EntitySnapshot);
    if (buffer_size < required_size) {
        return 0;
    }
    
    SnapshotHeader header;
    header.tick = snapshot.tick;
    header.entity_count = snapshot.entity_count;
    
    uint8_t* ptr = buffer;
    std::memcpy(ptr, &header, sizeof(SnapshotHeader));
    ptr += sizeof(SnapshotHeader);
    
    for (uint32_t i = 0; i < snapshot.entity_count; ++i) {
        std::memcpy(ptr, &snapshot.entities[i], sizeof(EntitySnapshot));
        ptr += sizeof(EntitySnapshot);
    }
    
    return required_size;
}

size_t deserialize_snapshot(const uint8_t* buffer, size_t buffer_size, Snapshot& snapshot) {
    if (!buffer || buffer_size < sizeof(SnapshotHeader)) {
        return 0;
    }
    
    SnapshotHeader header;
    std::memcpy(&header, buffer, sizeof(SnapshotHeader));
    if (header.entity_count > MAX_SNAPSHOT_ENTITIES) {
        return 0;
    }
    
    size_t required_size = sizeof(SnapshotHeader) + header.entity_count * sizeof(EntitySnapshot);
    if (buffer_size < required_size) {
        return 0;
    }
    
    snapshot.tick = header.tick;
    snapshot.entity_count = header.entity_count;
    
    const uint8_t* ptr = buffer + sizeof(SnapshotHeader);
    for (uint32_t i = 0; i < header.entity_count; ++i) {
        std::memcpy(&snapshot.entities[i], ptr, sizeof(EntitySnapshot));
        ptr += sizeof(EntitySnapshot);
    }
    
    return required_size;
}

size_t serialize_delta_snapshot(const DeltaSnapshot& snapshot, uint8_t* buffer, size_t buffer_size) {
    if (!buffer || snapshot.entity_count > MAX_SNAPSHOT_ENTITIES) {
        return 0;
    }
    size_t header_size = sizeof(uint32_t) * 4;
    size_t entities_size = snapshot.entity_count * sizeof(DeltaEntitySnapshot);
    size_t required_size = header_size + entities_size;
    
    if (buffer_size < required_size) {
        return 0;
    }
    
    uint8_t* ptr = buffer;
    std::memcpy(ptr, &snapshot.tick, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    std::memcpy(ptr, &snapshot.sequence_id, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    std::memcpy(ptr, &snapshot.reference_tick, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    std::memcpy(ptr, &snapshot.entity_count, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    
    for (uint32_t i = 0; i < snapshot.entity_count; ++i) {
        std::memcpy(ptr, &snapshot.entities[i], sizeof(DeltaEntitySnapshot));
        ptr += sizeof(DeltaEntitySnapshot);
    }
    
    return required_size;
}

size_t deserialize_delta_snapshot(const uint8_t* buffer, size_t buffer_size, DeltaSnapshot& snapshot) {
    size_t header_size = sizeof(uint32_t) * 4;
    if (!buffer || buffer_size < header_size) {
        return 0;
    }
    
    const uint8_t* ptr = buffer;
    std::memcpy(&snapshot.tick, ptr, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    std::memcpy(&snapshot.sequence_id, ptr, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    std::memcpy(&snapshot.reference_tick, ptr, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    std::memcpy(&snapshot.entity_count, ptr, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    if (snapshot.entity_count > MAX_SNAPSHOT_ENTITIES) {
        return 0;
    }
    
    size_t entities_size = snapshot.entity_count * sizeof(DeltaEntitySnapshot);
    size_t required_size = header_size + entities_size;
    if (buffer_size < required_size) {
        return 0;
    }
    
    for (uint32_t i = 0; i < snapshot.entity_count; ++i) {
        std::memcpy(&snapshot.entities[i], ptr, sizeof(DeltaEntitySnapshot));
        ptr += sizeof(DeltaEntitySnapshot);
    }
    
    return required_size;
}

size_t serialize_discovery_packet(const DiscoveryPacket& packet, uint8_t* buffer, size_t buffer_size) {
    if (!buffer || buffer_size < sizeof(DiscoveryPacket)) {
        return 0;
    }

    uint8_t* ptr = buffer;
    std::memcpy(ptr, &packet.header, sizeof(NetworkHeader));
    ptr += sizeof(NetworkHeader);
    
    uint8_t discovery_type_value = static_cast<uint8_t>(packet.discovery_type);
    std::memcpy(ptr, &discovery_type_value, sizeof(uint8_t));
    ptr += sizeof(uint8_t);
    
    std::memcpy(ptr, packet.server_name, MAX_SERVER_NAME_LENGTH);
    ptr += MAX_SERVER_NAME_LENGTH;
    
    std::memcpy(ptr, packet.map_hash, MAP_HASH_LENGTH);
    ptr += MAP_HASH_LENGTH;
    
    write_u32_le(ptr, packet.player_count);
    ptr += sizeof(uint32_t);
    
    write_u32_le(ptr, packet.max_players);
    ptr += sizeof(uint32_t);
    
    write_u32_le(ptr, packet.version_major);
    ptr += sizeof(uint32_t);
    
    write_u32_le(ptr, packet.version_minor);
    ptr += sizeof(uint32_t);
    
    write_u32_le(ptr, packet.version_patch);
    ptr += sizeof(uint32_t);
    
    std::memcpy(ptr, packet.padding, 4);
    
    return sizeof(DiscoveryPacket);
}

size_t deserialize_discovery_packet(const uint8_t* buffer, size_t buffer_size, DiscoveryPacket& packet) {
    if (!buffer || buffer_size < sizeof(DiscoveryPacket)) {
        return 0;
    }

    const uint8_t* ptr = buffer;
    std::memcpy(&packet.header, ptr, sizeof(NetworkHeader));
    ptr += sizeof(NetworkHeader);
    
    uint8_t discovery_type_value = 0;
    std::memcpy(&discovery_type_value, ptr, sizeof(uint8_t));
    ptr += sizeof(uint8_t);
    packet.discovery_type = static_cast<DiscoveryType>(discovery_type_value);
    
    std::memcpy(packet.server_name, ptr, MAX_SERVER_NAME_LENGTH);
    ptr += MAX_SERVER_NAME_LENGTH;
    
    std::memcpy(packet.map_hash, ptr, MAP_HASH_LENGTH);
    ptr += MAP_HASH_LENGTH;
    
    packet.player_count = read_u32_le(ptr);
    ptr += sizeof(uint32_t);
    
    packet.max_players = read_u32_le(ptr);
    ptr += sizeof(uint32_t);
    
    packet.version_major = read_u32_le(ptr);
    ptr += sizeof(uint32_t);
    
    packet.version_minor = read_u32_le(ptr);
    ptr += sizeof(uint32_t);
    
    packet.version_patch = read_u32_le(ptr);
    ptr += sizeof(uint32_t);
    
    std::memcpy(packet.padding, ptr, 4);
    
    return sizeof(DiscoveryPacket);
}

size_t serialize_join_request(const JoinRequest& request, uint8_t* buffer, size_t buffer_size) {
    if (!buffer || buffer_size < sizeof(JoinRequest)) {
        return 0;
    }

    uint8_t* ptr = buffer;
    std::memcpy(ptr, &request.header, sizeof(NetworkHeader));
    ptr += sizeof(NetworkHeader);
    
    std::memcpy(ptr, request.player_name, MAX_SERVER_NAME_LENGTH);
    ptr += MAX_SERVER_NAME_LENGTH;
    
    write_u32_le(ptr, request.version_major);
    ptr += sizeof(uint32_t);
    
    write_u32_le(ptr, request.version_minor);
    ptr += sizeof(uint32_t);
    
    write_u32_le(ptr, request.version_patch);
    ptr += sizeof(uint32_t);
    
    std::memcpy(ptr, request.accepted_map_hash, MAP_HASH_LENGTH);
    ptr += MAP_HASH_LENGTH;
    
    std::memcpy(ptr, request.padding, 4);
    
    return sizeof(JoinRequest);
}

size_t deserialize_join_request(const uint8_t* buffer, size_t buffer_size, JoinRequest& request) {
    if (!buffer || buffer_size < sizeof(JoinRequest)) {
        return 0;
    }

    const uint8_t* ptr = buffer;
    std::memcpy(&request.header, ptr, sizeof(NetworkHeader));
    ptr += sizeof(NetworkHeader);
    
    std::memcpy(request.player_name, ptr, MAX_SERVER_NAME_LENGTH);
    ptr += MAX_SERVER_NAME_LENGTH;
    
    request.version_major = read_u32_le(ptr);
    ptr += sizeof(uint32_t);
    
    request.version_minor = read_u32_le(ptr);
    ptr += sizeof(uint32_t);
    
    request.version_patch = read_u32_le(ptr);
    ptr += sizeof(uint32_t);
    
    std::memcpy(request.accepted_map_hash, ptr, MAP_HASH_LENGTH);
    ptr += MAP_HASH_LENGTH;
    
    std::memcpy(request.padding, ptr, 4);
    
    return sizeof(JoinRequest);
}

size_t serialize_join_response(const JoinResponse& response, uint8_t* buffer, size_t buffer_size) {
    if (!buffer || buffer_size < sizeof(JoinResponse)) {
        return 0;
    }

    uint8_t* ptr = buffer;
    std::memcpy(ptr, &response.header, sizeof(NetworkHeader));
    ptr += sizeof(NetworkHeader);
    
    write_u32_le(ptr, response.slot_id);
    ptr += sizeof(uint32_t);
    
    std::memcpy(ptr, response.accepted_map_hash, MAP_HASH_LENGTH);
    ptr += MAP_HASH_LENGTH;
    
    write_u32_le(ptr, response.tick);
    ptr += sizeof(uint32_t);
    
    uint8_t snapshot_hash_buffer[8];
    std::memcpy(snapshot_hash_buffer, &response.snapshot_hash, sizeof(uint64_t));
    for (int i = 0; i < 8; ++i) {
        ptr[i] = snapshot_hash_buffer[i];
    }
    ptr += sizeof(uint64_t);
    
    std::memcpy(ptr, response.padding, 8);
    
    return sizeof(JoinResponse);
}

size_t deserialize_join_response(const uint8_t* buffer, size_t buffer_size, JoinResponse& response) {
    if (!buffer || buffer_size < sizeof(JoinResponse)) {
        return 0;
    }

    const uint8_t* ptr = buffer;
    std::memcpy(&response.header, ptr, sizeof(NetworkHeader));
    ptr += sizeof(NetworkHeader);
    
    response.slot_id = read_u32_le(ptr);
    ptr += sizeof(uint32_t);
    
    std::memcpy(response.accepted_map_hash, ptr, MAP_HASH_LENGTH);
    ptr += MAP_HASH_LENGTH;
    
    response.tick = read_u32_le(ptr);
    ptr += sizeof(uint32_t);
    
    uint8_t snapshot_hash_buffer[8];
    std::memcpy(snapshot_hash_buffer, ptr, 8);
    std::memcpy(&response.snapshot_hash, snapshot_hash_buffer, sizeof(uint64_t));
    ptr += sizeof(uint64_t);
    
    std::memcpy(response.padding, ptr, 8);
    
    return sizeof(JoinResponse);
}

size_t serialize_connection_handshake(const ConnectionHandshake& handshake, uint8_t* buffer, size_t buffer_size) {
    if (!buffer || buffer_size < sizeof(ConnectionHandshake)) {
        return 0;
    }

	std::memset(buffer, 0, sizeof(ConnectionHandshake));
	uint8_t* ptr = buffer;
    write_u32_le(ptr, handshake.protocol_version);
    ptr += sizeof(uint32_t);
    
    write_u32_le(ptr, handshake.client_version);
    ptr += sizeof(uint32_t);
    
    std::memcpy(ptr, handshake.map_hash, MAP_HASH_LENGTH);
    ptr += MAP_HASH_LENGTH;
    
    write_u32_le(ptr, handshake.content_version_major);
    ptr += sizeof(uint32_t);
    
    write_u32_le(ptr, handshake.content_version_minor);
    ptr += sizeof(uint32_t);
    
	write_u32_le(ptr, handshake.content_version_patch);
	ptr += sizeof(uint32_t);


	std::memcpy(ptr, handshake.visual_pack_id, VISUAL_PACK_ID_LENGTH);
	ptr += VISUAL_PACK_ID_LENGTH;
	write_u32_le(ptr, handshake.visual_pack_version);
	ptr += sizeof(uint32_t);
	std::memcpy(ptr, handshake.visual_pack_hash, VISUAL_PACK_HASH_LENGTH);
	ptr += VISUAL_PACK_HASH_LENGTH;
	std::memcpy(ptr, handshake.padding, 4);
    
    return sizeof(ConnectionHandshake);
}

size_t deserialize_connection_handshake(const uint8_t* buffer, size_t buffer_size, ConnectionHandshake& handshake) {
    if (!buffer || buffer_size < sizeof(ConnectionHandshake)) {
        return 0;
    }

    const uint8_t* ptr = buffer;
    handshake.protocol_version = read_u32_le(ptr);
    ptr += sizeof(uint32_t);
    
    handshake.client_version = read_u32_le(ptr);
    ptr += sizeof(uint32_t);
    
    std::memcpy(handshake.map_hash, ptr, MAP_HASH_LENGTH);
    ptr += MAP_HASH_LENGTH;
    
    handshake.content_version_major = read_u32_le(ptr);
    ptr += sizeof(uint32_t);
    
    handshake.content_version_minor = read_u32_le(ptr);
    ptr += sizeof(uint32_t);
    
	handshake.content_version_patch = read_u32_le(ptr);
	ptr += sizeof(uint32_t);


	std::memcpy(handshake.visual_pack_id, ptr, VISUAL_PACK_ID_LENGTH);
	ptr += VISUAL_PACK_ID_LENGTH;
	handshake.visual_pack_version = read_u32_le(ptr);
	ptr += sizeof(uint32_t);
	std::memcpy(handshake.visual_pack_hash, ptr, VISUAL_PACK_HASH_LENGTH);
	ptr += VISUAL_PACK_HASH_LENGTH;
	std::memcpy(handshake.padding, ptr, 4);
    
    return sizeof(ConnectionHandshake);
}

size_t serialize_frame_command_batch(const FrameCommandBatch& batch, uint8_t* buffer, size_t buffer_size) {
    if (!buffer || buffer_size < sizeof(FrameCommandBatch)) {
        return 0;
    }

    uint8_t* ptr = buffer;
    write_u32_le(ptr, batch.tick);
    ptr += sizeof(uint32_t);
    
    write_u16_le(ptr, batch.command_count);
    ptr += sizeof(uint16_t);
    
    write_u16_le(ptr, batch.padding);
    ptr += sizeof(uint16_t);
    
    for (uint32_t i = 0; i < batch.command_count; ++i) {
        ptr += serialize_input_command(batch.commands[i], ptr, buffer_size - (ptr - buffer));
    }
    
    return sizeof(FrameCommandBatch);
}

size_t deserialize_frame_command_batch(const uint8_t* buffer, size_t buffer_size, FrameCommandBatch& batch) {
    if (!buffer || buffer_size < sizeof(FrameCommandBatch)) {
        return 0;
    }

    const uint8_t* ptr = buffer;
    batch.tick = read_u32_le(ptr);
    ptr += sizeof(uint32_t);
    
    batch.command_count = read_u16_le(ptr);
    ptr += sizeof(uint16_t);
    
    batch.padding = read_u16_le(ptr);
    ptr += sizeof(uint16_t);
    
    for (uint32_t i = 0; i < batch.command_count; ++i) {
        ptr += deserialize_input_command(ptr, buffer_size - (ptr - buffer), batch.commands[i]);
    }
    
    return sizeof(FrameCommandBatch);
}

size_t serialize_snapshot_checksum(const SnapshotChecksum& checksum, uint8_t* buffer, size_t buffer_size) {
    if (!buffer || buffer_size < sizeof(SnapshotChecksum)) {
        return 0;
    }

    uint8_t* ptr = buffer;
    write_u32_le(ptr, checksum.tick);
    ptr += sizeof(uint32_t);
    
    uint8_t snapshot_hash_buffer[8];
    std::memcpy(snapshot_hash_buffer, &checksum.snapshot_hash, sizeof(uint64_t));
    for (int i = 0; i < 8; ++i) {
        ptr[i] = snapshot_hash_buffer[i];
    }
    ptr += sizeof(uint64_t);
    
    std::memcpy(ptr, checksum.padding, 8);
    
    return sizeof(SnapshotChecksum);
}

size_t deserialize_snapshot_checksum(const uint8_t* buffer, size_t buffer_size, SnapshotChecksum& checksum) {
    if (!buffer || buffer_size < sizeof(SnapshotChecksum)) {
        return 0;
    }

    const uint8_t* ptr = buffer;
    checksum.tick = read_u32_le(ptr);
    ptr += sizeof(uint32_t);
    
    uint8_t snapshot_hash_buffer[8];
    std::memcpy(snapshot_hash_buffer, ptr, 8);
    std::memcpy(&checksum.snapshot_hash, snapshot_hash_buffer, sizeof(uint64_t));
    ptr += sizeof(uint64_t);
    
    std::memcpy(checksum.padding, ptr, 8);
    
    return sizeof(SnapshotChecksum);
}

} // namespace rts
