package bgu.spl.net.impl.stomp;

import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.atomic.AtomicInteger;

import bgu.spl.net.api.StompMessagingProtocol;
import bgu.spl.net.srv.Connections;
import bgu.spl.net.impl.data.Database;
import bgu.spl.net.impl.data.LoginStatus;

public class StompMessagingProtocolImpl implements StompMessagingProtocol<String> {
    private int connectionId;
    private boolean shouldTerminate = false;
    private Connections<String> connections;
    private static final AtomicInteger messageIdCounter = new AtomicInteger(0);
    private boolean connected = false;
    private final Database database = Database.getInstance();
    private String username = null;

    @Override
    public void start(int connectionId, Connections<String> connections) {
        this.connectionId = connectionId;
        this.connections = connections;
    }

    @Override
    public void process(String message) {

        String originalFrame = message;
        String[] lines = message.split("\n");
        String command = lines[0];

        Map<String, String> headers = new HashMap<>();
        String body = "";

        int i = 1;

        // Parse headers
        while (i < lines.length && !lines[i].isEmpty()) {
            String[] parts = lines[i].split(":", 2);
            if (parts.length == 2) {
                headers.put(parts[0], parts[1]);
            }
            i++;
        }

        // Skip empty line
        i++;

        // Parse body (if exists)
        while (i < lines.length) {
            body += lines[i];
            if (i < lines.length - 1) {
                body += "\n";
            }
            i++;
        }

        switch (command) {
            case "CONNECT":
                handleConnect(headers, originalFrame);
                break;

            case "SEND":
                handleSend(headers, body, originalFrame);
                break;

            case "SUBSCRIBE":
                handleSubscribe(headers, originalFrame);
                break;

            case "UNSUBSCRIBE":
                handleUnsubscribe(headers, originalFrame);
                break;

            case "DISCONNECT":
                handleDisconnect(headers);
                break;

            default:
                sendError("Unknown command", originalFrame, "", headers);
        }
    }

    @Override
    public boolean shouldTerminate() {
        return shouldTerminate;
    }

    /* ================= Handlers ================= */

    private void handleConnect(Map<String, String> headers, String originalFrame) {

        if (connected) {
            sendError(
                    "Already connected",
                    originalFrame,
                    "Client has already sent CONNECT frame.",
                    headers);
            return;
        }

        String acceptVersion = headers.get("accept-version");
        String host = headers.get("host");
        String login = headers.get("login");
        String passcode = headers.get("passcode");

        if (acceptVersion == null || host == null || login == null || passcode == null) {
            sendError(
                    "malformed frame received",
                    originalFrame,
                    "CONNECT frame must contain accept-version, host, login and passcode headers.",
                    headers);
            return;
        }

        if (!acceptVersion.contains("1.2")) {
            sendError(
                    "version not supported",
                    originalFrame,
                    "Server supports STOMP version 1.2 only.",
                    headers);
            return;
        }

        LoginStatus status = database.login(connectionId, login, passcode);

        switch (status) {

            case CLIENT_ALREADY_CONNECTED:
                sendError(
                        "Client already connected",
                        originalFrame,
                        "This connection id is already logged in.",
                        headers);
                return;

            case ALREADY_LOGGED_IN:
                sendError(
                        "User already logged in",
                        originalFrame,
                        "User " + login + " is already logged in.",
                        headers);
                return;

            case WRONG_PASSWORD:
                sendError(
                        "Wrong password",
                        originalFrame,
                        "Incorrect password for user " + login,
                        headers);
                return;

            case ADDED_NEW_USER:
            case LOGGED_IN_SUCCESSFULLY:
                // success
                connected = true;
                username = login;
                break;
        }

        String response = "CONNECTED\n" +
                "version:1.2\n\n" +
                "\0";

        connections.send(connectionId, response);
    }

