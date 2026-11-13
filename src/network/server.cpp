#include "server.hpp"
#include "packet.hpp"
#include "../lua/smlua.hpp"
#include "network.hpp"
#include "log.hpp"
#include "../config.hpp"
#include <chrono>
#include <mutex>
#include <thread>

Server::Server(int p) : port(p) {}

std::atomic<bool> gServerRunning{true};
std::mutex gNetworkMutex;
const double targetSMLuaFPS = 30.0;
const double SMLuaFrameDuration = 1.0 / targetSMLuaFPS;

static void receiveLoop(void) {
    while (gServerRunning) {
        {
            std::lock_guard<std::mutex> lock(gNetworkMutex);
            if (gNetworkSystem) gNetworkSystem->poll();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

static void luaLoop(void) {
    const auto frame = std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(SMLuaFrameDuration));

    auto next = std::chrono::steady_clock::now();
    while (gServerRunning) {
        {
            std::lock_guard<std::mutex> lock(gNetworkMutex);
            gSMLua.update();
        }

        next += frame;
        auto now = std::chrono::steady_clock::now();
        if (now - next > frame * 5) {
            next = now;
        }
        std::this_thread::sleep_until(next);
    }
}

void Server::run(void) {
    if (!networkInit((NetworkSystemType)gServerConfig.networkSystem, port)) {
        Logging::error("SERVER", "failed to initialize network system");
        return;
    }

    std::thread receiveThread(receiveLoop);
    std::thread luaThread(luaLoop);

    while (gServerRunning) {
        {
            std::lock_guard<std::mutex> lock(gNetworkMutex);
            if (gNetworkSystem) gNetworkSystem->updateNetwork();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    luaThread.join();
    receiveThread.join();
    networkShutdown();
}