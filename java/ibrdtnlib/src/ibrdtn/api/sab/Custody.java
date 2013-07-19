package ibrdtn.api.sab;

import ibrdtn.api.object.EID;
import ibrdtn.api.object.SingletonEndpoint;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * A custody report.
 *
 * @author Julian Timpner <timpner@ibr.cs.tu-bs.de>
 */
public class Custody {

    private Long timestamp = null;
    private Long sequenceNumber = null;
    private Long fragOffset = null;
    private Long fragLength = null;
    private EID source = null;
    private Custody.Status status = null;
    private Custody.Reason reason = null;

    public enum Reason {

        NO_ADDITIONAL_INFORMATION(0),
        REDUNDANT_RECEPTION(3),
        DEPLETED_STORAGE(4),
        DESTINATION_ENDPOINT_UNINTELLIGIBLE(5),
        NO_KNOWN_ROUTE(6),
        NO_TIMELY_CONTACT(7),
        BLOCK_UNINTELLIGIBLE(8);
        private int offset;

        Reason(int offset) {
            this.offset = offset;
        }

        public int getOffset() {
            return offset;
        }
    }

    public enum Status {

        ACCEPTED, REJECTED
    }

    public Custody(String data) {
        parse(data);
    }

    private void parse(String data) {
        String start = "(NOTIFY\\sCUSTODY)";
        String url = "(\\bdtn://[-a-zA-Z0-9+&@#/%?=~_|!:,.;]*[-a-zA-Z0-9+&@#/%=~_|])";
        String fragments = "(\\.\\d+:\\d+)?";

        // 604 NOTIFY CUSTODY <src_eid> <timestamp>.<seq_nr>[.<frag_offset>:<frag_len>] <dest_eid> <ACCEPTED|REJECTED(<reason_code>)> <timestamp>.<tmp_nanos>
        final Pattern pattern = Pattern.compile(
                start // NOTIFY CUSTODY
                + "\\s"
                + url // <src_eid>
                + "\\s(\\d+)\\.(\\d+)" // <timestamp>.<seq_nr>
                + fragments //[.<frag_offset>:<frag_len>]
                + "\\s"
                + url // <dst_eid>
                + "\\s(\\w+)\\b" // ACCEPTED|REJECTED
                + "(\\((\\d+)\\))?" // <reason_code>
                + "\\s"
                + "(\\d+)\\.(\\d+)"); // <timestamp>.<tmp_nanos>

        final Matcher matcher = pattern.matcher(data);
        matcher.find();

        source = new SingletonEndpoint(matcher.group(2));
        timestamp = Long.parseLong(matcher.group(3));
        sequenceNumber = Long.parseLong(matcher.group(4));

        String frag = matcher.group(5);
        if (frag != null) {
            String[] split = frag.split("\\.|:");
            fragOffset = Long.parseLong(split[0]);
            fragLength = Long.parseLong(split[1]);
        }

        // dest = new SingletonEndpoint(matcher.group(6));
        status = Status.valueOf(matcher.group(7));

        String rawReason = matcher.group(9);
        if (rawReason != null) {
            int reasonCode = Integer.parseInt(rawReason);
            this.reason = Reason.values()[reasonCode];
        }

        // System.out.println("Timestamp: " + matcher.group(10));
        // System.out.println("Nanos: " + matcher.group(11));
    }

    public Long getTimestamp() {
        return timestamp;
    }

    public Long getSequenceNumber() {
        return sequenceNumber;
    }

    public Long getFragOffset() {
        return fragOffset;
    }

    public Long getFragLength() {
        return fragLength;
    }

    public EID getSource() {
        return source;
    }

    public Status getStatus() {
        return status;
    }

    public Reason getReason() {
        return reason;
    }

    @Override
    public String toString() {
        if (reason != null) {
            return "[Custody] " + status + ": " + source + " +(" + reason + ")";
        } else {
            return "[Custody] " + status + ": " + source;
        }
    }
}
