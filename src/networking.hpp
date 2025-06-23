// Cross-platform networking lib (based on unix socket interface)
#ifndef NETWORKING_HPP
#define NETWORKING_HPP

// Add required standard library includes
#include <cstdint>
#include <string>
#include <iostream>

#if __unix__
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#elif _WIN32
// Prevent Windows headers from defining conflicting macros
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
// Prevent specific Windows API conflicts with Raylib
#define NOGDI     // Prevents wingdi.h from being included (Rectangle conflict)
#define NOUSER    // Prevents winuser.h from being included (DrawText, CloseWindow conflicts)
#define NOMCX     // Prevents mmsystem.h from being included (PlaySound conflict)
#include <winsock2.h>
#include <ws2tcpip.h> 
#pragma comment(lib, "ws2_32.lib")
#endif

// Platform-specific error handling
#if _WIN32
#define GET_LAST_ERROR WSAGetLastError()
#define SOCKET_ERROR_VAL SOCKET_ERROR

// Windows-specific error reporting function
inline void print_socket_error(const char* message) {
    int error_code = WSAGetLastError();
    std::cerr << message << ": WSA Error " << error_code;
    
    // Common WSA error codes
    switch(error_code) {
        case WSAECONNREFUSED:
            std::cerr << " (Connection refused)";
            break;
        case WSAENETUNREACH:
            std::cerr << " (Network unreachable)";
            break;
        case WSAETIMEDOUT:
            std::cerr << " (Connection timed out)";
            break;
        case WSAEHOSTUNREACH:
            std::cerr << " (Host unreachable)";
            break;
        case WSAEADDRINUSE:
            std::cerr << " (Address already in use)";
            break;
        case WSAEADDRNOTAVAIL:
            std::cerr << " (Address not available)";
            break;
        default:
            break;
    }
    std::cerr << std::endl;
}
#else
#define GET_LAST_ERROR errno
#define SOCKET_ERROR_VAL -1

inline void print_socket_error(const char* message) {
    perror(message);
}
#endif

// Socket creation
inline int create_socket(int domain, int type, int protocol) {
#if __unix__
    return socket(domain, type, protocol);
#elif _WIN32
    return (int)WSASocket(domain, type, protocol, NULL, 0, WSA_FLAG_OVERLAPPED);
#else
    return -1; // Not implemented for other systems
#endif
}

// Socket binding
inline int bind_socket(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
#if __unix__
    return bind(sockfd, addr, addrlen);
#elif _WIN32
    return bind((SOCKET)sockfd, addr, addrlen);
#else
    return -1;
#endif
}

// Socket listening
inline int listen_socket(int sockfd, int backlog) {
#if __unix__
    return listen(sockfd, backlog);
#elif _WIN32
    return listen((SOCKET)sockfd, backlog);
#else
    return -1;
#endif
}

// Socket accepting
inline int accept_connection(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
#if __unix__
    return accept(sockfd, addr, addrlen);
#elif _WIN32
    return (int)accept((SOCKET)sockfd, addr, addrlen);
#else
    return -1;
#endif
}

// Socket connecting
inline int connect_socket(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
#if __unix__
    return connect(sockfd, addr, addrlen);
#elif _WIN32
    return connect((SOCKET)sockfd, addr, addrlen);
#else
    return -1;
#endif
}

// Socket sending
inline int send_data(int sockfd, const void *buf, size_t len, int flags) {
#if __unix__
    return send(sockfd, buf, len, flags);
#elif _WIN32
    return send((SOCKET)sockfd, (const char*)buf, (int)len, flags);
#else
    return -1;
#endif
}

// Socket receiving
inline int recv_data(int sockfd, void *buf, size_t len, int flags) {
#if __unix__
    return recv(sockfd, buf, len, flags);
#elif _WIN32
    return recv((SOCKET)sockfd, (char*)buf, (int)len, flags);
#else
    return -1;
#endif
}

// Socket shutdown
inline int shutdown_socket(int sockfd, int how) {
#if __unix__
    return shutdown(sockfd, how);
#elif _WIN32
    return shutdown((SOCKET)sockfd, how);
#else
    return -1;
#endif
}

// Socket closing
inline int close_socket(int sockfd) {
#if __unix__
    return close(sockfd);
#elif _WIN32
    return closesocket((SOCKET)sockfd);
#else
    return -1;
#endif
}

// Socket options
inline int set_socket_option(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
#if __unix__
    return setsockopt(sockfd, level, optname, optval, optlen);
#elif _WIN32
    return setsockopt((SOCKET)sockfd, level, optname, (const char*)optval, (int)optlen);
#else
    return -1;
#endif
}

// Address conversion
inline unsigned long inet_address(const char *cp) {
#if __unix__
    return inet_addr(cp);
#elif _WIN32
    return inet_addr(cp);
#else
    return INADDR_NONE;
#endif
}

