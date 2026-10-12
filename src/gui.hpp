#pragma once

#include <atomic>
#include <thread>
#include <vector>
#include <string>
#include <mutex>

class Gui {
private:
    std::thread guiThread;
    std::atomic<bool> running{false};
    void run();
public:
    std::vector<std::string> chatLog;
    std::mutex chatMutex;

    Gui();
    ~Gui();
    void start();
    void stop();
};

extern Gui gGui;