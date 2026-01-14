package bgu.spl.net.srv;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;
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

    @Override
    public boolean connect(int connectionId, ConnectionHandler<T> handler) {
        if (handler == null) {
            return false;
        }

        ConnectionHandler<T> existing = handlers.putIfAbsent(connectionId, handler);

        if (existing != null) {
            return false; // already connected
        }

        clientSubscriptions.put(connectionId, new ConcurrentHashMap<>());
        return true;
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
        // Not used by the STOMP protocol implementation.
        // STOMP requires sending a different MESSAGE frame per subscriber (with a
        // different subscription-id).

        // ConcurrentHashMap<Integer, Integer> subscribers =
        // channelSubscriptions.get(channel);
        // if (subscribers == null) {
        // return;
        // }
        // for (Integer connectionId : subscribers.keySet()) {
        // send(connectionId, msg);
        // }
    }

    @Override
    public boolean subscribe(int connectionId, String channel, int subscriptionId) {
        if (isSubscribed(connectionId, channel)) {
            return false; // already subscribed
        }

        // update channelSubscriptions
        ConcurrentHashMap<Integer, Integer> channelMap = channelSubscriptions.get(channel);
        if (channelMap == null) {
            channelMap = new ConcurrentHashMap<>();
            channelSubscriptions.put(channel, channelMap);
        }
        channelMap.put(connectionId, subscriptionId);

        // update clientSubscriptions
        ConcurrentHashMap<String, Integer> clientMap = clientSubscriptions.get(connectionId);
        if (clientMap == null) {
            return false;
        }
        clientMap.put(channel, subscriptionId);

        return true;
    }

    @Override
    public boolean unsubscribe(int connectionId, int subscriptionId) {

        // Get all subscriptions of this client: <channel , subscriptionId>
        ConcurrentHashMap<String, Integer> subs = clientSubscriptions.get(connectionId);
        if (subs == null)
            return false;

        String channelToRemove = null;

        // Search for the channel that matches the given subscriptionId
        for (String channel : subs.keySet()) {
            if (subs.get(channel) == subscriptionId) {
                channelToRemove = channel;
                break;
            }
        }

        if (channelToRemove == null)
            return false;

        // Remove from client's subscription list
        subs.remove(channelToRemove);

        ConcurrentHashMap<Integer, Integer> channelMap = channelSubscriptions.get(channelToRemove);
        if (channelMap != null) {
            // Remove this client from the channel subscribers
            channelMap.remove(connectionId);
        }

        return true;
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

    @Override
    public Map<Integer, Integer> getSubscribers(String channel) {
        ConcurrentHashMap<Integer, Integer> subs = channelSubscriptions.get(channel);
        if (subs == null) {
            // returns empty map
            return new HashMap<Integer, Integer>();
        }
        return new HashMap<>(subs); // snapshot
    }

    public boolean isSubscribed(int connectionId, String channel) {
        ConcurrentHashMap<String, Integer> subs = clientSubscriptions.get(connectionId);
        return subs != null && subs.containsKey(channel);
    }

}
