package bgu.spl.net.impl.stomp;
import bgu.spl.net.api.MessageEncoderDecoder;
import bgu.spl.net.api.MessagingProtocol;
import bgu.spl.net.srv.Server;

public class StompServer {

    public static void main(String[] args) {
        if(args.length < 2){
            System.out.println("should get exactly 2 args : <port> <tpc/reactor>");
            System.exit(1);
        }
        int port = Integer.parseInt(args[0]);
        String serverType = args[1];

        Server<String> server;

        if(serverType.equals("tpc")){
            server = Server.threadPerClient(
                port, 
                () -> (MessagingProtocol<String>) new StompMessagingProtocolImpl(), 
                () -> (MessageEncoderDecoder<String>) new StompEncoderDecoder()
                );
            server.serve();
        } else if(serverType.equals("reactor")){
           final int NUM_THREADS = 100;
           server =  Server.reactor(NUM_THREADS,
             port,
              () -> (MessagingProtocol<String>) new StompMessagingProtocolImpl(),
               () -> (MessageEncoderDecoder<String>) new StompEncoderDecoder());
               server.serve();
        }
    }
}
