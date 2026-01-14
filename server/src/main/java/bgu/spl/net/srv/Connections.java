package bgu.spl.net.srv;

import java.io.IOException;
import java.util.Map;

public interface Connections<T> {

    boolean connect(int connectionId, ConnectionHandler<T> handler);
    
    boolean send(int connectionId, T msg);

    void send(String channel, T msg);

    boolean subscribe(int connectionId, String channel, int subscriptionId);

    boolean unsubscribe(int connectionId, int subscriptionId);

    void disconnect(int connectionId);

    Map<Integer, Integer> getSubscribers(String channel);

    boolean isSubscribed(int connectionId, String channel);

}
