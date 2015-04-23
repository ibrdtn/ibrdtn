package ibrdtn.api;

public interface P2PEventListener {
	void connect(String data);
	void disconnect(String data);
}
