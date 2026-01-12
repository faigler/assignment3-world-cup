package bgu.spl.net.impl.stomp;

import java.util.HashMap;
import java.util.Map;

import bgu.spl.net.api.MessagingProtocol;
import bgu.spl.net.api.StompMessagingProtocol;
import bgu.spl.net.srv.Connections;
import bgu.spl.net.srv.ConnectionsImpl;

public class StompMessagingProtocolImpl implements StompMessagingProtocol<String> {
    private int connectionId;
    private boolean shouldTerminate = false;
    private ConnectionsImpl<String> connections;

    @Override
    public void start(int connectionId, Connections<String> connections) {
        this.connectionId = connectionId;
        this.connections = (ConnectionsImpl<String>) connections;
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
            body += lines[i] + "\n";
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
                sendError("Unknown command",originalFrame, "", headers);
        }
    }

    @Override
    public boolean shouldTerminate() {
        return shouldTerminate;
    }

    /* ================= Handlers ================= */

    private void handleConnect(Map<String, String> headers, String originalFrame) {

        if (!headers.containsKey("accept-version") ||
                !headers.containsKey("host") ||
                !headers.containsKey("login") ||
                !headers.containsKey("passcode")) {

            sendError(
                    "malformed frame received",
                    originalFrame,
                    "CONNECT frame must contain accept-version, host, login and passcode headers.",
                    headers);
            return;
        }

        // success
        String response = "CONNECTED\n" +
                "version:1.2\n\n" +
                "\0";

        connections.send(connectionId, response);
    }

    private void handleSend(Map<String, String> headers,
            String body,
            String originalFrame) {

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
        connections.send(destination, body);

        handleReceipt(headers);
    }

    private void handleSubscribe(Map<String, String> headers, String originalFrame) {

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

        connections.subscribe(connectionId, destination, subId);
        handleReceipt(headers);
    }

    private void handleUnsubscribe(Map<String, String> headers, String originalFrame) {

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

        connections.unsubscribe(connectionId, subId);
        handleReceipt(headers);
    }

    private void handleDisconnect(Map<String, String> headers) {
        handleReceipt(headers);
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
