#pragma once

// The one place that knows the difference between POSIX sockets and Winsock.
// Sockets stay plain ints everywhere else. Winsock handles fit in 32 bits, and
// its invalid handle truncates to -1, so the existing < 0 checks keep working.

#include <cstddef>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <basetsd.h>
using ssize_t = SSIZE_T;
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace rts::net {

inline int open_socket(int family, int type) {
#if defined(_WIN32)
    static const bool started = [] {
        WSADATA data;
        return WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }();
    if (!started) {
        return -1;
    }
    return static_cast<int>(::socket(family, type, 0));
#else
    return ::socket(family, type, 0);
#endif
}

inline void close_socket(int socket) {
#if defined(_WIN32)
    ::closesocket(static_cast<SOCKET>(socket));
#else
    ::close(socket);
#endif
}

inline void set_nonblocking(int socket) {
#if defined(_WIN32)
    u_long enabled = 1;
    ::ioctlsocket(static_cast<SOCKET>(socket), FIONBIO, &enabled);
#else
    ::fcntl(socket, F_SETFL, ::fcntl(socket, F_GETFL, 0) | O_NONBLOCK);
#endif
}

inline bool enable_option(int socket, int option) {
    const int enabled = 1;
    return ::setsockopt(socket, SOL_SOCKET, option, reinterpret_cast<const char*>(&enabled),
                        sizeof(enabled)) == 0;
}

// A closed peer returns an error rather than raising SIGPIPE.
inline ssize_t send_bytes(int socket, const void* data, std::size_t size) {
#if defined(_WIN32)
    return ::send(static_cast<SOCKET>(socket), static_cast<const char*>(data),
                  static_cast<int>(size), 0);
#else
    return ::send(socket, data, size, MSG_NOSIGNAL);
#endif
}

inline ssize_t recv_bytes(int socket, void* data, std::size_t size) {
#if defined(_WIN32)
    return ::recv(static_cast<SOCKET>(socket), static_cast<char*>(data), static_cast<int>(size), 0);
#else
    return ::recv(socket, data, size, 0);
#endif
}

inline std::size_t bytes_available(int socket) {
#if defined(_WIN32)
    u_long bytes = 0;
    return ::ioctlsocket(static_cast<SOCKET>(socket), FIONREAD, &bytes) == 0 ? bytes : 0;
#else
    int bytes = 0;
    return ::ioctl(socket, FIONREAD, &bytes) == 0 && bytes > 0 ? static_cast<std::size_t>(bytes) : 0;
#endif
}

// Returns at once with what is already queued, or a value <= 0 if nothing is.
inline ssize_t recv_available(int socket, void* data, std::size_t size) {
#if defined(_WIN32)
    const std::size_t queued = bytes_available(socket);
    return queued == 0 ? 0 : recv_bytes(socket, data, queued < size ? queued : size);
#else
    return ::recv(socket, data, size, MSG_DONTWAIT);
#endif
}

inline ssize_t send_datagram(int socket, const void* data, std::size_t size, const sockaddr_in& target) {
#if defined(_WIN32)
    return ::sendto(static_cast<SOCKET>(socket), static_cast<const char*>(data), static_cast<int>(size), 0,
                    reinterpret_cast<const sockaddr*>(&target), sizeof(target));
#else
    return ::sendto(socket, data, size, 0, reinterpret_cast<const sockaddr*>(&target), sizeof(target));
#endif
}

inline ssize_t recv_datagram(int socket, void* data, std::size_t size, sockaddr_in& sender) {
    socklen_t sender_size = sizeof(sender);
#if defined(_WIN32)
    return ::recvfrom(static_cast<SOCKET>(socket), static_cast<char*>(data), static_cast<int>(size), 0,
                      reinterpret_cast<sockaddr*>(&sender), &sender_size);
#else
    return ::recvfrom(socket, data, size, 0, reinterpret_cast<sockaddr*>(&sender), &sender_size);
#endif
}

} // namespace rts::net
