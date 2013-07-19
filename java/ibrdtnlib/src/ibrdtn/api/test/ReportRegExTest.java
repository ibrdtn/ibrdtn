package ibrdtn.api.test;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 *
 * @author Julian Timpner <timpner@ibr.cs.tu-bs.de>
 */
public class ReportRegExTest {

    public enum ReasonCode {

        NO_ADDITIONAL_INFORMATION,
        LIFETIME_EXPIRED,
        UNIDIRECTIONAL_FORWARD,
        TRANSMISSION_CANCELED,
        DEPLETED_STORAGE,
        DESTINATION_UNINTELLIGIBLE,
        NO_KNOWN_ROUTE,
        NO_TIMELY_CONTACT,
        BLOCK_UNINTELLIGIBLE
    }

    public static void main(String[] args) {
        String start = "(603\\sNOTIFY\\sREPORT)";
        String url = "(\\bdtn://[-a-zA-Z0-9+&@#/%?=~_|!:,.;]*[-a-zA-Z0-9+&@#/%=~_|])";
        String fragments = "(\\.\\d+:\\d+)?";
        String notificationType = "(\\w+)\\[(\\d+)\\.(\\d+)\\]";

        // 603 NOTIFY REPORT <timestamp>.<seq_nr>[.<frag_offset>:<frag_len>] <src_eid> <reason_code> <type>\\[<timestamp>.<tmp_nanos>\\]
        final Pattern pattern = Pattern.compile(
                start 
                + "\\s"
                + url
                + "\\s(\\d+)\\.(\\d+)" 
                + fragments 
                + "\\s" 
                + url 
                + "\\s(\\d+)\\s" 
                + notificationType
                + "(.*)");
        final Matcher matcher = pattern.
                matcher("603 NOTIFY REPORT dtn://timpner-lx 420030842.1 dtn://timpner-lx/1 0 RECEIPT[1366715642.374735000] ");
        matcher.find();
        System.out.println("Status: " + matcher.group(1));
        System.out.println("Source: " + matcher.group(2));
        System.out.println("Timestamp: " + matcher.group(3));
        System.out.println("Seq: " + matcher.group(4));
        System.out.println("[Frag]: " + matcher.group(5));
        String frag = matcher.group(5);
        if (frag != null) {
            String[] split = frag.split("\\.|:");
            System.out.println("Offset: " + split[0]);
            System.out.println("Length: " + split[1]);
        }

        System.out.println("Dest: " + matcher.group(6));
        int reasonCode = Integer.parseInt(matcher.group(7));
        System.out.println("Reason code: " + reasonCode + ":" + ReasonCode.values()[reasonCode]);
        System.out.println("Type: " + matcher.group(8));
        System.out.println("Timestamp: " + matcher.group(9));
        System.out.println("Nanos: " + matcher.group(10));
    }
}
