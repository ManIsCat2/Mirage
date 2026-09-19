#include "smlua.hpp"
#include "log.hpp"
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
    
    luaL_openlibs(L);
    return true;
}

void SMLua::shutdown() {
    if (L) {
        lua_close(L);
        L = nullptr;
    }
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