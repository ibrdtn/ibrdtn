package ibrdtn.api.sab;

import ibrdtn.api.object.EID;
import ibrdtn.api.object.SingletonEndpoint;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * A status report.
 *
 * @author Julian Timpner <timpner@ibr.cs.tu-bs.de>
 */
public class StatusReport {

    private Long timestamp = null;
    private Long sequenceNumber = null;
    private Long fragOffset = null;
    private Long fragLength = null;
    private EID source = null;
    private ReasonCode reasonCode = null;
    private Status status = null;

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

    public enum Status {

        RECEIPT,
        CUSTODY_ACCEPTANCE,
        FORWARDING,
        DELIVERY,
        DELETION
    }

    public StatusReport(String data) {
        parse(data);
    }

    private void parse(String data) {
        String start = "(NOTIFY\\sREPORT)";
        String url = "(\\bdtn://[-a-zA-Z0-9+&@#/%?=~_|!:,.;]*[-a-zA-Z0-9+&@#/%=~_|])";
        String fragments = "(\\.\\d+:\\d+)?";
        String notificationType = "(\\w+)\\[(\\d+)\\.(\\d+)\\]";

        // NOTIFY REPORT <src_eid> <timestamp>.<seq_nr>[.<frag_offset>:<frag_len>] <dst_eid> <reason_code> <type>\\[<timestamp>.<tmp_nanos>\\]
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
                + "(.*)"); // This takes care of possible repititions of <type>\\[<timestamp>.<tmp_nanos>\\]. This is not expected behavior, though.
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

        //dest = new SingletonEndpoint(matcher.group(6));

        int reason = Integer.parseInt(matcher.group(7));
        reasonCode = ReasonCode.values()[reason];

        status = Status.valueOf(matcher.group(8));

        // System.out.println("Timestamp: " + matcher.group(9));
        // System.out.println("Nanos: " + matcher.group(10));
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

    public ReasonCode getReasonCode() {
        return reasonCode;
    }

    public Status getStatus() {
        return status;
    }

    @Override
    public String toString() {
        return "[Report] " + status + ": " + source + " (" + reasonCode + ")";
    }
}
