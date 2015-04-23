package ibrdtn.api.test;

import ibrdtn.api.P2PEventListener;
import ibrdtn.api.P2PExtensionClient;
import ibrdtn.api.P2PExtensionClient.ConnectionType;
import ibrdtn.api.P2PExtensionClient.ExtensionType;
import ibrdtn.api.object.SingletonEndpoint;

import java.io.IOException;
import java.net.UnknownHostException;

import org.junit.Test;

public class P2PExtensionClientTest {

	@Test
	public void testOpen() {
		P2PExtensionClient client = new P2PExtensionClient(ExtensionType.WIFI);
		
		try {
			client.open();
			client.close();
		} catch (UnknownHostException e) {
		} catch (IOException e) {
		}
	}

	@Test
	public void testFireConnected() {
		P2PExtensionClient client = new P2PExtensionClient(ExtensionType.WIFI);	
		SingletonEndpoint eid = new SingletonEndpoint("dtn://p2p-test-node");
		
		try {
			client.open();
			client.fireConnected(eid, ConnectionType.TCP, "ip=1.2.3.4;port=1234;");	
			client.close();
		} catch (UnknownHostException e) {
		} catch (IOException e) {
		}
	}

	@Test
	public void testFireDisconnected() {
		P2PExtensionClient client = new P2PExtensionClient(ExtensionType.WIFI);	
		SingletonEndpoint eid = new SingletonEndpoint("dtn://p2p-test-node");
		
		try {
			client.open();
			client.fireDisconnected(eid, ConnectionType.TCP, "ip=1.2.3.4;port=1234;");	
			client.close();
		} catch (UnknownHostException e) {
		} catch (IOException e) {
		}
	}

	@Test
	public void testFireDiscovered() {
		P2PExtensionClient client = new P2PExtensionClient(ExtensionType.WIFI);
		
		P2PEventListener cb = new P2PEventListener() {
			@Override
			public void connect(String data) {
				System.out.println("connect request: " + data);
			}

			@Override
			public void disconnect(String data) {
				System.out.println("disconnect request: " + data);
			}
		};
		
		client.setP2PEventListener(cb);
		
		SingletonEndpoint eid = new SingletonEndpoint("dtn://p2p-test-node");
		
		try {
			client.open();
			client.fireDiscovered(eid, "hello..world");
			
			Thread.sleep(5000);
			
			client.close();
		} catch (UnknownHostException e) {
		} catch (IOException e) {
		} catch (InterruptedException e) {
		}
	}

	@Test
	public void testFireInterfaceUp() {
		P2PExtensionClient client = new P2PExtensionClient(ExtensionType.WIFI);	
		
		try {
			client.open();
			client.fireInterfaceUp("wlan0");	
			client.close();
		} catch (UnknownHostException e) {
		} catch (IOException e) {
		}
	}

	@Test
	public void testFireInterfaceDown() {
		P2PExtensionClient client = new P2PExtensionClient(ExtensionType.WIFI);	
		
		try {
			client.open();
			client.fireInterfaceDown("wlan0");	
			client.close();
		} catch (UnknownHostException e) {
		} catch (IOException e) {
		}
	}

}
