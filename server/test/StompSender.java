import java.io.*;
import java.net.Socket;

public class StompSender {
    public static void main(String[] args) {
        try (Socket socket = new Socket("localhost", 7777);
             OutputStream out = socket.getOutputStream();
             InputStream in = socket.getInputStream()) {

            // 1. התחברות
            sendFrame(out, "CONNECT\naccept-version:1.2\nhost:localhost\nlogin:user2\npasscode:456\n\n");
            readFrame(in);

            // 2. שליחת הודעה ל-science
            System.out.println("User2: Sending message to 'science'...");
            sendFrame(out, "SEND\ndestination:science\n\nHello from User 2!\n");
            
            Thread.sleep(1000); // מחכה רגע כדי לוודא שההודעה נשלחה
            System.out.println("User2: Done.");

        } catch (Exception e) { e.printStackTrace(); }
    }

    private static void sendFrame(OutputStream out, String f) throws IOException {
        out.write((f + "\0").getBytes());
        out.flush();
    }

    private static void readFrame(InputStream in) throws IOException {
        int ch;
        while ((ch = in.read()) != 0 && ch != -1); // רק מנקה את הבאפר
    }
}