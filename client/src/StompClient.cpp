#include "StompClient.h"

#include <iostream>
#include <sstream>
#include <vector>

using namespace std;

static vector<string> splitBySpace(const string& line) {
    stringstream ss(line);
    vector<string> tokens;
    string t;
    while (ss >> t) tokens.push_back(t);
    return tokens;
}

StompClient::StompClient()
    : connectionHandler(nullptr),
      protocol(nullptr),
      shouldStop(false),
      connected(false) {}

StompClient::~StompClient() {
    cleanup();
}

void StompClient::run() {
    while (!shouldStop.load()) {
        string line;
        if (!getline(cin, line)) break; // EOF

        if (line.empty()) continue;

        auto tokens = splitBySpace(line);
        if (tokens.empty()) continue;

        const string& cmd = tokens[0];

        if (cmd == "login") {
            handleLoginCommand(line);
            continue;
        }

        // Every other command requires login/connect first
        if (connectionHandler == nullptr || protocol == nullptr) {
            cout << "Please login first" << endl;
            continue;
        }

        // Build STOMP frame via protocol
        string outFrame = protocol->processUserCommand(line);

        // Some commands (like summary) return empty string on purpose
        if (outFrame.empty()) continue;

        // Send to server with '\0' delimiter
        if (!connectionHandler->sendFrameAscii(outFrame, '\0')) {
            cout << "Disconnected. Exiting..." << endl;
            shouldStop = true;
            break;
        }

        // Note: logout is handled by waiting for RECEIPT in listenToServer(),
        // so we don't close immediately here.
    }

    cleanup();
}

void StompClient::handleLoginCommand(const std::string& line) {
    // Already connected?
    if (connectionHandler != nullptr) {
        cout << "User is already logged in" << endl;
        return;
    }

    auto args = splitBySpace(line);
    // expected: login host:port username password
    if (args.size() < 4) {
        cout << "Usage: login <host:port> <username> <password>" << endl;
        return;
    }

    string hostPort = args[1];
    string username = args[2];
    string password = args[3];

    size_t colon = hostPort.find(':');
    if (colon == string::npos) {
        cout << "Invalid host:port format" << endl;
        return;
    }

    string host = hostPort.substr(0, colon);
    short port = static_cast<short>(stoi(hostPort.substr(colon + 1)));

    // Create handler + TCP connect
    connectionHandler = new ConnectionHandler(host, port);
    if (!connectionHandler->connect()) {
        cout << "Cannot connect to " << host << ":" << port << endl;
        delete connectionHandler;
        connectionHandler = nullptr;
        return;
    }
    connected = true;

    // Create protocol
    protocol = new StompProtocol();
    protocol->setUsername(username);
    protocol->setLoggedIn(false);

    // Build CONNECT frame (no Frame class)
    // Important: empty line before body (no body here)
    string connectFrame =
        "CONNECT\n"
        "accept-version:1.2\n"
        "host:stomp.cs.bgu.ac.il\n"
        "login:" + username + "\n"
        "passcode:" + password + "\n"
        "\n";

    if (!connectionHandler->sendFrameAscii(connectFrame, '\0')) {
        cerr << "Failed to send CONNECT frame" << endl;
        delete protocol;
        delete connectionHandler;
        protocol = nullptr;
        connectionHandler = nullptr;
        connected = false;
        return;
    }

    // Start listener thread
    shouldStop = false;
    listenerThread = thread(&StompClient::listenToServer, this);
}

void StompClient::listenToServer() {
    while (!shouldStop.load()) {
        string inFrame;

        // Read until '\0'
        if (!connectionHandler->getFrameAscii(inFrame, '\0')) {
            if (!shouldStop.load()) {
                cout << "Disconnected. Exiting..." << endl;
            }
            shouldStop = true;
            break;
        }

        // Delegate parsing/printing to protocol
        bool keepRunning = protocol->processServerFrame(inFrame);

        if (!keepRunning) {
            // Protocol says stop (ERROR or logout receipt)
            shouldStop = true;
            break;
        }
    }
}

void StompClient::cleanup() {
    // Stop loops
    shouldStop = true;

    // Close socket first (unblocks listener if it's stuck on read)
    if (connectionHandler != nullptr) {
        connectionHandler->close();
    }

    // Join thread if exists
    if (listenerThread.joinable()) {
        listenerThread.join();
    }

    // Delete owned objects
    if (protocol != nullptr) {
        delete protocol;
        protocol = nullptr;
    }

    if (connectionHandler != nullptr) {
        delete connectionHandler;
        connectionHandler = nullptr;
    }

    connected = false;
}

/* =========================
   main() - stays in client
   ========================= */
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    StompClient client;
    client.run();
    return 0;
}
