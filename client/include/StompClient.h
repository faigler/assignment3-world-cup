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

    // Runs the client main loop (reads user input and sends frames)
    void run();

private:
    // Handles the "login" keyboard command:
    // - opens socket
    // - sends CONNECT frame
    // - starts listener thread
    void handleLoginCommand(const std::string& line);

    // Listener thread function:
    // reads frames from server and delegates parsing to protocol
    void listenToServer();

    // Stops everything safely: close socket, join thread, delete objects
    void cleanup();

private:
    ConnectionHandler* connectionHandler;
    StompProtocol* protocol;

    std::thread listenerThread;

    std::atomic<bool> shouldStop;   // stop the whole client
};
