#include "smlua.hpp"
#include "miragelib.hpp"
#include "../config.hpp"
#include "constants.hpp"
#include "log.hpp"
#include "structs.hpp"
#include "globals.hpp"
#include "util.hpp"
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

SMLua gSMLua;

SMLua::SMLua() {
    init();
}

SMLua::~SMLua() {
    shutdown();
}

bool SMLua::init() {
    if (L) return true;
    L = luaL_newstate();
    if (!L) return false;

    luaopen_base(L);
    luaL_requiref(L, "io", luaopen_io, 1);
    luaL_requiref(L, "math", luaopen_math, 1);
    luaL_requiref(L, "string", luaopen_string, 1);
    luaL_requiref(L, "table", luaopen_table, 1);
    luaL_requiref(L, "coroutine", luaopen_coroutine, 1);
    luaL_requiref(L, "utf8", luaopen_utf8, 1);
    luaL_requiref(L, "mirage", luaopen_mirage, 1);

    lua_register(L, "hook_event", [](lua_State *L) {
        int hookType = luaL_checkinteger(L, 1);
        if (!lua_isfunction(L, 2)) {
            return luaL_error(L, "hook_event expects a function as the second argument");
        }

        lua_pushvalue(L, 2);
        int funcRef = luaL_ref(L, LUA_REGISTRYINDEX);

        gSMLua.registerHook(static_cast<HookEventType>(hookType), funcRef);
        return 0;
    });

    lua_register(L, "network_is_server", [](lua_State *L) {
        lua_pushboolean(L, true);
        return 1;
    });

    lua_register(L, "get_coopnet_id", [](lua_State *L) {
        lua_Integer index = luaL_checkinteger(L, 1);
        if (!isValidGlobalIndex(index)) {
            return luaL_error(L, "player index %d out of range", (int)index);
        }

        uint64_t peer = gNetworkPlayers[(size_t)index].endpoint.peerId;
        uint64_t coopNetId = gCoopNetIds[peer];

        std::string id = std::to_string(coopNetId);
        lua_pushlstring(L, id.c_str(), id.size());
        return 1;
    });

    lua_register(L, "clock_elapsed", [](lua_State *L) {
        lua_pushnumber(L, (float)clockElapsed());
        return 1;
    });

    lua_register(L, "clock_elapsed_f64", [](lua_State *L) {
        lua_pushnumber(L, clockElapsed());
        return 1;
    });

    lua_register(L, "clock_elapsed_ticks", [](lua_State *L) {
        lua_pushinteger(L, clockElapsedNano() * 3 / 100000000);
        return 1;
    });

    initLuaGlobalBindings(L);
    registerLuaGlobals();

    int result = luaL_dostring(L, gSMLuaConstants);
    if (result != LUA_OK) {
        Logging::error("SMLUA", "lua error: {}", lua_tostring(L, -1));
        lua_pop(L, 1);
        shutdown();
        return false;
    }

    lua_settop(L, 0);
    return true;
}

void SMLua::shutdown() {
    if (L) {
        registeredHooks.clear();
        lua_close(L);
        L = nullptr;
    }
}

void SMLua::registerHook(HookEventType hookType, int funcRef) {
    registeredHooks[hookType].push_back({funcRef});
}

int SMLua::createModEnv() {
    lua_newtable(L);
    lua_newtable(L);
    lua_pushglobaltable(L);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, [](lua_State *L) { 
        if (lua_type(L, 2) == LUA_TSTRING && isGlobalBound(lua_tostring(L, 2))) {
            lua_pushglobaltable(L);
            lua_pushvalue(L, 2);
            lua_pushvalue(L, 3);
            lua_settable(L, -3);
        } else {
            lua_rawset(L, 1);
        }
        return 0;
    });
    lua_setfield(L, -2, "__newindex");
    lua_setmetatable(L, -2);
    return lua_gettop(L);
}

bool SMLua::executeMod(const Mod &mod) {
    if (!L) return false;

    int envIdx = createModEnv();
    bool success = true;

    for (const auto &file : mod.files) {
        fs::path p(file.realPath);
        std::string ext = p.extension().string();

        if (ext == ".lua" || ext == ".luac") {
            if (luaL_loadfile(L, file.realPath.c_str()) != LUA_OK) {
                Logging::error("SMLUA", "error loading script {}: {} ", file.realPath, lua_tostring(L, -1));
                lua_pop(L, 1);
                success = false;
                continue;
            }

            lua_pushvalue(L, envIdx);
            const char *upvalueName = lua_setupvalue(L, -2, 1);
            if (!upvalueName) {
            }

            if (lua_pcall(L, 0, LUA_MULTRET, 0) != LUA_OK) {
                Logging::error("SMLUA", "error executing script {}: {} ", file.realPath, lua_tostring(L, -1));
                lua_pop(L, 1);
                success = false;
            }
        }
    }

    lua_remove(L, envIdx);
    return success;
}


void SMLua::callHook(HookEventType hookType) {
    auto it = registeredHooks.find(hookType);
    if (it == registeredHooks.end()) return;

    for (const auto &hook : it->second) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, hook.luaFuncRef);
        if (!lua_isfunction(L, -1)) {
            lua_pop(L, 1);
            continue;
        }

        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            Logging::error("SMLUA", "error executing hook {} callback: {}", static_cast<int>(hookType), lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }
}

void SMLua::callHook(HookEventType hookType, const std::function<int(lua_State *)> &pushArgs) {
    auto it = registeredHooks.find(hookType);
    if (it == registeredHooks.end()) return;

    for (const auto &hook : it->second) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, hook.luaFuncRef);
        if (!lua_isfunction(L, -1)) {
            lua_pop(L, 1);
            continue;
        }

        int nargs = pushArgs(L);

        if (lua_pcall(L, nargs, 0, 0) != LUA_OK) {
            Logging::error("SMLUA", "error executing hook {} callback: {}", static_cast<int>(hookType), lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }
}

void SMLua::pushLNT(const LSTNetworkValue &lnt) {
    switch (lnt.type) {
        case LSTNetworkType::INTEGER:
            lua_pushinteger(L, std::get<int64_t>(lnt.value));
            break;
        case LSTNetworkType::NUMBER:
            lua_pushnumber(L, std::get<double>(lnt.value));
            break;
        case LSTNetworkType::BOOLEAN:
            lua_pushboolean(L, std::get<bool>(lnt.value));
            break;
        case LSTNetworkType::STRING: {
            const auto &s = std::get<std::string>(lnt.value);
            lua_pushlstring(L, s.data(), s.size());
            break;
        }
        case LSTNetworkType::NIL:
            lua_pushnil(L);
            break;
        default:
            lua_pushnil(L);
            break;
    }
}

void SMLua::update() {
    if (!L) return;

    callHook(HookEventType::UPDATE);

    for (int i = 0; i < 16; i++) {
        callHook(HookEventType::MARIO_UPDATE, [&](lua_State *L) {
            lua_pushnil(L);
            return 1;
        });
    }
}