// Host to network short
inline uint16_t host_to_network_short(uint16_t hostshort) {
#if __unix__ || _WIN32
    return htons(hostshort);
#else
    return hostshort;
#endif
}

// Network to host short
inline uint16_t network_to_host_short(uint16_t netshort) {
#if __unix__ || _WIN32
    return ntohs(netshort);
#else
    return netshort;
#endif
}

// Host to network long
inline uint32_t host_to_network_long(uint32_t hostlong) {
#if __unix__ || _WIN32
    return htonl(hostlong);
#else
    return hostlong;
#endif
}

// Network to host long
inline uint32_t network_to_host_long(uint32_t netlong) {
#if __unix__ || _WIN32
    return ntohl(netlong);
#else
    return netlong;
#endif
}

// IP String to Socket Address
inline uint32_t ip_string_to_binary(const char* ipStr) {
#if defined(_WIN32)
    in_addr addr;
    // Windows Vista+ supports inet_pton
    if (inet_pton(AF_INET, ipStr, &addr) == 1) {
        return addr.s_addr;
    } else {
        // Fallback to inet_addr (returns INADDR_NONE on failure)
        return inet_addr(ipStr);
    }
#else
    in_addr addr;
    if (inet_pton(AF_INET, ipStr, &addr) == 1) {
        return addr.s_addr;
    } else {
        return INADDR_NONE;
    }
#endif
}

// Socket Address to IP String
inline std::string ip_binary_to_string(uint32_t ip) {
    char buf[INET_ADDRSTRLEN];
    const char* result = inet_ntop(AF_INET, &ip, buf, INET_ADDRSTRLEN);
    return result ? std::string(result) : std::string();
}


// Socket address structures
#if __unix__ || _WIN32
using socket_address_in = sockaddr_in;
using socket_address = sockaddr;
#endif

// Constants
#if __unix__
constexpr int SOCKET_STREAM = SOCK_STREAM;
constexpr int SOCKET_DGRAM = SOCK_DGRAM;
constexpr int ADDRESS_FAMILY_INET = AF_INET;
constexpr int ADDRESS_FAMILY_INET6 = AF_INET6;
constexpr int SOCKET_LEVEL = SOL_SOCKET;
constexpr int SOCKET_REUSEADDR = SO_REUSEADDR;
constexpr int SOCKET_REUSEPORT = SO_REUSEPORT; // Not directly available on Winsock for individual sockets without specific API calls or options that mimic it
constexpr int SHUTDOWN_READ = SHUT_RD;
constexpr int SHUTDOWN_WRITE = SHUT_WR;
constexpr int SHUTDOWN_BOTH = SHUT_RDWR;
constexpr uint32_t ADDRESS_ANY = INADDR_ANY;
constexpr uint32_t ADDRESS_NONE = INADDR_NONE;
#elif _WIN32
constexpr int SOCKET_STREAM = SOCK_STREAM;
constexpr int SOCKET_DGRAM = SOCK_DGRAM;
constexpr int ADDRESS_FAMILY_INET = AF_INET;
constexpr int ADDRESS_FAMILY_INET6 = AF_INET6;
constexpr int SOCKET_LEVEL = SOL_SOCKET;
constexpr int SOCKET_REUSEADDR = SO_REUSEADDR;
constexpr int SOCKET_REUSEPORT = 0; // Winsock doesn't have a direct SO_REUSEPORT equivalent on individual sockets.
                                    // Generally, SO_REUSEADDR handles most scenarios.
constexpr int SHUTDOWN_READ = SD_RECEIVE;
constexpr int SHUTDOWN_WRITE = SD_SEND;
constexpr int SHUTDOWN_BOTH = SD_BOTH;
constexpr uint32_t ADDRESS_ANY = INADDR_ANY;
constexpr uint32_t ADDRESS_NONE = INADDR_NONE;
#else
// Generic fallbacks if neither Unix nor Windows is detected.
// These values might not be universally correct, but provide a compile-time default.
constexpr int SOCKET_STREAM = 1;
constexpr int SOCKET_DGRAM = 2;
constexpr int ADDRESS_FAMILY_INET = 2;
constexpr int ADDRESS_FAMILY_INET6 = 10;
constexpr int SOCKET_LEVEL = 1;
constexpr int SOCKET_REUSEADDR = 2;
constexpr int SOCKET_REUSEPORT = 15;
constexpr int SHUTDOWN_READ = 0;
constexpr int SHUTDOWN_WRITE = 1;
constexpr int SHUTDOWN_BOTH = 2;
constexpr uint32_t ADDRESS_ANY = 0;
constexpr uint32_t ADDRESS_NONE = 0xFFFFFFFF;
#endif

#ifdef _WIN32
// Winsock initialization and cleanup
// These functions should be called once at the start and end of your application.
inline int initialize_winsock() {
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData);
}

inline int cleanup_winsock() {
    return WSACleanup();
}
#endif

#endif // NETWORKING_HPP