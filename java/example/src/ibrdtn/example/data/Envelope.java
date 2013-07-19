package ibrdtn.example.data;

import ibrdtn.api.object.BundleID;

/**
 * A custom message format combining the actual payload and meta data in form of a BundleID.
 *
 * @author Julian Timpner <timpner@ibr.cs.tu-bs.de>
 */
public class Envelope {

    private BundleID bundleID;
    private MessageData data;

    public Envelope() {
    }

    public BundleID getBundleID() {
        return bundleID;
    }

    public void setBundleID(BundleID bundleID) {
        this.bundleID = bundleID;
    }

    public MessageData getData() {
        return data;
    }

    public void setData(MessageData data) {
        this.data = data;
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append("\n\t").append(getBundleID());
        sb.append("\n").append(getData());
        return sb.toString();
    }
}
