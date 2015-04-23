package ibrdtn.api.object;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Connection information, only available with the according daemon extension.
 *
 * @author Julian Timpner <timpner@ibr.cs.tu-bs.de>
 */
public class NodeConnection {

    private int priority;
    private Type type = null;
    private Protocol protocol = null;
    private String connectionData;
    private static final Map<String, Protocol> protocolMap;

    static {
        Map<String, Protocol> aMap = new HashMap<>();
        aMap.put("unsupported", Protocol.CONN_UNSUPPORTED);
        aMap.put("undefined", Protocol.CONN_UNDEFINED);
        aMap.put("UDP", Protocol.CONN_UDPIP);
        aMap.put("TCP", Protocol.CONN_TCPIP);
        aMap.put("LoWPAN", Protocol.CONN_LOWPAN);
        aMap.put("Bluetooth", Protocol.CONN_BLUETOOTH);
        aMap.put("HTTP", Protocol.CONN_HTTP);
        aMap.put("FILE", Protocol.CONN_FILE);
        aMap.put("DGRAM:UDP", Protocol.CONN_DGRAM_UDP);
        aMap.put("DGRAM:ETHERNET", Protocol.CONN_DGRAM_ETHERNET);
        aMap.put("DGRAM:LOWPAN", Protocol.CONN_DGRAM_LOWPAN);
        aMap.put("P2P:WIFI", Protocol.CONN_P2P_WIFI);
        aMap.put("P2P:BT", Protocol.CONN_P2P_BT);
        protocolMap = Collections.unmodifiableMap(aMap);
    }
    private static final Map<String, Type> typeMap;

    static {
        Map<String, Type> aMap = new HashMap<>();
        aMap.put("unavailable", Type.NODE_UNAVAILABLE);
        aMap.put("discovered", Type.NODE_DISCOVERED);
        aMap.put("static global", Type.NODE_STATIC_GLOBAL);
        aMap.put("static local", Type.NODE_STATIC_LOCAL);
        aMap.put("connected", Type.NODE_CONNECTED);
        aMap.put("dht discovered", Type.NODE_DHT_DISCOVERED);
        aMap.put("p2p dialup", Type.NODE_P2P_DIALUP);

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

    public NodeConnection(String data) {
        parse(data);
    }

    private void parse(String data) {
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
    }

    public int getPriority() {
        return priority;
    }

    public Type getType() {
        return type;
    }

    public Protocol getProtocol() {
        return protocol;
    }

    public String getConnectionData() {
        return connectionData;
    }

    @Override
    public String toString() {
        return type + ":" + protocol + "#" + connectionData;
    }
}
