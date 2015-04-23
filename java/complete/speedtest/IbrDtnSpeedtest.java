package ibrdtn.speedtest;

import ibrdtn.api.object.Bundle;
import ibrdtn.api.object.EID;
import ibrdtn.api.object.PayloadBlock;
import ibrdtn.api.object.SingletonEndpoint;
import ibrdtn.speedtest.api.APIHandlerType;
import ibrdtn.speedtest.api.DTNClient;
import ibrdtn.speedtest.api.PayloadType;
import java.net.Socket;
import java.util.concurrent.TimeUnit;

/**
 *
 * @author timpner
 */
public class IbrDtnSpeedtest {

    private static DTNClient dtnClient;
    private static EID DESTINATION = new SingletonEndpoint("dtn://timpner-lx/bla");
    private static long LIFETIME = 3600L;
    private static String DATA = "0xC0FFEE";
    private static int COUNT = 1000;

    /**
     * @param args the command line arguments
     */
    public static void main(String[] args) {
        // Init connection to daemon
        dtnClient = new DTNClient("speedtest", PayloadType.BYTE, APIHandlerType.PASSTHROUGH);
        System.out.println(dtnClient.getConfiguration());

        // Create bundle to send
        Bundle bundle = new Bundle(DESTINATION, LIFETIME);
        bundle.setPriority(Bundle.Priority.NORMAL);
        String data = DATA;
        bundle.appendBlock(new PayloadBlock(data.getBytes()));

        long before = System.currentTimeMillis();

        for (int i = 0; i < COUNT; i++) {
            dtnClient.send(bundle);
        }
        try {
            dtnClient.executor.shutdown();
            dtnClient.executor.awaitTermination(3, TimeUnit.DAYS);
        } catch (InterruptedException ex) {
            System.out.println("Interrupted");
        }
        long after = System.currentTimeMillis();
        long runningTime = after - before;

        System.out.println("Running time: " + runningTime);
    }
}
