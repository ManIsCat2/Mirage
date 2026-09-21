#include "smlua.hpp"
#include "../config.hpp"
#include "constants.hpp"
#include "functions.hpp"
#include "log.hpp"
#include <iostream>
#include <filesystem>
#include <lualib.h>

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
    luaL_requiref(L, "math", luaopen_math, 1);
    luaL_requiref(L, "string", luaopen_string, 1);
    luaL_requiref(L, "table", luaopen_table, 1);
    luaL_requiref(L, "coroutine", luaopen_coroutine, 1);
    luaL_requiref(L, "utf8", luaopen_utf8, 1);

    lua_register(L, "hook_event", [](lua_State* L) {
        int hookType = luaL_checkinteger(L, 1);
        if (!lua_isfunction(L, 2)) {
            return luaL_error(L, "hook_event expects a function as the second argument");
        }

        lua_pushvalue(L, 2);
        int funcRef = luaL_ref(L, LUA_REGISTRYINDEX);

        gSMLua.registerHook(hookType, funcRef);
        return 0;
    });

    lua_register(L, "network_is_server", [](lua_State* L) {
        lua_pushboolean(L, true);
        return 1;
    });

    // until i add proper c objects/userdata and stuff
    const char *luaTableStubs = R"(
        local function createFakeStruct()
            local mt = {}
            mt.__index = function(t, k)
                local fake = setmetatable({}, mt)
                rawset(t, k, fake)
                return fake
            end
            mt.__add = function() return 0 end
            mt.__sub = function() return 0 end
            mt.__mul = function() return 0 end
            mt.__div = function() return 0 end
            mt.__eq  = function() return false end
            mt.__lt  = function() return false end
            mt.__le  = function() return false end
            
            mt.__band = function() return 0 end
            mt.__bor  = function() return 0 end
            mt.__bxor = function() return 0 end
            mt.__shl  = function() return 0 end
            mt.__shr  = function() return 0 end
            mt.__bnot = function() return 0 end
            
            return setmetatable({}, mt)
        end

        _G.createFakeStruct = createFakeStruct

        local function createSyncTable(name, parent)
            local st = {
                _name = name,
                _parent = parent,
                _table = {},
                _seq = {},
                _hook_on_changed = {}
            }
            return setmetatable(st, _SyncTable)
        end

        _G.gGlobalSyncTable = createSyncTable("gGlobalSyncTable", nil)
        _G.gPlayerSyncTable = {}
        _G.gMarioStates = {}
        _G.gNetworkPlayers = {}

        for i = 0, 15 do
            local mario = createFakeStruct()
            mario.playerIndex = i
            _G.gMarioStates[i] = mario

            local np = createFakeStruct()
            np.globalIndex = i
            _G.gNetworkPlayers[i] = np

            _G.gPlayerSyncTable[i] = createSyncTable("gPlayerSyncTable", nil)
        end
        
        _G.gServerSettings = createFakeStruct()
        _G.gLevelValues = createFakeStruct()
    )";

    int result = luaL_dostring(L, luaTableStubs);
    if (result != LUA_OK) {
        Logging::log("SMLUA", "Lua error (stubs): {}", lua_tostring(L, -1));
        lua_pop(L, 1);
        shutdown();
        return false;
    }

    result = luaL_dostring(L, gSMLuaConstants);
    if (result != LUA_OK) {
        Logging::log("SMLUA", "Lua error (constants): {}", lua_tostring(L, -1));
        lua_pop(L, 1);
        shutdown();
        return false;
    }

    smluaBindAutogenFuncs();

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

void SMLua::registerHook(int hookType, int funcRef) {
    registeredHooks[hookType].push_back({funcRef});
    //Logging::log("SMLUA", "Hooked func {} to hook {}", funcRef, hookType);
}

void SMLua::registerFunc(lua_State *L, const char *name, int (*func)(lua_State *L)) {
    lua_pushcfunction(L, func);
    lua_setglobal(L, name);
}

int SMLua::createModEnv() {
    lua_newtable(L); 
    lua_newtable(L); 
    lua_pushglobaltable(L);
    lua_setfield(L, -2, "__index");
    lua_setmetatable(L, -2);
    
    return lua_gettop(L);
}

bool SMLua::executeMod(const CoopMod &mod) {
    if (!L) return false;

    int envIdx = createModEnv();
    bool success = true;

    for (const auto &file : mod.files) {
        fs::path p(file.realPath);
        std::string ext = p.extension().string();

        if (ext == ".lua" || ext == ".luac") {
            if (luaL_loadfile(L, file.realPath.c_str()) != LUA_OK) {
                Logging::log("SMLUA", "Error loading script {}: {} ", file.realPath, lua_tostring(L, -1));
                lua_pop(L, 1);
                success = false;
                continue;
            }

            lua_pushvalue(L, envIdx);
            const char *upvalueName = lua_setupvalue(L, -2, 1);
            if (!upvalueName) {
            }

            if (lua_pcall(L, 0, LUA_MULTRET, 0) != LUA_OK) {
                Logging::log("SMLUA", "Error executing script {}: {} ", file.realPath, lua_tostring(L, -1));
                lua_pop(L, 1);
                success = false;
            }
        }
    }

    lua_remove(L, envIdx);
    return success;
}


void SMLua::executeHooks(int hookType) {
    auto it = registeredHooks.find(hookType);
    if (it == registeredHooks.end()) return;

    for (const auto& hook : it->second) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, hook.luaFuncRef);
        if (!lua_isfunction(L, -1)) {
            lua_pop(L, 1);
            continue;
        }

        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            Logging::log("SMLUA", "Error executing hook {} callback: {}", hookType, lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }
}
template <typename F>
void SMLua::executeHooks(int hookType, F &&pushArgs) {
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
            Logging::log("SMLUA", "Error executing hook {} callback: {}", hookType, lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }
}

void SMLua::update() {
    if (!L || !gServerConfig.executeMods) return;

    executeHooks(HOOK_UPDATE);

    for (int i = 0; i < 16; i++) {
        executeHooks(HOOK_MARIO_UPDATE, [&](lua_State *L) {
            lua_getglobal(L, "gMarioStates");
            lua_pushinteger(L, i);
            lua_gettable(L, -2);
            lua_remove(L, -2);
            return 1;
        });
    }
}