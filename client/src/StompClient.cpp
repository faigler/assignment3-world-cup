#include "StompClient.h"

#include <iostream>
#include <sstream>
#include <vector>

using namespace std;


 // Splits an input line by spaces into tokens (vector<string>).
 // Used to parse user commands from stdin.
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
      shouldStop(false) {}

StompClient::~StompClient() {
    cleanup();
}


// Main client loop:
 //- Reads user input
 //- Handles login separately
 //- Sends other commands to the server via the protocol
void StompClient::run() {
    while (!shouldStop.load()) {
        string line;
        if (!getline(cin, line)) break; // returns false only if the client actively closes stdin (Ctrl-D)

        if (line.empty()) continue;

        auto tokens = splitBySpace(line); // the compiler will decide the type based on the return type of the function
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


// Handles the login command:
// - Parses host, port, username, password
// - Opens TCP connection
// - Sends CONNECT frame
// - Starts listener thread
void StompClient::handleLoginCommand(const std::string& line) {
    // Already connected?
    if (connectionHandler != nullptr) {
        cout << "User is already logged in" << endl;
        return;
    }

    auto args = splitBySpace(line); // the compiler will decide the type based on the return type of the function
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

    // Create protocol
    protocol = new StompProtocol();
    protocol->setUsername(username);
    protocol->setLoggedIn(false);

    // Build CONNECT frame 
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
        return;
    }

    shouldStop = false;
    listenerThread = thread(&StompClient::listenToServer, this); // new thread runs listenToServer() on this object
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

    // Join thread if exists, the main thread (listening to stdin) need to wait for the listenerThread to end
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
}


// Program entry point.
// Creates a StompClient instance and starts it.
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    StompClient client; // STACK object
    client.run();
    return 0;
}
