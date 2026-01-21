import java.io.*;
import java.net.Socket;
import java.nio.charset.StandardCharsets;

public class StompListener {
    public static void main(String[] args) {
        // שימוש ב-UTF-8 כדי לתמוך בעברית ובתווים מיוחדים
        try (Socket socket = new Socket("localhost", 7777);
             OutputStream out = socket.getOutputStream();
             InputStream in = socket.getInputStream()) {

            // 1. התחברות
            sendFrame(out, "CONNECT\naccept-version:1.2\nhost:localhost\nlogin:user1\npasscode:123\n\n");
            
            // 2. הרשמה (עדיף לשלוח הכל ואז להיכנס ללולאת האזנה אחת)
            sendFrame(out, "SUBSCRIBE\ndestination:science\nid:sub10\n\n");
            System.out.println("User1: Connected and Subscribed. Waiting for messages...");

            // 3. לולאת האזנה אחודה - מטפלת בכל הפריימים הנכנסים
            while (true) {
                String frame = readFrame(in);
                if (frame == null) {
                    System.out.println("Server disconnected.");
                    break; // יוצא מהלולאה אם הסוקט נסגר
                }
                if (!frame.isEmpty()) {
                    System.out.println("\n[" + System.currentTimeMillis() + "] --- User1 Received ---");
                    System.out.println("\n--- User1 Received ---\n" + frame);
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private static void sendFrame(OutputStream out, String f) throws IOException {
        out.write((f + "\0").getBytes(StandardCharsets.UTF_8));
        out.flush();
    }

    private static String readFrame(InputStream in) throws IOException {
        ByteArrayOutputStream buffer = new ByteArrayOutputStream();
        int ch;
        
        // קריאת בתים עד לתו ה-null
        while ((ch = in.read()) != 0) {
            if (ch == -1) return null; // סגירת סוקט
            buffer.write(ch);
        }
        
        // הפיכת כל הבתים שקראנו למחרוזת אחת בקידוד הנכון
        return new String(buffer.toByteArray(), StandardCharsets.UTF_8);
    }
}