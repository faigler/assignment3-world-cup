#include "StompClient.h"

#include <iostream>
#include <sstream>
#include <vector>

using namespace std;

static vector<string> splitBySpace(const string &line)
{
	stringstream ss(line);
	vector<string> tokens;
	string t;
	while (ss >> t)
		tokens.push_back(t);
	return tokens;
}

StompClient::StompClient()
	: connectionHandler(nullptr),
	  protocol(nullptr),
	  loggedIn(false),
	  shouldStop(false),
	  listenerRunning(false) {}

StompClient::~StompClient()
{
	cleanup();
}

void StompClient::run()
{
	while (!shouldStop.load())
	{
		string line;
		if (!getline(cin, line))
			break; // EOF

		if (line.empty())
			continue;

		auto tokens = splitBySpace(line);
		if (tokens.empty())
			continue;

		string cmd = tokens[0];

		if (cmd == "login")
		{
			handleLoginCommand(line);
			continue;
		}

		// Any command except login requires being logged in (or at least connected)
		if (connectionHandler == nullptr || protocol == nullptr)
		{
			cout << "Please login first" << endl;
			continue;
		}

		// Build the frame text via protocol
		string outFrame = protocol->processUserCommand(line);

		if (outFrame.empty())
		{
			// protocol already printed error or command was "summary"
			continue;
		}

		// Send to server (delimiter '\0')
		if (!connectionHandler->sendFrameAscii(outFrame, '\0'))
		{
			cout << "Disconnected. Exiting..." << endl;
			shouldStop = true;
			loggedIn = false;
			break;
		}

		// If this was logout, we do NOT close immediately.
		// We wait for RECEIPT in listener thread, then it will stop and we cleanup.
	}

	cleanup();
}

void StompClient::handleLoginCommand(const string &line)
{
	// If already connected, reject
	if (connectionHandler != nullptr)
	{
		cout << "User is already logged in" << endl;
		return;
	}

	auto args = splitBySpace(line);
	// expected: login host:port username password
	if (args.size() < 4)
	{
		cout << "Usage: login <host:port> <username> <password>" << endl;
		return;
	}

	string hostPort = args[1];
	string user = args[2];
	string pass = args[3];

	size_t colon = hostPort.find(':');
	if (colon == string::npos)
	{
		cout << "Invalid host:port format" << endl;
		return;
	}

	string host = hostPort.substr(0, colon);
	short port = static_cast<short>(stoi(hostPort.substr(colon + 1)));

	connectionHandler = new ConnectionHandler(host, port);
	if (!connectionHandler->connect())
	{
		cout << "Cannot connect to " << host << ":" << port << endl;
		delete connectionHandler;
		connectionHandler = nullptr;
		return;
	}

	protocol = new StompProtocol();
	protocol->setUsername(user);
	protocol->setLoggedIn(false);

	// Build CONNECT frame text manually (no Frame class)
	// STOMP server expects \n line breaks and empty line before body (none here)
	string connectFrame =
		"CONNECT\n"
		"accept-version:1.2\n"
		"host:stomp.cs.bgu.ac.il\n"
		"login:" +
		user + "\n"
			   "passcode:" +
		pass + "\n"
			   "\n"; // end headers

	if (!connectionHandler->sendFrameAscii(connectFrame, '\0'))
	{
		cerr << "Failed to send CONNECT frame" << endl;
		delete protocol;
		delete connectionHandler;
		protocol = nullptr;
		connectionHandler = nullptr;
		return;
	}

	// Start listener thread
	shouldStop = false;
	loggedIn = true; // connection exists (actual login confirmed when CONNECTED arrives)
	listenerRunning = true;

	listenerThread = thread(&StompClient::listenToServer, this);
}

void StompClient::listenToServer()
{
	while (!shouldStop.load())
	{
		string inFrame;
		if (!connectionHandler->getFrameAscii(inFrame, '\0'))
		{
			// socket closed or read failed
			if (!shouldStop.load())
			{
				cout << "Disconnected. Exiting..." << endl;
			}
			shouldStop = true;
			loggedIn = false;
			break;
		}

		bool keepRunning = protocol->processServerFrame(inFrame);

		if (!keepRunning)
		{
			// protocol decided: stop (ERROR or logout receipt)
			shouldStop = true;
			loggedIn = false;
			break;
		}
	}

	listenerRunning = false;
}

void StompClient::cleanup()
{
	// Stop loop
	shouldStop = true;

	// Close socket (safe even if already closed)
	if (connectionHandler != nullptr)
	{
		connectionHandler->close();
	}

	// Join listener thread if running
	if (listenerThread.joinable())
	{
		listenerThread.join();
	}

	// Delete objects
	if (protocol != nullptr)
	{
		delete protocol;
		protocol = nullptr;
	}
	if (connectionHandler != nullptr)
	{
		delete connectionHandler;
		connectionHandler = nullptr;
	}

	loggedIn = false;
}


