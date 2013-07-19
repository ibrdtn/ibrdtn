package ibrdtn.example.api;

import ibrdtn.api.ExtendedClient;
import ibrdtn.api.object.Block;
import ibrdtn.api.object.Bundle;
import ibrdtn.api.object.BundleID;
import ibrdtn.api.sab.Custody;
import ibrdtn.api.sab.StatusReport;
import ibrdtn.example.data.Processor;
import java.io.IOException;
import java.io.OutputStream;
import java.io.PipedInputStream;
import java.io.PipedOutputStream;
import java.util.concurrent.ExecutorService;
import java.util.logging.Level;
import java.util.logging.Logger;

/**
 * The application's custom handler for Simple API for Bundle Protocol (SAB) events, such as reception of a bundle. The
 * SelectiveHandler only loads bundle meta data at first, thus allowing for selective bundle loading.
 *
 * @author Julian Timpner <timpner@ibr.cs.tu-bs.de>
 */
public class SelectiveHandler extends AbstractAPIHandler {

    private static final Logger logger = Logger.getLogger(SelectiveHandler.class.getName());

    public SelectiveHandler(ExtendedClient client, ExecutorService executor, PayloadType messageType) {
        this.client = client;
        this.executor = executor;
        this.messageType = messageType;
    }

    @Override
    public void notify(BundleID id) {
        loadAndGetInfo(id);
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
        logger.log(Level.INFO, "Bundle info received");

        /*
         * Decide if payload is interesting, based on bundle meta data, e.g., payload length of the 
         * individual block. If applicable, partial payloads can be requested, as well. 
         * 
         * If payload is not to be loaded, markDelivered();
         */
        boolean isPayloadInteresting = true;

        if (isPayloadInteresting) {
            getPayload();
        } else {
            markDelivered();
        }
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
            } catch (InterruptedException ex) {
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

        /*
         * With the SelectiveHandler, when the payload has been received, the bundle is complete and can be deleted
         * 
         * Bundle needs to be marked delivered before processing starts concurrently, as the transfer of new Bundles 
         * might interfere otherwise.
         */
        markDelivered();

        executor.execute(new Processor(envelope, client, executor));
    }

    @Override
    public void progress(long pos, long total) {
        logger.log(Level.INFO, "Payload: {0} of {1} bytes", new Object[]{pos, total});
    }
}
