#include "server.hpp"
#include "packet.hpp"
#include "network.hpp"
#include "log.hpp"
#include "../config.hpp"
#include <chrono>

std::atomic<bool> gServerRunning{true};

Server::Server(int p) : port(p) {}

void Server::run() {
    if (!networkInit((NetworkSystemType)gServerConfig.networkSystem, port)) {
        Logging::error("SERVER", "failed to initialize network system");
        return;
    }

    while (gServerRunning) {
        if (gNetworkSystem) {
            gNetworkSystem->update();
        }
    }

    networkShutdown();
}