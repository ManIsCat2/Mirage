#include "config.hpp"
#include "json.hpp"
#include "log.hpp"
#include "network/network.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>

using json = nlohmann::json;

ServerConfig gServerConfig{};

template <typename T>
static std::vector<T> readArray(const json &data, const char *key) {
    std::vector<T> result;
    if (auto it = data.find(key); it != data.end() && it->is_array()) {
        for (const auto &item : *it) {
            result.push_back(item.get<T>());
        }
    }
    return result;
}

static std::string toLower(std::string text) {
    // wtf is this :pray:
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

static bool isValidNetworkSystem(int id) {
    return id == SYS_SOCKET || id == SYS_COOPNET || id == SYS_BOTH;
}

static int parseNetworkSystem(const json &value) {
    if (value.is_number_integer()) {
        int id = value.get<int>();
        if (isValidNetworkSystem(id)) {
            return id;
        }
    } else if (value.is_string()) {
        std::string text = toLower(value.get<std::string>());

        if (text == "socket") return SYS_SOCKET;
        if (text == "coopnet") return SYS_COOPNET;
        if (text == "hybrid" || text == "both") return SYS_BOTH;

        if (!text.empty() && std::all_of(text.begin(), text.end(), isdigit)) {
            int id = std::stoi(text);
            if (isValidNetworkSystem(id)) {
                return id;
            }
        }
    }

    Logging::warn("CONFIG", "unknown network system {}, defaulting to socket", value.dump());
    return SYS_SOCKET;
}

std::string networkSystemName(int id) {
    switch (id) {
        case SYS_COOPNET: return "coopnet";
        case SYS_BOTH:    return "hybrid";
        default:          return "socket";
    }
}

void ServerConfig::read(const std::string &filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        write(filename);
        return;
    }

    json data = json::parse(file, nullptr, false);
    if (data.is_discarded() || !data.is_object()) {
        Logging::warn("CONFIG", "could not parse {}, using defaults", filename);
        return;
    }

    if (data.contains("networkSystem")) {
        networkSystem = parseNetworkSystem(data["networkSystem"]);
    }

    port = data.value("port", port);
    version = data.value("version", version);
    name = data.value("name", name);
    game = data.value("game", game);
    mode = data.value("mode", mode);
    description = data.value("description", description);
    password = data.value("password", password);

    savefileIndex = data.value("savefileIndex", savefileIndex);
    playerInteractions = data.value("playerInteractions", playerInteractions);
    bouncyBounds = data.value("bouncyBounds", bouncyBounds);
    knockStrength = data.value("knockStrength", knockStrength);
    starStaying = data.value("starStaying", starStaying);
    skipIntro = data.value("skipIntro", skipIntro);
    bubbleDeath = data.value("bubbleDeath", bubbleDeath);
    pauseAnywhere = data.value("pauseAnywhere", pauseAnywhere);
    nametags = data.value("nametags", nametags);
    maxPlayers = data.value("maxPlayers", maxPlayers);

    gui = data.value("gui", gui);

    whitelist = readArray<int64_t>(data, "whitelist");
    blacklist = readArray<int64_t>(data, "blacklist");

    mods.clear();
    for (const auto &entry : readArray<std::string>(data, "mods")) {
        CoopMod mod;
        if (mod.load(entry)) {
            Logging::info("CONFIG", "loaded mod {}", mod.name);
            mods.push_back(mod);
        } else {
            Logging::error("CONFIG", "failed to load mod at {}", entry);
        }
    }
}

void ServerConfig::write(const std::string &filename) {
    json modPaths = json::array();
    for (const auto &mod : mods) {
        modPaths.push_back(mod.basePath);
    }

    const json data = {
        {"port", port},
        {"networkSystem", networkSystemName(networkSystem)},
        {"version", version},
        {"game", game},
        {"name", name},
        {"mode", mode},
        {"description", description},
        {"password", password},
        {"savefileIndex", savefileIndex},
        {"playerInteractions", playerInteractions},
        {"bouncyBounds", bouncyBounds},
        {"knockStrength", knockStrength},
        {"starStaying", starStaying},
        {"skipIntro", skipIntro},
        {"bubbleDeath", bubbleDeath},
        {"nametags", nametags},
        {"maxPlayers", maxPlayers},
        {"pauseAnywhere", pauseAnywhere},
        {"mods", modPaths},
        {"whitelist", whitelist},
        {"blacklist", blacklist},
        {"gui", gui},
    };

    std::ofstream file(filename);
    file << data.dump(4) << '\n';
}