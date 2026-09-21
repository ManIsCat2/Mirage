#include "server.hpp"
#include "packet.hpp"
#include "network.hpp"
#include "log.hpp"
#include "../lua/smlua.hpp"
#include "../config.hpp"
#include <chrono>

std::atomic<bool> gServerRunning{true};

const double targetSMLuaFPS = 30.0;
const double SMLuaFrameDuration = 1.0 / targetSMLuaFPS;

CoopServer::CoopServer(int p) : port(p) {}

void CoopServer::runLoop() {
    NetworkSystemType type = (gServerConfig.networkSystem == 1) ? SYS_COOPNET : SYS_SOCKET;
    if (!networkInit(type, port)) {
        Logging::log("SERVER", "Failed to initialize network system!");
        return;
    }

    auto lastTime = std::chrono::high_resolution_clock::now();
    double accumulator = 0.0;

    while (gServerRunning) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = currentTime - lastTime;
        lastTime = currentTime;
        accumulator += elapsed.count();

        if (gNetworkSystem) {
            gNetworkSystem->update();
        }

        while (accumulator >= SMLuaFrameDuration) {
            gSMLua.update();
            accumulator -= SMLuaFrameDuration;
        }
    }

    networkShutdown();
}