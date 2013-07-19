package ibrdtn.api.object;

import java.util.LinkedList;
import java.util.List;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * A Node instance holds connection informations of a neighboring node.
 *
 * @author Julian Timpner <timpner@ibr.cs.tu-bs.de>
 */
public class Node {

    private static final Logger logger = Logger.getLogger(Node.class.getName());
    private String eid;
    private List<NodeConnection> connections;

    public Node(String s) {
        connections = new LinkedList<>();
        parse(s);
    }

    private void parse(String data) {

        String[] split = data.split("\\s");

        String url = "("
                + "(\\bdtn://[-a-zA-Z0-9+&@#/%?=~_|!:,.;]*[-a-zA-Z0-9+&@#/%=~_|])" // dtn://
                + "|"
                + "(\\bipn:[0-9]*)" // ipn:
                + ")"; 

        final Pattern eidPattern = Pattern.compile(url);
        final Matcher eidMatcher = eidPattern.matcher(split[0]);
        if (!eidMatcher.matches()) {
            logger.log(Level.WARNING, "Unexpected eid format: {0}", split[0]);
        }

        eid = split[0];

        // dtn://dtnbucket.ibr.cs.tu-bs.de 20#discovered#TCP#ip=::ffff:134.169.35.178;port=4556; 20#discovered#UDP#ip=::ffff:134.169.35.178;port=4556;
        // dtn://sbase.dtn 20#discovered#TCP#ip=::ffff:134.169.35.168;port=4556

        for (int i = 1; i < split.length; i++) {
            NodeConnection conn = new NodeConnection(split[i]);
            connections.add(conn);
        }
    }

    public String getEid() {
        return eid;
    }

    public List<NodeConnection> getConnections() {
        return connections;
    }

    @Override
    public String toString() {
        return eid + "=" + connections;
    }
}
