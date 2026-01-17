#pragma once

#include <string>
#include <thread>
#include <atomic>

#include "ConnectionHandler.h"
#include "StompProtocol.h"

class StompClient {
public:
    StompClient();
    ~StompClient();

    // main loop of the client (reads user input)
    void run();

private:
    // Listener thread function: reads frames from server and passes them to protocol
    void listenToServer();

    // Handle "login" command separately (creates ConnectionHandler, connects, sends CONNECT)
    void handleLoginCommand(const std::string& line);

    // Cleanup: stop threads, close socket, delete objects
    void cleanup();

private:
    ConnectionHandler* connectionHandler;
    StompProtocol* protocol;

    std::thread listenerThread;

    std::atomic<bool> loggedIn;        // true after CONNECTED, false after logout/error
    std::atomic<bool> shouldStop;      // stop the whole client
    std::atomic<bool> listenerRunning; // to avoid join issues
};
