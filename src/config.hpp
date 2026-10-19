#pragma once

#include "mod.hpp"
#include "network/network.hpp"

#include <cstdint>
#include <string>
#include <vector>

inline const std::string SERVER_CONFIGFILE = "config.json";
inline const std::string MIRAGE_NAME = "\\#e86100\\Mirage";

class ServerConfig {
public:
    int port = 7777;
    int networkSystem = SYS_SOCKET;

    std::string version = "v1.5.1";
    std::string name = MIRAGE_NAME;
    std::string game = "sm64coopdx";
    std::string mode;
    std::string description;
    std::string password;

    int savefileIndex = 1;
    int playerInteractions = 1;
    int bouncyBounds = 0;
    int knockStrength = 25;
    int starStaying = 1;
    int skipIntro = 1;
    int bubbleDeath = 1;
    int pauseAnywhere = 1;
    int nametags = 1;
    int maxPlayers = MAX_PLAYERS;

    bool gui = false;

    std::vector<CoopMod> mods;
    std::vector<int64_t> whitelist;
    std::vector<int64_t> blacklist;

    void read(const std::string &filename);
    void write(const std::string &filename);
};

extern ServerConfig gServerConfig;
