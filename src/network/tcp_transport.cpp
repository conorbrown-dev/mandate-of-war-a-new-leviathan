#include "network/tcp_transport.hpp"
#include "network/serializer.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <algorithm>
#include <chrono>
#include <netdb.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>

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

TcpTransport::TcpTransport() : recv_buffer_(65536) {}

TcpTransport::~TcpTransport() {
    disconnect();
}

bool TcpTransport::connect(const std::string& host, uint16_t port) {
    if (connected_) {
        disconnect();
    }
    
    socket_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (socket_ < 0) {
        return false;
    }
    
    hostent* server = gethostbyname(host.c_str());
    if (!server) {
        ::close(socket_);
        socket_ = -1;
        return false;
    }
    
    sockaddr_in server_addr{};
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = *reinterpret_cast<in_addr_t*>(server->h_addr_list[0]);
    server_addr.sin_port = htons(port);
    
    if (::connect(socket_, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        ::close(socket_);
        socket_ = -1;
        return false;
    }
    
    connected_ = true;
    is_server_ = false;
    // Client and server use the same active socket abstraction below.
    client_socket_ = socket_;
    bytes_sent_ = 0;
    bytes_received_ = 0;
    last_activity_time_ = 0.0f;
    
    return true;
}

bool TcpTransport::listen(uint16_t port) {
    if (connected_) {
        disconnect();
    }
    
    socket_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (socket_ < 0) {
        return false;
    }
    
    int opt = 1;
    setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    sockaddr_in server_addr{};
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (::bind(socket_, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        ::close(socket_);
        socket_ = -1;
        return false;
    }
    
    if (::listen(socket_, 1) < 0) {
        ::close(socket_);
        socket_ = -1;
        return false;
    }
    
    is_server_ = true;
    client_socket_ = -1;
    
    return true;
}

bool TcpTransport::accept() {
    if (!is_server_ || socket_ < 0) {
        return false;
    }
    
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    
    client_socket_ = ::accept(socket_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
    if (client_socket_ < 0) {
        return false;
    }
    
    connected_ = true;
    bytes_sent_ = 0;
    bytes_received_ = 0;
    last_activity_time_ = 0.0f;
    
    return true;
}

void TcpTransport::disconnect() {
    if (client_socket_ >= 0) {
        ::close(client_socket_);
        client_socket_ = -1;
    }
    
    if (socket_ >= 0 && is_server_) {
        ::close(socket_);
        socket_ = -1;
    }
    
    connected_ = false;
    is_server_ = false;
    socket_ = -1;
    recv_buffer_pos_ = 0;
    recv_buffer_count_ = 0;
}

bool TcpTransport::send_all(const uint8_t* data, size_t size) {
    size_t sent = 0;
    while (sent < size) {
        ssize_t n = ::send(client_socket_, data + sent, size - sent, MSG_NOSIGNAL);
        if (n <= 0) {
            return false;
        }
        sent += static_cast<size_t>(n);
    }
    bytes_sent_ += size;
    return true;
}

bool TcpTransport::recv_all(uint8_t* data, size_t size) {
    size_t received = 0;
    while (received < size) {
        ssize_t n = ::recv(client_socket_, data + received, size - received, 0);
        if (n <= 0) {
            return false;
        }
        received += static_cast<size_t>(n);
    }
    bytes_received_ += size;
    return true;
}

bool TcpTransport::send_handshake(const ConnectionHandshake& handshake) {
    if (!connected_ || is_server_) {
        return false;
    }
    
    uint8_t buffer[sizeof(ConnectionHandshake)];
    size_t serialized = serialize_connection_handshake(handshake, buffer, sizeof(buffer));
    if (serialized == 0) {
        return false;
    }
    
    uint8_t header_buffer[8];
    write_u32_le(header_buffer, static_cast<uint32_t>(TransportType::HANDSHAKE));
    write_u32_le(header_buffer + 4, static_cast<uint32_t>(serialized));
    
    if (!send_all(header_buffer, 8)) {
        return false;
    }
    
    return send_all(buffer, serialized);
}

bool TcpTransport::recv_handshake(ConnectionHandshake& handshake) {
    if (!connected_ || !is_server_) {
        return false;
    }
    
    uint8_t header[8];
    if (!recv_all(header, 8)) {
        return false;
    }
    
    uint32_t type = read_u32_le(header);
    if (static_cast<TransportType>(type) != TransportType::HANDSHAKE) {
        return false;
    }
    
    uint32_t size = read_u32_le(header + 4);
    if (size > sizeof(ConnectionHandshake)) {
        return false;
    }
    
    uint8_t buffer[sizeof(ConnectionHandshake)];
    if (!recv_all(buffer, size)) {
        return false;
    }
    
    return deserialize_connection_handshake(buffer, size, handshake) == size;
}

bool TcpTransport::send_frame_command_batch(const FrameCommandBatch& batch) {
    if (!connected_) {
        return false;
    }
    
    uint8_t buffer[sizeof(FrameCommandBatch)];
    size_t serialized = serialize_frame_command_batch(batch, buffer, sizeof(buffer));
    if (serialized == 0) {
        return false;
    }
    
    uint8_t header[8];
    write_u32_le(header, static_cast<uint32_t>(TransportType::FRAME_COMMAND_BATCH));
    write_u32_le(header + 4, static_cast<uint32_t>(serialized));
    
    if (!send_all(header, 8)) {
        return false;
    }
    
    return send_all(buffer, serialized);
}

bool TcpTransport::recv_frame_command_batch(FrameCommandBatch& batch) {
    if (!connected_) {
        return false;
    }
    
    uint8_t header[8];
    if (!recv_all(header, 8)) {
        return false;
    }
    
    uint32_t type = read_u32_le(header);
    if (static_cast<TransportType>(type) != TransportType::FRAME_COMMAND_BATCH) {
        return false;
    }
    
    uint32_t size = read_u32_le(header + 4);
    if (size > sizeof(FrameCommandBatch)) {
        return false;
    }
    
    uint8_t buffer[sizeof(FrameCommandBatch)];
    if (!recv_all(buffer, size)) {
        return false;
    }
    
    return deserialize_frame_command_batch(buffer, size, batch) == size;
}

bool TcpTransport::send_snapshot_checksum(const SnapshotChecksum& checksum) {
    if (!connected_) {
        return false;
    }
    
    uint8_t buffer[sizeof(SnapshotChecksum)];
    size_t serialized = serialize_snapshot_checksum(checksum, buffer, sizeof(buffer));
    if (serialized == 0) {
        return false;
    }
    
    uint8_t header[8];
    write_u32_le(header, static_cast<uint32_t>(TransportType::SNAPSHOT_CHECKSUM));
    write_u32_le(header + 4, static_cast<uint32_t>(serialized));
    
    if (!send_all(header, 8)) {
        return false;
    }
    
    return send_all(buffer, serialized);
}

bool TcpTransport::recv_snapshot_checksum(SnapshotChecksum& checksum) {
    if (!connected_) {
        return false;
    }
    
    uint8_t header[8];
    if (!recv_all(header, 8)) {
        return false;
    }
    
    uint32_t type = read_u32_le(header);
    if (static_cast<TransportType>(type) != TransportType::SNAPSHOT_CHECKSUM) {
        return false;
    }
    
    uint32_t size = read_u32_le(header + 4);
    if (size > sizeof(SnapshotChecksum)) {
        return false;
    }
    
    uint8_t buffer[sizeof(SnapshotChecksum)];
    if (!recv_all(buffer, size)) {
        return false;
    }
    
    return deserialize_snapshot_checksum(buffer, size, checksum) == size;
}

size_t TcpTransport::available() const {
    if (client_socket_ < 0) {
        return 0;
    }
    
    int bytes = 0;
    if (ioctl(client_socket_, FIONREAD, &bytes) < 0) {
        return 0;
    }
    
    return static_cast<size_t>(bytes);
}

size_t TcpTransport::read(uint8_t* buffer, size_t size) {
    if (client_socket_ < 0) {
        return 0;
    }
    
    ssize_t n = ::recv(client_socket_, buffer, size, MSG_DONTWAIT);
    if (n <= 0) {
        return 0;
    }
    
    bytes_received_ += static_cast<size_t>(n);
    return static_cast<size_t>(n);
}

size_t TcpTransport::write(const uint8_t* data, size_t size) {
    if (client_socket_ < 0) {
        return 0;
    }
    
    ssize_t n = ::send(client_socket_, data, size, MSG_NOSIGNAL);
    if (n <= 0) {
        return 0;
    }
    
    bytes_sent_ += static_cast<size_t>(n);
    return static_cast<size_t>(n);
}

bool TcpTransport::poll(float timeout_ms) {
    if (client_socket_ < 0) {
        return false;
    }
    
    timeval tv;
    tv.tv_sec = static_cast<long>(timeout_ms / 1000.0f);
    tv.tv_usec = static_cast<long>((timeout_ms - tv.tv_sec * 1000.0f) * 1000.0f);
    
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(client_socket_, &read_fds);
    
    int ret = select(client_socket_ + 1, &read_fds, nullptr, nullptr, &tv);
    return ret > 0;
}

void TcpTransport::update(float delta_ms) {
    last_activity_time_ += delta_ms;
}

void TcpTransport::reset() {
    disconnect();
    connected_ = false;
    is_server_ = false;
    socket_ = -1;
    client_socket_ = -1;
    recv_buffer_pos_ = 0;
    recv_buffer_count_ = 0;
    bytes_sent_ = 0;
    bytes_received_ = 0;
    last_activity_time_ = 0.0f;
}

} // namespace rts
