#pragma once

#include <string>
#include <lua.hpp>
#include "../mod.hpp"

class SMLua {
public:
    SMLua();
    ~SMLua();

    bool init();
    void shutdown();

    bool executeMod(const CoopMod &mod);

    lua_State *getState() const { return L; }
private:
    lua_State *L = nullptr;

    int createModEnv(); 
};

extern SMLua gSMLua;