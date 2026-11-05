#pragma once

#include <atomic>

class Server {
private:
    int port;
public:
    Server(int p);
    void run(void);
    int getPort() const { return port; }
};

extern std::atomic<bool> gServerRunning;