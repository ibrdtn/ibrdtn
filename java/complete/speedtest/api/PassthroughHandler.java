package ibrdtn.speedtest.api;

import ibrdtn.api.ExtendedClient;
import ibrdtn.api.object.Block;
import ibrdtn.api.object.Bundle;
import ibrdtn.api.object.BundleID;
import ibrdtn.api.sab.Custody;
import ibrdtn.api.sab.StatusReport;
import java.io.IOException;
import java.io.OutputStream;
import java.io.PipedInputStream;
import java.io.PipedOutputStream;
import java.util.concurrent.ExecutorService;
import java.util.logging.Level;
import java.util.logging.Logger;

/**
 * The application's custom handler for Simple API for Bundle Protocol (SAB) events, such as reception of a bundle. The
 * PassthroughHandler directly loads a new bundle after being notified of it.
 *
 * @author Julian Timpner <timpner@ibr.cs.tu-bs.de>
 */
public class PassthroughHandler extends AbstractAPIHandler {

    private static final Logger logger = Logger.getLogger(PassthroughHandler.class.getName());

    public PassthroughHandler(ExtendedClient client, ExecutorService executor, PayloadType messageType) {
        this.client = client;
        this.executor = executor;
        this.messageType = messageType;
    }

    @Override
    public void notify(final BundleID id) {
        loadAndGet(id);
    }

    @Override
    public void notify(StatusReport r) {
        logger.log(Level.INFO, r.toString());
    }

    @Override
    public void notify(Custody c) {
        logger.log(Level.INFO, c.toString());
    }

    @Override
    public void startBundle(Bundle bundle) {
        logger.log(Level.FINE, "Receiving: {0}", bundle);
        this.bundle = bundle;
    }

    @Override
    public void endBundle() {
        // logger.log(Level.INFO, "Received: {0}:", bundle);

        /*
         * Bundle needs to be marked delivered before processing starts concurrently, as the transfer of new Bundles 
         * might interfere otherwise.
         */
        markDelivered();

//        executor.execute(new Processor(envelope, client, executor));
    }

    @Override
    public void startBlock(Block block) {
        //logger.log(Level.FINE, "Receiving: {0}", block.toString());
        bundle.appendBlock(block);
    }

    @Override
    public void endBlock() {
        //logger.log(Level.FINE, "Ending block");
    }

    @Override
    public OutputStream startPayload() {
        //logger.log(Level.INFO, "Receiving payload");
        /*
         * For a detailed description of how different streams affect efficiency, consult:
         * 
         * code.google.com/p/io-tools
         */
        is = new PipedInputStream();
        try {
            os = new PipedOutputStream(is);

            // Concurrently read data from stream lest the buffer runs full and creates a deadlock situation
            t = new Thread(new PipedStreamReader());
            t.start();

        } catch (IOException ex) {
            logger.log(Level.SEVERE, "Opening pipes failed", ex);
        }

        return os;
    }

    @Override
    public void endPayload() {
        if (os != null) {
            try {
                t.join();
            } catch (Exception ex) {
                logger.log(Level.SEVERE, null, ex);
            } finally {
                try {
                    if (os != null) {
                        os.close();
                    }
                } catch (IOException ex) {
                    logger.log(Level.SEVERE, "Failed to close streams", ex);
                }
            }
        }
    }

    @Override
    public void progress(long pos, long total) {
        logger.log(Level.INFO, "Payload: {0} of {1} bytes", new Object[]{pos, total});
    }
}
