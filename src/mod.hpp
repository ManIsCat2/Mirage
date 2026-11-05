#pragma once

#include <cstdint>
#include <vector>
#include <string>

struct ModFile {
    std::string relativePath = "";
    std::string realPath = "";
    size_t size = 0;
};

class Mod {
public:
    std::string name = "";
    std::vector<ModFile> files;
    std::string relativePath = "";
    std::string basePath = "";
    bool isDirectory = false;
    bool pausable = true;
    bool ignoreScriptWarnings = false;
    size_t size = 0;
    
    bool load(const std::string &modPath);
    void extractFields(const std::string &mainFilePath);
};