#pragma once

#include <chrono>
#include <ctime>
#include <format>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <utility>

inline std::string timestamp() {
    std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

inline std::string applyHexColors(const std::string &str) {
    static const std::regex pattern(R"(\\#(?:([0-9a-fA-F]{6})|([0-9a-fA-F]{3}))\\|(\\#\\)|(\\#reset\\))");

    auto it = std::sregex_iterator(str.begin(), str.end(), pattern);
    auto end = std::sregex_iterator();

    std::string result;
    size_t lastPos = 0;

    for (; it != end; it++) {
        const std::smatch &match = *it;
        result.append(str, lastPos, match.position() - lastPos);
        lastPos = match.position() + match.length();

        if (match[4].matched) {
            result.append("\033[39m");
            continue;
        }

        uint8_t r = 0, g = 0, b = 0;

        if (match[1].matched) {
            std::string hex = match[1].str();
            r = std::stoi(hex.substr(0, 2), nullptr, 16);
            g = std::stoi(hex.substr(2, 2), nullptr, 16);
            b = std::stoi(hex.substr(4, 2), nullptr, 16);
        } else if (match[2].matched) {
            std::string hex = match[2].str();
            r = std::stoi(std::string(2, hex[0]), nullptr, 16);
            g = std::stoi(std::string(2, hex[1]), nullptr, 16);
            b = std::stoi(std::string(2, hex[2]), nullptr, 16);
        } else if (match[3].matched) {
            r = g = b = 0xff;
        }

        std::ostringstream ansiSeq;
        ansiSeq << "\e[38;2;" << static_cast<int>(r) << ";" << static_cast<int>(g) << ";" << static_cast<int>(b) << "m";
        result.append(ansiSeq.str());
    }
    result.append(str, lastPos, std::string::npos);
    return result;
}

namespace Logging {
    inline void write(std::ostream &os, const char *level, const char *levelColor, const std::string &title, const std::string &body) {
        os << "[" << timestamp() << "] "
        << levelColor << "[" << level << "]\033[0m "
        << "[" << title << "] "
        << applyHexColors(body) << "\033[0m\n";
    }

    template <typename... Args>
    void info(const std::string &title, std::format_string<Args...> fmt, Args &&...args) {
        write(std::cout, "INFO", "\033[36m", title, std::format(fmt, std::forward<Args>(args)...));
    }

    template <typename... Args>
    void warn(const std::string &title, std::format_string<Args...> fmt, Args &&...args) {
        write(std::cerr, "WARN", "\033[33m", title, std::format(fmt, std::forward<Args>(args)...));
    }

    template <typename... Args>
    void error(const std::string &title, std::format_string<Args...> fmt, Args &&...args) {
        write(std::cerr, "ERROR", "\033[31m", title, std::format(fmt, std::forward<Args>(args)...));
    }
}