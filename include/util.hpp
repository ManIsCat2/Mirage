#pragma once

#include <chrono>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#undef WIN32_LEAN_AND_MEAN
typedef SOCKET socket_t;
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
typedef int socket_t;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

constexpr int MAX_PLAYERS = 16;

inline double clockElapsedNano(void) {
    static const auto start = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count();
}

inline double clockElapsed() {
    return clockElapsedNano() / 1000000000.0;
}

inline std::string stringToLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

inline bool getBit(uint8_t val, uint8_t num) {
    return (val >> num) & 1;
}

inline bool isValidGlobalIndex(int idx) {
    return idx >= 0 && idx < MAX_PLAYERS;
}

inline std::string getIPString(const sockaddr_in *addr) {
    if (addr == nullptr) return "0.0.0.0:7777";
    char buffer[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr->sin_addr, buffer, sizeof(buffer));
    uint16_t port = ntohs(addr->sin_port);

    return std::format("{}:{}", buffer, port);
}