package bgu.spl.net.srv;

import java.io.IOException;
import java.util.concurrent.ConcurrentHashMap;

public class ConnectionsImpl<T> implements Connections<T> {

    // <connectionId, ConnectionHandler>
    private final ConcurrentHashMap<Integer, ConnectionHandler<T>> handlers;

    // <channel, <connectionId, subscriptionId>>
    private final ConcurrentHashMap<String, ConcurrentHashMap<Integer, Integer>> channelSubscriptions;

    // <connectionId, <channel, subscriptionId>>    
    private final ConcurrentHashMap<Integer, ConcurrentHashMap<String, Integer>> clientSubscriptions;

    public ConnectionsImpl() {
        handlers = new ConcurrentHashMap<>();
        channelSubscriptions = new ConcurrentHashMap<>();
        clientSubscriptions = new ConcurrentHashMap<>();
    }

    public void connect(int connectionId, ConnectionHandler<T> handler) {
        handlers.put(connectionId, handler);
        clientSubscriptions.put(connectionId, new ConcurrentHashMap<>());
    }

    @Override
    public boolean send(int connectionId, T msg) {
        ConnectionHandler<T> handler = handlers.get(connectionId);
        if (handler == null) {
            return false;
        }
        handler.send(msg);
        return true;
    }

    @Override
    public void send(String channel, T msg) {
        ConcurrentHashMap<Integer, Integer> subscribers = channelSubscriptions.get(channel);
        if (subscribers == null) {
            return;
        }
        for (Integer connectionId : subscribers.keySet()) {
            send(connectionId, msg);
        }
    }

    public void subscribe(int connectionId, String channel, int subscriptionId) {
        channelSubscriptions.computeIfAbsent(channel, k -> new ConcurrentHashMap<>()).put(connectionId, subscriptionId);
        clientSubscriptions.computeIfAbsent(connectionId, k -> new ConcurrentHashMap<>()).put(channel, subscriptionId);
    }

    public void unsubscribe(int connectionId, int subscriptionId) {

        // Get all subscriptions of this client: <channel , subscriptionId>
        ConcurrentHashMap<String, Integer> subs = clientSubscriptions.get(connectionId);
        if (subs == null)
            return;

        String channelToRemove = null;

        // Search for the channel that matches the given subscriptionId
        for (String channel : subs.keySet()) {
            if (subs.get(channel) == subscriptionId) {
                channelToRemove = channel;
                break;
            }
        }

        if (channelToRemove != null) {
            // Remove from client's subscription list
            subs.remove(channelToRemove);

            ConcurrentHashMap<Integer, Integer> channelMap = channelSubscriptions.get(channelToRemove);
            if (channelMap != null) {
                // Remove this client from the channel subscribers
                channelMap.remove(connectionId);
            }
        }
    }

    @Override
    public void disconnect(int connectionId) {
        ConnectionHandler<T> handler = handlers.remove(connectionId);
        // Remove and get all subscriptions of this client: <channel , subscriptionId>
        ConcurrentHashMap<String, Integer> subs = clientSubscriptions.remove(connectionId);

        if (subs != null) {
            // Iterate over all channels this client was subscribed to
            for (String channel : subs.keySet()) {
                ConcurrentHashMap<Integer, Integer> channelMap = channelSubscriptions.get(channel);
                if (channelMap != null) {
                    // Remove this client from the channel subscribers
                    channelMap.remove(connectionId);
                }
            }
        }

        // Close the actual connection handler
        if (handler != null) {
            try {
                handler.close();
            } catch (IOException ex) {
                // Closing failed, but connection is already removed from server state
                ex.printStackTrace();
            }
        }
    }

}
