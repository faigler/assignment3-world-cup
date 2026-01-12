package bgu.spl.net.impl.stomp;

import java.util.Arrays;
import bgu.spl.net.api.MessageEncoderDecoder;

public class StompEncoderDecoder implements MessageEncoderDecoder<String> {

    private byte[] bytes = new byte[1 << 10]; // Start with 1k
    private int len = 0;

    @Override
    public String decodeNextByte(byte nextByte) {

        // End of STOMP frame
        if (nextByte == '\u0000') {
            String message = new String(bytes, 0, len);
            len = 0;
            return message;
        }

        // Expand buffer if needed
        if (len >= bytes.length) {
            bytes = Arrays.copyOf(bytes, len * 2);
        }

        // Store next byte
        bytes[len++] = nextByte;

        // Message not complete yet
        return null;
    }

    @Override
    public byte[] encode(String message) {
        // STOMP frames must end with '\0'
        return (message + "\u0000").getBytes();
    }
}


