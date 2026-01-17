#include "StompProtocol.h"
#include <sstream>
#include <iostream>
#include <fstream>
#include <algorithm>

using namespace std;

StompProtocol::StompProtocol()
    : loggedIn(false),
      subIdCounter(0),
      receiptIdCounter(0),
      pendingLogoutReceiptId(-1),
      shouldTerminate(false) {}

void StompProtocol::setUsername(const string &user)
{
    username = user;
}

void StompProtocol::setLoggedIn(bool value)
{
    loggedIn = value;
}

bool StompProtocol::processServerFrame(const string &frame)
{
    if (frame.find("CONNECTED") == 0)
    {
        cout << "Login successful" << endl;
        loggedIn = true;
        return true;
    }

    if (frame.find("RECEIPT") == 0)
    {
        // Find the position of the "receipt-id:" header
        size_t pos = frame.find("receipt-id:");
        if (pos == string::npos)
            return true; // No receipt-id found, ignore frame

        // Extract only the receipt-id value (until end of line)
        size_t endLine = frame.find('\n', pos);
        string idStr = frame.substr(pos + 11, endLine - (pos + 11));
        int id = stoi(idStr);

        // Print the action associated with this receipt-id, if exists
        if (receiptActions.count(id) > 0)
            cout << receiptActions[id] << endl;

        // If this receipt corresponds to a logout request,
        // mark protocol for termination (socket will be closed by the client)
        if (id == pendingLogoutReceiptId)
        {
            loggedIn = false;
            shouldTerminate = true;
            return false; // Signal listener thread to stop
        }

        return true;
    }

    if (frame.find("ERROR") == 0)
    {
        // Find "message:" header
        size_t msgPos = frame.find("message:");
        string shortMsg = "";

        if (msgPos != string::npos)
        {
            // Extract short error message line
            size_t msgEnd = frame.find('\n', msgPos);
            shortMsg = frame.substr(msgPos + 8, msgEnd - (msgPos + 8));
        }

        // Find start of body (after empty line)
        size_t bodyPos = frame.find("\n\n");
        string body = "";

        if (bodyPos != string::npos)
        {
            // Extract full error body
            body = frame.substr(bodyPos + 2);
        }

        // Print error information
        cout << "ERROR message: " << shortMsg << endl;
        cout << "ERROR description:\n"
             << body << endl;

        // Protocol rule: ERROR closes connection
        loggedIn = false;
        shouldTerminate = true;
        return false;
    }

    if (frame.find("MESSAGE") == 0)
    {
        // Extract destination
        size_t destPos = frame.find("destination:");
        size_t destEnd = frame.find('\n', destPos);
        string gameName = frame.substr(destPos + 12, destEnd - (destPos + 12));

        // Extract body
        size_t bodyPos = frame.find("\n\n");
        string body = frame.substr(bodyPos + 2);

        Event event(body);

        // Extract username from the first line: "user: <name>"
        stringstream bodyStream(body);
        string firstLine;
        getline(bodyStream, firstLine);

        string user = "";
        if (firstLine.find("user:") == 0)
        {
            user = firstLine.substr(6); // after "user: "
        }

        gameEvents[gameName][user].push_back(event);

        cout << "Received message from " << gameName << ":\n"
             << body << endl;
        return true;
    }

    return true;
}

string StompProtocol::processUserCommand(const string &line)
{
    stringstream ss(line);
    string cmd;
    ss >> cmd;

    if (cmd == "login")
    {
        cout << "User already logged in" << endl;
        return "";
    }

    if (cmd == "join")
    {
        string channel;
        if (!(ss >> channel))
        {
            cout << "Usage: join <channel>" << endl;
            return "";
        }
        return handleJoin(channel);
    }

    if (cmd == "exit")
    {
        string channel;
        if (!(ss >> channel))
        {
            cout << "Usage: exit <channel>" << endl;
            return "";
        }
        return handleExit(channel);
    }

    if (cmd == "report")
    {
        string filePath;
        if (!(ss >> filePath))
        {
            cout << "Usage: report <file>" << endl;
            return "";
        }
        return handleReport(filePath);
    }

    if (cmd == "summary")
    {
        string game, user, file;
        if (!(ss >> game >> user >> file))
        {
            cout << "Usage: summary <game> <user> <file>" << endl;
            return "";
        }
        handleSummary(game, user, file);
        return "";
    }

    if (cmd == "logout")
    {
        return handleLogout();
    }

    cout << "Illegal command" << endl;
    return "";
}

/* ================= Handlers ================= */

string StompProtocol::handleJoin(const string &channel)
{
    if (subscriptions.count(channel) > 0)
    {
        cout << "Already subscribed to " << channel << "\n";
        return "";
    }

    int subId = ++subIdCounter;
    int receiptId = ++receiptIdCounter;

    subscriptions[channel] = subId;
    receiptActions[receiptId] = "Joined channel " + channel;

    return "SUBSCRIBE\n"
           "destination:" +
           channel + "\n"
                     "id:" +
           to_string(subId) + "\n"
                              "receipt:" +
           to_string(receiptId) + "\n\n";
}

