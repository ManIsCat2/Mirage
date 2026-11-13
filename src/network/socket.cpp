#include "socket.hpp"
#include "log.hpp"
#include <iostream>
#include <cstdlib>

bool sockaddrInEqual(const sockaddr_in &a, const sockaddr_in &b) {
    return a.sin_family == b.sin_family &&
           a.sin_port == b.sin_port &&
           a.sin_addr.s_addr == b.sin_addr.s_addr;
}

UDPSocket::UDPSocket(int port) {
#ifdef _WIN32
    WSADATA wsaData;
    int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (res != 0) {
        Logging::error("SERVER", "WSAStartup failed: {}", res);
        exit(1);
    }
#endif

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        Logging::error("SERVER", "failed to create socket");
        exit(1);
    }

#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        Logging::error("SERVER", "failed to bind port");
        exit(1);
    }
}

UDPSocket::~UDPSocket() {
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
    }
#ifdef _WIN32
    WSACleanup();
#endif
}

int UDPSocket::getSock() const {
    return sock;
}

ssize_t UDPSocket::receive(uint8_t *buffer, size_t maxLen, sockaddr_in &clientAddr) {
    socklen_t clientLen = sizeof(clientAddr);
    return recvfrom(sock, reinterpret_cast<char*>(buffer), maxLen, 0, (struct sockaddr*)&clientAddr, &clientLen);
}