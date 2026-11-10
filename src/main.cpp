#include <cstring>
#include <csignal>
#include "config.hpp"
#include "log.hpp"
#include "network/network.hpp"
#include "network/server.hpp"
#include "cxxopts.hpp"
#include "savefile.hpp"

static void handleSignal(int sig) {
    gServerRunning.store(false);
}

int main(int argc, char *argv[]) {
    cxxopts::Options options("Mirage", "Headless server for sm64coopdx");
    options.add_options()
        ("config", "Path to config file", cxxopts::value<std::string>()->default_value(SERVER_CONFIGFILE))
        ("write-config", "Write default config to file and exit", cxxopts::value<bool>()->default_value("false"))
        ("h,help", "Show this help");

    cxxopts::ParseResult result;
    try {
        result = options.parse(argc, argv);
    } catch (const cxxopts::exceptions::exception &e) {
        Logging::error("MIRAGE", "{}\n{}", e.what(), options.help());
        return 1;
    }

    if (result.count("help")) {
        std::cout << options.help() << "\n";
        return 0;
    }

    std::string configPath = result["config"].as<std::string>();
    const bool writeConfig = result["write-config"].as<bool>();

    if (writeConfig) {
        gServerConfig.write(configPath);
        return 0;
    }

    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    const bool firstRun = !std::filesystem::exists(configPath);
    if (!firstRun) {
        gServerConfig.read(configPath);
    }

    gNetworkPlayers[0].type = NetworkPlayerType::LOCAL;
    gNetworkPlayers[0].globalIndex = 0;
    gNetworkPlayers[0].connected = true;
    gNetworkPlayers[0].name = MIRAGE_NAME;
    gNetworkPlayers[0].currLevelNum = 0;
    gNetworkPlayers[0].currAreaIndex = 0;

    gSaveFile.setIndex(gServerConfig.savefileIndex);
    gSaveFile.load();

    Server server(gServerConfig.port);
    server.run();

    if (firstRun) {
        gServerConfig.write(configPath);
    }

    return 0;
}