#include "server.hpp"
#include "packet.hpp"
#include "network.hpp"
#include "log.hpp"
#include "../config.hpp"
#include <chrono>

std::atomic<bool> gServerRunning{true};

CoopServer::CoopServer(int p) : port(p) {}

void CoopServer::runLoop() {
    if (!networkInit((NetworkSystemType)gServerConfig.networkSystem, port)) {
        Logging::log("SERVER", "failed to initialize network system");
        return;
    }

    while (gServerRunning) {
        if (gNetworkSystem) {
            gNetworkSystem->update();
        }
    }

    networkShutdown();
}