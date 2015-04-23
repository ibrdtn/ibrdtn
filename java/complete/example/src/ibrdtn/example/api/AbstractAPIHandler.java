package ibrdtn.example.api;

import ibrdtn.api.APIException;
import ibrdtn.api.ExtendedClient;
import ibrdtn.api.object.Bundle;
import ibrdtn.api.object.BundleID;
import ibrdtn.example.data.Envelope;
import ibrdtn.example.data.MessageData;
import static ibrdtn.example.api.PayloadType.BYTE;
import static ibrdtn.example.api.PayloadType.OBJECT;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.PipedInputStream;
import java.io.PipedOutputStream;
import java.util.concurrent.ExecutorService;
import java.util.logging.Level;
import java.util.logging.Logger;

/**
 * Abstracts functionality common to all classes implementing a CallbackHandler, such as loading Bundles into the Bundle
 * register or marking Bundles as delivered.
 *
 * @author Julian Timpner <timpner@ibr.cs.tu-bs.de>
 */
public abstract class AbstractAPIHandler implements ibrdtn.api.sab.CallbackHandler {

    private static final Logger logger = Logger.getLogger(AbstractAPIHandler.class.getName());
    protected PipedInputStream is;
    protected PipedOutputStream os;
    protected ExtendedClient client;
    protected ExecutorService executor;
    protected Bundle bundle = null;
    protected PayloadType messageType;
    protected Thread t;
    protected Envelope envelope;
    protected byte[] bytes;

    /**
     * Marks the Bundle currently in the register as delivered.
     */
    protected void markDelivered() {
        final BundleID finalBundleID = new BundleID(bundle);
        final ExtendedClient finalClient = this.client;

        executor.execute(new Runnable() {
            @Override
            public void run() {
                // Example: add message to database
                // Message msg = new Message(received.source, received.destination, playfile);
                // msg.setCreated(received.timestamp);
                // msg.setReceived(new Date());
                // _database.put(Folder.INBOX, msg);

                try {
                    // Mark bundle as delivered...                    
                    finalClient.markDelivered(finalBundleID);
                    logger.log(Level.FINE, "Delivered: {0}", finalBundleID);
                } catch (Exception e) {
                    logger.log(Level.SEVERE, "Unable to mark bundle as delivered.", e);
                }
            }
        });
    }

    /**
     * Loads the given bundle from the queue into the register and initiates the file transfer.
     */
    protected void loadAndGet(BundleID bundleId) {
        final BundleID finalBundleId = bundleId;
        final ExtendedClient exClient = this.client;

        executor.execute(new Runnable() {
            @Override
            public void run() {
                try {
                    exClient.loadBundle(finalBundleId);
                    exClient.getBundle();
                    logger.log(Level.INFO, "New bundle loaded");
                } catch (APIException e) {
                    logger.log(Level.WARNING, "Failed to load next bundle");
                }
            }
        });
    }

    /**
     * Loads the next bundle from the queue into the register and initiates transfer of the Bundle's meta data.
     */
    protected void loadAndGetInfo(BundleID bundleId) {
        final BundleID finalBundleId = bundleId;
        final ExtendedClient exClient = this.client;
        executor.execute(new Runnable() {
            @Override
            public void run() {
                try {
                    exClient.loadBundle(finalBundleId);
                    exClient.getBundleInfo();
                    logger.log(Level.INFO, "New bundle loaded, getting meta data");
                } catch (APIException e) {
                    logger.log(Level.WARNING, "Failed to load next bundle");
                }
            }
        });
    }

    /**
     * Initiates transfer of the Bundle's payload. Requires loading the Bundle into the register first.
     */
    protected void getPayload() {
        final ExtendedClient finalClient = this.client;
        executor.execute(new Runnable() {
            @Override
            public void run() {
                try {
                    logger.log(Level.INFO, "Requesting payload");
                    finalClient.getPayload();
                } catch (Exception e) {
                    logger.log(Level.SEVERE, "Unable to get bundle payload.", e);
                }
            }
        });
    }

    /**
     * Concurrently reads from a PipedInputStream lest the streams internal buffer runs full and creates a deadlock
     * situation.
     */
    class PipedStreamReader implements Runnable {

        @Override
        public void run() {
            switch (messageType) {

                case BYTE:
                    ByteArrayOutputStream buffer;

                    try {

                        buffer = new ByteArrayOutputStream();

                        int nRead;
                        byte[] data = new byte[16384];

                        while ((nRead = is.read(data, 0, data.length)) != -1) {
                            buffer.write(data, 0, nRead);
                        }

                        buffer.flush();

                        bytes = buffer.toByteArray();

                        StringBuilder sb = new StringBuilder();

                        for (byte b : bytes) {
                            sb.append(String.format("%02X ", b));
                        }

                        logger.log(Level.INFO, "Payload received: \n\t{0} [{1}]",
                                new Object[]{sb.toString(), new String(bytes)});
                    } catch (IOException ex) {
                        logger.log(Level.SEVERE, "Unable to decode payload");
                    }
                    break;

                case OBJECT:
                    ObjectInputStream ois = null;
                    try {
                        ois = new ObjectInputStream(is);

                        Object object = ois.readObject();

                        // Check object type
                        String messageType = object.getClass().getCanonicalName();
                        if (messageType.equals(MessageData.class.getCanonicalName())) {

                            MessageData messageData = (MessageData) object;
                            envelope = new Envelope();
                            envelope.setBundleID(new BundleID(bundle));
                            envelope.setData(messageData);

                            logger.log(Level.INFO, "Data received: {0}", envelope);
                            // Do further processing, for instance

                        } else {
                            logger.log(Level.WARNING, "Unknown Message Type ''{0}''", messageType);
                        }

                    } catch (IOException | ClassNotFoundException ex) {
                        logger.log(Level.SEVERE, "Unable to decode payload");
                    } finally {
                        try {
                            if (ois != null) {
                                ois.close();
                            }
                        } catch (IOException ex) {
                            logger.log(Level.SEVERE, "Failed to close streams", ex);
                        }
                    }
                    break;
            }
        }
    }
}
