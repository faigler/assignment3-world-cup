#pragma once

#include "../include/ConnectionHandler.h"ִִ

#ifndef STOMPPROTOCOL_H_
#define STOMPPROTOCOL_H_

#include <string>
#include <map>
#include <vector>
#include <unordered_map>

#include "event.h"

using std::map;
using std::string;
using std::vector;

class StompProtocol
{
public:
    StompProtocol();

    // State setters
    void setUsername(const string &user);
    void setLoggedIn(bool value);

    // Processing
    bool processServerFrame(const string &frame);
    string processUserCommand(const string &line);

private:
    // Client command handlers
    string handleJoin(const string &channel);
    string handleExit(const string &channel);
    vector<string> StompProtocol::handleReport(const string &filePath);
    void handleSummary(const string &game,
                       const string &user,
                       const string &file);
    string handleLogout();

    // User state
    string username;
    bool loggedIn;

    // Counters
    int subIdCounter;
    int receiptIdCounter;
    // Logout flow
    int pendingLogoutReceiptId; // -1 when no logout is pending
    bool shouldTerminate;       // true only after RECEIPT of logout

    // Subscriptions and receipts
    map<string, int> subscriptions;  // channel -> subscription id
    map<int, string> receiptActions; // receipt id -> action message

    // Stored game events (for summary)
    map<string, map<string, vector<Event>>> gameEvents;
};

#endif /* STOMPPROTOCOL_H_ */