    private void handleSend(Map<String, String> headers,
            String body,
            String originalFrame) {

        if (!connected) {
            sendError(
                    "Not connected",
                    originalFrame,
                    "Command sent before CONNECT.",
                    headers);
            return;
        }

        String destination = headers.get("destination");

        if (destination == null) {
            sendError(
                    "malformed frame received",
                    originalFrame,
                    "SEND frame must contain a destination header.",
                    headers);
            return;
        }

        // check if client is subscribed
        if (!connections.isSubscribed(connectionId, destination)) {
            sendError(
                    "not subscribed",
                    originalFrame,
                    "Client is not subscribed to destination " + destination,
                    headers);
            return;
        }

        // broadcast message
        Map<Integer, Integer> subscribers = connections.getSubscribers(destination);

        for (Map.Entry<Integer, Integer> entry : subscribers.entrySet()) {
            int subscriberId = entry.getKey();
            int subscriptionId = entry.getValue();

            int messageId = messageIdCounter.incrementAndGet();
            String messageFrame = "MESSAGE\n" +
                    "subscription:" + subscriptionId + "\n" +
                    "destination:" + destination + "\n" +
                    "message-id:" + messageId + "\n\n" +
                    body +
                    "\0";

            connections.send(subscriberId, messageFrame);
        }

        handleReceipt(headers);
    }

    private void handleSubscribe(Map<String, String> headers, String originalFrame) {

        if (!connected) {
            sendError(
                    "Not connected",
                    originalFrame,
                    "Command sent before CONNECT.",
                    headers);
            return;
        }

        String destination = headers.get("destination");
        String idStr = headers.get("id");

        if (destination == null || idStr == null) {
            sendError(
                    "malformed frame received",
                    originalFrame,
                    "SUBSCRIBE frame must contain destination and id headers.",
                    headers);
            return;
        }

        int subId;
        try {
            subId = Integer.parseInt(idStr);
        } catch (NumberFormatException e) {
            sendError(
                    "malformed frame received",
                    originalFrame,
                    "Subscription id must be a number.",
                    headers);
            return;
        }

        boolean ok = connections.subscribe(connectionId, destination, subId);

        if (!ok) {
            sendError(
                    "subscription failed",
                    originalFrame,
                    "Client is already subscribed to destination " + destination,
                    headers);
            return;
        }

        handleReceipt(headers);
    }

    private void handleUnsubscribe(Map<String, String> headers, String originalFrame) {

        if (!connected) {
            sendError(
                    "Not connected",
                    originalFrame,
                    "Command sent before CONNECT.",
                    headers);
            return;
        }

        String idStr = headers.get("id");

        if (idStr == null) {
            sendError(
                    "malformed frame received",
                    originalFrame,
                    "UNSUBSCRIBE frame must contain id header.",
                    headers);
            return;
        }

        int subId;
        try {
            subId = Integer.parseInt(idStr);
        } catch (NumberFormatException e) {
            sendError(
                    "malformed frame received",
                    originalFrame,
                    "Subscription id must be a number.",
                    headers);
            return;
        }

        boolean ok = connections.unsubscribe(connectionId, subId);

        if (!ok) {
            sendError(
                    "subscription not found",
                    originalFrame,
                    "No subscription with id " + subId + " exists.",
                    headers);
            return;
        }

        handleReceipt(headers);
    }

    private void handleDisconnect(Map<String, String> headers) {

        if (headers == null) {
            sendError(
                    "malformed frame received",
                    null,
                    "DISCONNECT frame must contain headers section (even if empty).",
                    headers);
            return;
        }

        if (!connected) {
            sendError(
                    "Not connected",
                    null,
                    "DISCONNECT frame received before CONNECT.",
                    headers);
            return;
        }

        handleReceipt(headers);

        database.logout(connectionId);

        shouldTerminate = true;
        connections.disconnect(connectionId);
    }

    /* ================= HELPERS ================= */

    private void handleReceipt(Map<String, String> headers) {
        String receiptId = headers.get("receipt");
        if (receiptId != null) {
            String response = "RECEIPT\n" +
                    "receipt-id:" + receiptId + "\n\n" +
                    "\0";

            connections.send(connectionId, response);
        }
    }

    private void sendError(String shortMessage,
            String originalFrame,
            String details,
            Map<String, String> headers) {

        StringBuilder error = new StringBuilder("ERROR\n");

        // If the client asked for a receipt, send it back in receipt-id
        if (headers != null && headers.containsKey("receipt")) {
            error.append("receipt-id:").append(headers.get("receipt")).append("\n");
        }

        // Short description (header)
        error.append("message:").append(shortMessage).append("\n\n");

        // Body: original frame + detailed reason
        error.append("The message:\n-----\n");
        error.append(originalFrame == null ? "" : originalFrame);
        error.append("\n-----\n");
        error.append(details == null ? "" : details);
        error.append("\n\0");

        connections.send(connectionId, error.toString());

        // Protocol rule: after ERROR -> close connection
        shouldTerminate = true;
        connections.disconnect(connectionId);
    }

}
