import java.io.*;
import java.net.Socket;

public class StompTestClient {
    public static void main(String[] args) {
        try (Socket socket = new Socket("localhost", 7777);
             OutputStream out = socket.getOutputStream();
             InputStream in = socket.getInputStream()) {

            // 1. בדיקת CONNECT
            System.out.println("--- Testing CONNECT ---");
            sendFrame(out, "CONNECT\naccept-version:1.2\nhost:localhost\nlogin:ofek\npasscode:123\n\n");
            readFrame(in); // מצפים ל-CONNECTED

            // 2. בדיקת SUBSCRIBE עם Receipt
            System.out.println("\n--- Testing SUBSCRIBE ---");
            sendFrame(out, "SUBSCRIBE\ndestination:test-channel\nid:sub0\nreceipt:77\n\n");
            readFrame(in); // מצפים ל-RECEIPT 77

            // 3. בדיקת SEND (הפצה עצמית)
            System.out.println("\n--- Testing SEND ---");
            sendFrame(out, "SEND\ndestination:test-channel\n\nHello World!\n");
            // מכיוון שאנחנו מנויים, ה-ConnectionsImpl אמור לשלוח לנו MESSAGE
            readFrame(in); 

            // 4. בדיקת UNSUBSCRIBE
            System.out.println("\n--- Testing UNSUBSCRIBE ---");
            sendFrame(out, "UNSUBSCRIBE\nid:sub0\nreceipt:88\n\n");
            readFrame(in); // מצפים ל-RECEIPT 88

            // 5. בדיקת DISCONNECT
            System.out.println("\n--- Testing DISCONNECT ---");
            sendFrame(out, "DISCONNECT\nreceipt:99\n\n");
            readFrame(in); // מצפים ל-RECEIPT 99

            System.out.println("\nAll tests sent. Checking if server closed connection...");
            if (in.read() == -1) {
                System.out.println("Success: Server closed the socket as expected.");
            }

        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private static void sendFrame(OutputStream out, String frame) throws IOException {
        out.write((frame + "\0").getBytes()); // הוספת תו ה-NULL
        out.flush();
    }

    private static void readFrame(InputStream in) throws IOException {
        StringBuilder sb = new StringBuilder();
        int ch;
        while ((ch = in.read()) != 0 && ch != -1) {
            sb.append((char) ch);
        }
        if (sb.length() > 0) {
            System.out.println("Server Received:\n" + sb.toString());
        }
    }
}