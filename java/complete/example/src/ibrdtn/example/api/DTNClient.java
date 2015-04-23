package ibrdtn.example.api;

import ibrdtn.api.APIException;
import ibrdtn.api.EventClient;
import ibrdtn.api.ExtendedClient;
import ibrdtn.api.object.Bundle;
import java.io.IOException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
import java.util.logging.Level;
import java.util.logging.Logger;

/**
 * A thin wrapper around the IBR-DTN Java Library that takes care of connecting to the daemon and provides some
 * convenience functions.
 *
 * @author Julian Timpner <timpner@ibr.cs.tu-bs.de>
 */
public class DTNClient {

    private static final Logger logger = Logger.getLogger(DTNClient.class.getName());
    private ExecutorService executor;
    private ExtendedClient exClient = null;
    private EventClient eventClient = null;
    private ibrdtn.api.sab.CallbackHandler sabHandler = null;
    private String endpoint = null;
    private final APIHandlerType apiType;
    private final PayloadType payloadType;

    /**
     * Default constructor, using byte[] as the expected payload format and passthrough as the API handling strategy.
     *
     * @param endpoint the application's primary EID
     */
    public DTNClient(String endpoint) {

        this(endpoint, PayloadType.BYTE);
    }

    /**
     * Constructor allowing to choose between different payload types. Uses passthrough as the API handling strategy.
     *
     * @param endpoint the application's primary EID
     * @param type the expected payload format
     */
    public DTNClient(String endpoint, PayloadType type) {
        this(endpoint, type, APIHandlerType.PASSTHROUGH);
    }

    /**
     * Constructor allowing to choose between different payload types and API handling strategies.
     *
     * @param endpoint the application's primary EID
     * @param payloadType the expected payload format
     * @param apiType the API handling strategy
     */
    public DTNClient(String endpoint, PayloadType payloadType, APIHandlerType apiType) {
        executor = Executors.newSingleThreadExecutor();

        this.endpoint = endpoint;
        this.payloadType = payloadType;
        this.apiType = apiType;

        exClient = new ExtendedClient();

        switch (apiType) {
            case PASSTHROUGH:
                sabHandler = new PassthroughHandler(exClient, executor, payloadType);
                break;
            case SELECTIVE:
                sabHandler = new SelectiveHandler(exClient, executor, payloadType);
                break;
            /* case WHATEVER:
             * //implement your own;
             * break;
             */
        }

        exClient.setHandler(sabHandler);
        exClient.setHost(Constants.HOST);
        exClient.setPort(Constants.PORT);

        connect();
    }

    /**
     * Opens the API connection to the DTN daemon.
     */
    private void connect() {
        try {
            exClient.open();
            logger.log(Level.FINE, "Successfully connected to DTN daemon");

            exClient.setEndpoint(endpoint);
            logger.log(Level.INFO, "Endpoint ''{0}'' registered.", endpoint);

        } catch (APIException e) {
            logger.log(Level.WARNING, "API error: {0}", e.getMessage());
        } catch (IOException e) {
            logger.log(Level.WARNING, "Could not connect to DTN daemon: {0}", e.getMessage());
        }
    }

    /**
     * Sends the given Bundle to the daemon.
     *
     * @param bundle
     */
    public void send(Bundle bundle) {

        logger.log(Level.INFO, "Sending {0}", bundle);

        final Bundle finalBundle = bundle;
        final ExtendedClient finalClient = this.exClient;

        executor.execute(new Runnable() {
            @Override
            public void run() {

                try {
                    finalClient.send(finalBundle);
                } catch (Exception e) {
                    logger.log(Level.SEVERE, "Unable to send bundle", e);
                }
            }
        });
    }

    /**
     * Shuts down the API connection.
     */
    public void shutdown() {

        logger.log(Level.INFO, "Shutting down {0}", endpoint);

        executor.shutdown(); // Disable new tasks from being submitted
        try {

            if (!executor.awaitTermination(8, TimeUnit.SECONDS)) {

                logger.log(Level.FINE, "Thread pool did not terminate on shutdown");

                executor.shutdownNow(); // Cancel currently executing tasks
                // Wait a while for tasks to respond to being cancelled
                if (!executor.awaitTermination(5, TimeUnit.SECONDS)) {
                    logger.log(Level.SEVERE, "Thread pool did not terminate");
                }
            }
        } catch (InterruptedException ie) {

            logger.log(Level.SEVERE, "Shutdown interrupted: {0}", ie.getMessage());
            // (Re-)Cancel if current thread also interrupted
            executor.shutdownNow();
            // Preserve interrupt status
            Thread.currentThread().interrupt();
        }

        try {
            exClient.close();
            if (eventClient != null) {
                eventClient.close();
            }
        } catch (IOException e) {
            logger.log(Level.WARNING, "Could not close connection to the DTN daemon: {0}", e.getMessage());
        }

        logger.log(Level.INFO, "DTN connection closed");
    }

    /**
     * Enables/disables event notifications from the daemon.
     *
     * These are different from (custody) reports, which are received anyway.
     *
     * @param eventNotifications true if event notifications are to be enabled
     */
    public void setEvents(boolean eventNotifications) {
        if (eventNotifications) {
            EventNotifier notifier = new EventNotifier();
            eventClient = new EventClient(notifier);
            try {
                eventClient.open();
            } catch (IOException e) {
                logger.log(Level.WARNING, "Could not connect to DTN daemon: {0}", e.getMessage());
            }
        } else {
            if (eventClient != null) {
                try {
                    eventClient.close();
                } catch (IOException e) {
                    logger.log(Level.WARNING, "Closing EventClient connection failed: {0}", e.getMessage());
                }
            }
        }
    }

    public String getConfiguration() {
        return "DTN Client: " + endpoint + " [" + Constants.HOST + ":" + Constants.PORT + "]";
    }

    public APIHandlerType getApiType() {
        return apiType;
    }

    public PayloadType getPayloadType() {
        return payloadType;
    }

    public ExtendedClient getEC() {
        return this.exClient;
    }
}