#include "server.hpp"
#include "packet.hpp"
#include "../lua/smlua.hpp"
#include "network.hpp"
#include "log.hpp"
#include "../config.hpp"
#include <chrono>

std::atomic<bool> gServerRunning{true};
const double targetSMLuaFPS = 30.0;
const double SMLuaFrameDuration = 1.0 / targetSMLuaFPS;

Server::Server(int p) : port(p) {}

void Server::run(void) {
    if (!networkInit((NetworkSystemType)gServerConfig.networkSystem, port)) {
        Logging::error("SERVER", "failed to initialize network system");
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