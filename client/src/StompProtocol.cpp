#include "StompProtocol.h"
#include <sstream>
#include <iostream>

using namespace std;

StompProtocol::StompProtocol()
    : subIdCounter(0), receiptIdCounter(0), loggedIn(false) {}

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
        size_t pos = frame.find("receipt-id:");
        int id = stoi(frame.substr(pos + 11));
        cout << receiptActions[id] << endl;
        return true;
    }

    if (frame.find("ERROR") == 0)
    {
        cout << "Server ERROR:\n"
             << frame << endl;
        loggedIn = false;
        return false;
    }

    if (frame.find("MESSAGE") == 0)
    {
        cout << frame << endl;
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

    if (cmd == "logout")
    {
        return handleLogout();
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

    cout << "Illegal command" << endl;
    return "";
}

string StompProtocol::handleJoin(const string &channel)
{
    if (subscriptions.count(channel))
    {
        cout << "Already subscribed\n";
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
           to_string(receiptId) + "\n\n\0";
}

string StompProtocol::handleExit(const string &channel)
{
    if (!subscriptions.count(channel))
    {
        cout << "Not subscribed\n";
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
           to_string(receiptId) + "\n\n\0";
}

string StompProtocol::handleLogout()
{
    int receiptId = ++receiptIdCounter;
    receiptActions[receiptId] = "Disconnecting";

    return "DISCONNECT\n"
           "receipt:" +
           to_string(receiptId) + "\n\n\0";
}

void StompProtocol::handleSummary(const string &game,
                                  const string &user,
                                  const string &file)
{
    cout << "Summary command received" << endl;
    cout << "Game: " << game << endl;
    cout << "User: " << user << endl;
    cout << "Output file: " << file << endl;
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

    // חייב להיות subscribed
    if (activeSubscriptions.count(gameName) == 0)
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

    return frames;
}


