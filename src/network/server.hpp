#pragma once

#include <atomic>

class Server {
private:
    int port;
public:
    Server(int p);
    void run();
    int getPort() const { return port; }
};

extern std::atomic<bool> gServerRunning;