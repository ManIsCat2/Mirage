#pragma once

#include <chrono>

constexpr int MAX_PLAYERS = 16;

inline double clockElapsed() {
    static const auto start = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

inline std::string stringToLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

inline bool getBit(uint8_t val, uint8_t num) {
    return (val >> num) & 1;
}

inline bool isValidGlobalIndex(int idx) {
    return idx >= 0 && idx < MAX_PLAYERS;
}