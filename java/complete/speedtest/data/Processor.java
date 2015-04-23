package ibrdtn.speedtest.data;

import ibrdtn.api.ExtendedClient;
import java.util.concurrent.ExecutorService;
import java.util.logging.Logger;

/**
 * A sample processor that triggers an automatic response to the message source.
 *
 * @author Julian Timpner <timpner@ibr.cs.tu-bs.de>
 */
public class Processor implements Runnable {

    private static final Logger logger = Logger.getLogger(Processor.class.getName());
    private Envelope envelope;
    private final ExtendedClient client;
    private final ExecutorService executor;

    public Processor(Envelope envelope, ExtendedClient client, ExecutorService executor) {
        this.envelope = envelope;
        this.client = client;
        this.executor = executor;
    }

    @Override
    public void run() {
        processMessage();
    }

    private void processMessage() {
    }
}