string StompProtocol::handleExit(const string &channel)
{
    if (subscriptions.count(channel) == 0)
    {
        cout << "Not subscribed to " << channel << "\n";
        return "";
    }

    int subId = subscriptions[channel];
    subscriptions.erase(channel);

    int receiptId = ++receiptIdCounter;
    receiptActions[receiptId] = "Exited channel " + channel;

    return "UNSUBSCRIBE\n"
           "id:" +
           to_string(subId) + "\n"
                              "receipt:" +
           to_string(receiptId) + "\n\n";
}

string StompProtocol::handleReport(const string &filePath)
{
    names_and_events data;

    try
    {
        data = parseEventsFile(filePath);
    }
    catch (...)
    {
        cout << "Error: could not parse events file" << endl;
        return "";
    }

    string gameName = data.team_a_name + "_" + data.team_b_name;

    if (subscriptions.count(gameName) == 0)
    {
        cout << "Error: not subscribed to " << gameName << endl;
        return "";
    }

    string frames = "";

    for (const Event &event : data.events)
    {

        string body =
            "user: " + username + "\n" +
            "team a: " + data.team_a_name + "\n" +
            "team b: " + data.team_b_name + "\n" +
            "event name: " + event.get_name() + "\n" +
            "time: " + to_string(event.get_time()) + "\n" +
            "general game updates:\n";

        for (const auto &p : event.get_game_updates())
        {
            body += p.first + ":" + p.second + "\n";
        }

        body += "team a updates:\n";
        for (const auto &p : event.get_team_a_updates())
        {
            body += p.first + ":" + p.second + "\n";
        }

        body += "team b updates:\n";
        for (const auto &p : event.get_team_b_updates())
        {
            body += p.first + ":" + p.second + "\n";
        }

        body += "description:\n" + event.get_discription() + "\n";

        frames +=
            "SEND\n"
            "destination:" +
            gameName + "\n\n" +
            body + "\0";

        gameEvents[gameName][username].push_back(event);
    }

    if (!frames.empty() && frames.back() == '\0')
    {
        frames.pop_back();
    }

    return frames;
}

void StompProtocol::handleSummary(const string &game, const string &user, const string &file)
{
    // Check if we have events
    if (gameEvents.count(game) == 0 ||
        gameEvents[game].count(user) == 0)
    {
        cout << "No events found for user " << user
             << " in game " << game << endl;
        return;
    }

    // Opens the output file:
    // - If the file does not exist, it will be created
    // - If the file already exists, its content will be overwritten
    ofstream outFile(file);
    if (!outFile)
    {
        cout << "Error creating file: " << file << endl;
        return;
    }

    // Copy events and sort by time
    vector<Event> events = gameEvents[game][user];
    sort(events.begin(), events.end(),
         [](const Event &a, const Event &b)
         {
             return a.get_time() < b.get_time();
         });

    string teamA = events[0].get_team_a_name();
    string teamB = events[0].get_team_b_name();

    map<string, string> generalStats;
    map<string, string> teamAStats;
    map<string, string> teamBStats;

    // Collect statistics (last value wins)
    for (const Event &event : events)
    {
        for (const auto &p : event.get_game_updates())
            generalStats[p.first] = p.second;

        for (const auto &p : event.get_team_a_updates())
            teamAStats[p.first] = p.second;

        for (const auto &p : event.get_team_b_updates())
            teamBStats[p.first] = p.second;
    }

    // Header
    outFile << teamA << " vs " << teamB << "\n";
    outFile << "Game stats:\n";

    // General stats
    outFile << "General stats:\n";
    for (const auto &p : generalStats)
        outFile << p.first << ": " << p.second << "\n";
    outFile << "\n";

    // Team A stats
    outFile << teamA << " stats:\n";
    for (const auto &p : teamAStats)
        outFile << p.first << ": " << p.second << "\n";
    outFile << "\n";

    // Team B stats
    outFile << teamB << " stats:\n";
    for (const auto &p : teamBStats)
        outFile << p.first << ": " << p.second << "\n";
    outFile << "\n";

    // Event reports
    outFile << "Game event reports:\n";
    for (const Event &event : events)
    {
        outFile << event.get_time()
                << " - " << event.get_name() << ":\n\n";
        outFile << event.get_discription() << "\n\n";
    }

    outFile.close();
    cout << "Summary file created at " << file << endl;
}

string StompProtocol::handleLogout()
{
    if (!loggedIn)
    {
        cout << "Not logged in" << endl;
        return "";
    }

    int receiptId = ++receiptIdCounter;
    pendingLogoutReceiptId = receiptId;
    receiptActions[receiptId] = "Disconnecting...";

    return "DISCONNECT\n"
           "receipt:" +
           to_string(receiptId) +
           "\n\n";
}
