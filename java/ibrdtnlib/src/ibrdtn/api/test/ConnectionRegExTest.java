package ibrdtn.api.test;

import ibrdtn.api.object.NodeConnection;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 *
 * @author Julian Timpner <timpner@ibr.cs.tu-bs.de>
 */
public class ConnectionRegExTest {

    private static int priority;
    private static NodeConnection.Type type = null;
    private static NodeConnection.Protocol protocol = null;
    private static String connectionData;
    private static final Map<String, NodeConnection.Protocol> protocolMap;

    static {
        Map<String, NodeConnection.Protocol> aMap = new HashMap<>();
        aMap.put("unsupported", NodeConnection.Protocol.CONN_UNSUPPORTED);
        aMap.put("undefined", NodeConnection.Protocol.CONN_UNDEFINED);
        aMap.put("UDP", NodeConnection.Protocol.CONN_UDPIP);
        aMap.put("TCP", NodeConnection.Protocol.CONN_TCPIP);
        aMap.put("LoWPAN", NodeConnection.Protocol.CONN_LOWPAN);
        aMap.put("Bluetooth", NodeConnection.Protocol.CONN_BLUETOOTH);
        aMap.put("HTTP", NodeConnection.Protocol.CONN_HTTP);
        aMap.put("FILE", NodeConnection.Protocol.CONN_FILE);
        aMap.put("DGRAM:UDP", NodeConnection.Protocol.CONN_DGRAM_UDP);
        aMap.put("DGRAM:ETHERNET", NodeConnection.Protocol.CONN_DGRAM_ETHERNET);
        aMap.put("DGRAM:LOWPAN", NodeConnection.Protocol.CONN_DGRAM_LOWPAN);
        aMap.put("P2P:WIFI", NodeConnection.Protocol.CONN_P2P_WIFI);
        aMap.put("P2P:BT", NodeConnection.Protocol.CONN_P2P_BT);
        protocolMap = Collections.unmodifiableMap(aMap);
    }
    private static final Map<String, NodeConnection.Type> typeMap;

    static {
        Map<String, NodeConnection.Type> aMap = new HashMap<>();
        aMap.put("unavailable", NodeConnection.Type.NODE_UNAVAILABLE);
        aMap.put("discovered", NodeConnection.Type.NODE_DISCOVERED);
        aMap.put("static global", NodeConnection.Type.NODE_STATIC_GLOBAL);
        aMap.put("static local", NodeConnection.Type.NODE_STATIC_LOCAL);
        aMap.put("connected", NodeConnection.Type.NODE_CONNECTED);
        aMap.put("dht discovered", NodeConnection.Type.NODE_DHT_DISCOVERED);
        aMap.put("p2p dialup", NodeConnection.Type.NODE_P2P_DIALUP);

        typeMap = Collections.unmodifiableMap(aMap);
    }

    public enum Type {

        NODE_UNAVAILABLE(0),
        NODE_CONNECTED(1),
        NODE_DISCOVERED(2),
        NODE_STATIC_GLOBAL(3),
        NODE_STATIC_LOCAL(4),
        NODE_DHT_DISCOVERED(5),
        NODE_P2P_DIALUP(6);

        Type(int offset) {
            this.offset = offset;
        }
        int offset;

        public int getOffset() {
            return offset;
        }
    };

    public enum Protocol {

        CONN_UNSUPPORTED(-1),
        CONN_UNDEFINED(0),
        CONN_TCPIP(1),
        CONN_UDPIP(2),
        CONN_LOWPAN(3),
        CONN_BLUETOOTH(4),
        CONN_HTTP(5),
        CONN_FILE(6),
        CONN_DGRAM_UDP(7),
        CONN_DGRAM_LOWPAN(8),
        CONN_DGRAM_ETHERNET(9),
        CONN_P2P_WIFI(10),
        CONN_P2P_BT(11);

        Protocol(int offset) {
            this.offset = offset;
        }
        int offset;

        public int getOffset() {
            return offset;
        }
    };

    public static void main(String[] args) {
        // dtn://sbase.dtn
        String data = "ipn:1234 10#static local#TCP#ip=quorra.ibr.cs.tu-bs.de;port=4559";//"ipn:1337 20#discovered#TCP#ip=::ffff:134.169.35.168;port=4556";

        String[] split = data.split("\\s");

        String url = "("
                + "(\\bdtn://[-a-zA-Z0-9+&@#/%?=~_|!:,.;]*[-a-zA-Z0-9+&@#/%=~_|])" // dtn://
                + "|"
                + "(\\bipn:[0-9]*)" // ipn:
                + ")";

        final Pattern eidPattern = Pattern.compile(url);
        final Matcher eidMatcher = eidPattern.matcher(split[0]);
        if (!eidMatcher.matches()) {
            System.err.println("Unexpected EID format: " + split[0]);
        } else {
            System.out.println("EID: " + split[0]);
        }

        final Pattern pattern = Pattern.compile(
                "(\\d+)" // priority
                + "#"
                + "([\\w\\s]+)" // type
                + "#"
                + "([\\w:]+)" // protocol
                + "#"
                + "([-a-zA-Z0-9+&@#/%?=~_|!:,.;]*)" // connection data
                );

        final Matcher matcher = pattern.matcher(data);
        matcher.find();

        priority = Integer.parseInt(matcher.group(1));
        type = typeMap.get(matcher.group(2));
        protocol = protocolMap.get(matcher.group(3));
        connectionData = matcher.group(4);
        
        System.out.println("Priority: " + priority);
        System.out.println("Type: " + type);
        System.out.println("Protocol: " + protocol);
        System.out.println("ConnectionData: " + connectionData);
    }
}
