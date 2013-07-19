/*
 * ExtendedClient.java
 * 
 * Copyright (C) 2011 IBR, TU Braunschweig
 *
 * Written-by: 
 *  Johannes Morgenroth <morgenroth@ibr.cs.tu-bs.de>
 *  Julian Timpner      <timpner@ibr.cs.tu-bs.de>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
package ibrdtn.api;

import ibrdtn.api.object.Bundle;
import ibrdtn.api.object.BundleID;
import ibrdtn.api.object.EID;
import ibrdtn.api.object.GroupEndpoint;
import ibrdtn.api.object.InputStreamBlockData;
import ibrdtn.api.object.Node;
import ibrdtn.api.object.PayloadBlock;
import ibrdtn.api.object.PlainSerializer;
import ibrdtn.api.object.SelfEncodingObject;
import ibrdtn.api.object.SelfEncodingObjectBlockData;
import ibrdtn.api.object.SingletonEndpoint;
import ibrdtn.api.sab.CallbackHandler;
import ibrdtn.api.sab.Response;
import java.io.BufferedWriter;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.ObjectInputStream;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.net.UnknownHostException;
import java.util.LinkedList;
import java.util.List;
import java.util.logging.Level;
import java.util.logging.Logger;

public class ExtendedClient extends Client {

    private static final Logger logger = Logger.getLogger(ExtendedClient.class.getName());
    private final Object connection_mutex = new Object();
    private final Object state_mutex = new Object();
    private final Object handler_mutex = new Object();
    private State state = State.UNINITIALIZED;
    private BufferedWriter _writer = null;
    private DataReceiver _receiver = null;
    private Boolean _debug = false;
    private EID remoteEID = null;
    protected CallbackHandler handler = null;

    public enum Encoding {

        BASE64,
        RAW
    }

    private enum State {

        UNINITIALIZED,
        CONNECTING,
        CONNECTED,
        CLOSED,
        CLOSING,
        FAILED
    }

    /**
     * Creates an instance of the extended client interface. By default this is not connected to a daemon.
     */
    public ExtendedClient() {
        super();
    }

    private void setState(State s) {
        synchronized (state_mutex) {
            state = s;
            state_mutex.notifyAll();
        }
    }

    /**
     * Enables / disables the debug output.
     *
     * @param val true if debugging output required
     */
    public void setDebug(Boolean val) {
        this._debug = val;
    }

    protected void debug(String msg) {
        if (!this._debug) {
            return;
        }
        logger.log(Level.INFO, msg);
    }

    /**
     * Sets the handler for incoming API events.
     *
     * @param handler the handler object derived from SABHandler
     */
    public void setHandler(CallbackHandler handler) {
        synchronized (handler_mutex) {
            this.handler = handler;
            if (_receiver != null) {
                _receiver.abort();
            }
            _receiver = new DataReceiver(this, handler_mutex, handler);
        }
    }

    /*
     * (non-Javadoc)
     * @see ibrdtn.api.Client#open()
     */
    @Override
    public void open() throws UnknownHostException, IOException {
        synchronized (state_mutex) {
            // just return if the connection is already open
            if (state != State.UNINITIALIZED) {
                throw new IOException("client in invalid state for open()");
            }
            setState(State.CONNECTING);
        }

        try {
            super.open();

            synchronized (connection_mutex) {
                this._writer = new BufferedWriter(new OutputStreamWriter(this.ostream));
            }

            // run the notify receiver
            if (handler == null) {
                _receiver = new DataReceiver(this, handler_mutex, handler);
            }
            _receiver.start();

            // switch to extended protocol
            if (query("protocol extended") != 200) {
                // error
                throw new APIException("protocol switch failed");
            }
        } catch (APIException ex) {
            _writer.close();
            _writer = null;
            if (_receiver != null) {
                _receiver.abort();
                _receiver = null;
            }
            setState(State.UNINITIALIZED);
            throw new IOException(ex.getMessage());
        } catch (UnknownHostException e) {
            setState(State.FAILED);
            throw e;
        } catch (IOException e) {
            setState(State.UNINITIALIZED);
            throw e;
        }

        // set state to connected
        setState(State.CONNECTED);
    }

    /*
     * (non-Javadoc)
     * @see ibrdtn.api.Client#close()
     */
    @Override
    public void close() throws IOException {
        synchronized (state_mutex) {
            try {
                while (state == State.CONNECTING) {
                    state_mutex.wait();
                }
            } catch (InterruptedException e) {
                setState(State.FAILED);
                return;
            }

            // just return if the connection is not open
            if (state != State.CONNECTED) {
                return;
            }
            setState(State.CLOSING);
        }

        try {
            synchronized (connection_mutex) {
                if (_writer != null) {
                    _writer.close();
                }
                if (_receiver != null) {
                    _receiver.abort();
                    _receiver = null;
                }
                super.close();
            }
        } catch (IOException e) {
            setState(State.FAILED);
            throw e;
        }

        // set state to connected
        setState(State.CLOSED);
    }

    @Override
    public synchronized Boolean isConnected() {
        synchronized (state_mutex) {
            try {
                while (state == State.CONNECTING) {
                    state_mutex.wait();
                }
            } catch (InterruptedException e) {
                return false;
            }

            return (state == State.CONNECTED);
        }
    }

    @Override
    public synchronized Boolean isClosed() {
        synchronized (state_mutex) {
            try {
                while (state == State.CONNECTING) {
                    state_mutex.wait();
                }
            } catch (InterruptedException e) {
                return false;
            }

            return (state != State.CONNECTED);
        }
    }

    protected void mark_error() {
        setState(State.FAILED);

        try {
            synchronized (connection_mutex) {
                if (_writer != null) {
                    _writer.close();
                }
                if (_receiver != null) {
                    _receiver.abort();
                    _receiver = null;
                }
                super.close();
            }
        } catch (IOException e) {
        }
    }

    public void setEncoding(Encoding encoding) throws APIException {
        if (query("set encoding " + encoding.toString().toLowerCase()) != 200) {
            // error
            throw new APIException("encoding switch failed");
        }
    }

    /**
     * Executes a NOOP command to the API. Just checking that this connection is active.
     *
     * @throws APIException if the connection has been terminated or the received response is wrong
     */
    public synchronized void noop() throws APIException {
        // throw exception if not connected
        if (state != State.CONNECTED) {
            throw new APIException("not connected");
        }

        // send a message with no effect
        send("noop");

        // read answer
        if (_receiver.getResponse().getCode() != 400) {
            // error
            throw new APIException("noop failed");
        }
    }

    /**
     * Sends a bundle directly to the daemon.
     *
     * @param bundle the bundle to send
     * @throws APIException if the transmission fails
     */
    public synchronized void send(Bundle bundle) throws APIException {
        // throw exception if not connected
        if (state != State.CONNECTED) {
            throw new APIException("not connected");
        }

        // clear the previous bundle first
        if (query("bundle clear") != 200) {
            throw new APIException("bundle clear failed");
        }

        // announce a proceeding plain bundle
        if (query("bundle put plain") != 100) {
            throw new APIException("bundle put failed");
        }

        PlainSerializer serializer = new PlainSerializer(ostream);

        try {
            serializer.serialize(bundle);
        } catch (IOException e) {
            throw new APIException("serialization of bundle failed.");
        }

        if (_receiver.getResponse().getCode() != 200) {
            throw new APIException("bundle rejected or put failed");
        }

        // send the bundle away
        if (query("bundle send") != 200) {
            throw new APIException("bundle send failed");
        }
    }

    /**
     * Sends a bundle directly to the daemon. The given payload has to be encoded as base64 first.
     *
     * @param destination the destination of the bundle
     * @param lifetime the lifetime of the bundle
     * @param base64 the base64 encoded payload
     * @param length the length of the un-encoded payload
     * @throws APIException If the transmission fails
     */
    @Deprecated
    public synchronized void send(EID destination, Integer lifetime, String base64, Long length) throws APIException {
        // throw exception if not connected
        if (state != State.CONNECTED) {
            throw new APIException("not connected");
        }

        // clear the previous bundle first
        if (query("bundle clear") != 200) {
            throw new APIException("bundle clear failed");
        }

        // announce a proceeding plain bundle
        if (query("bundle put plain") != 100) {
            throw new APIException("bundle put failed");
        }

        // upload bundle to the daemon
        if (destination instanceof GroupEndpoint) {
            startBundle(0, destination, lifetime, 1);
        } else {
            startBundle(16, destination, lifetime, 1);
        }

        startBlock(1, length, true);

        send(base64);
        send("");

        if (_receiver.getResponse().getCode() != 200) {
            // error
            throw new APIException("bundle rejected or put failed");
        }

        // send the bundle away
        if (query("bundle send") != 200) {
            throw new APIException("bundle send failed");
        }
    }

    /**
     * Sends a bundle directly to the daemon. The given payload has to be encoded as base64 first.
     *
     * @param destination the destination of the bundle
     * @param lifetime the lifetime of the bundle
     * @param base64 the base64 encoded payload
     * @param length the length of the un-encoded payload
     * @throws APIException if the transmission fails
     */
    @Deprecated
    public void send(EID destination, Integer lifetime, String base64, Integer length) throws APIException {
        send(destination, lifetime, base64, new Long(length));
    }

    /**
     * Sends a bundle directly to the daemon.
     *
     * @param destination the destination of the bundle
     * @param lifetime the lifetime of the bundle
     * @param data the payload as byte-array
     * @throws APIException if the transmission fails
     */
    public void send(EID destination, Integer lifetime, byte[] data) throws APIException {
        send(destination, lifetime, data, Bundle.Priority.NORMAL);
    }

    /**
     * Sends a bundle directly to the daemon.
     *
     * @param destination the destination of the bundle
     * @param lifetime the lifetime of the bundle
     * @param data the payload as byte-array
     * @param priority the priority of the bundle
     * @throws APIException if the transmission fails
     */
    public void send(EID destination, Integer lifetime, byte[] data, Bundle.Priority priority) throws APIException {
        // wrapper to the send(Bundle) function
        Bundle bundle = new Bundle(destination, lifetime);
        bundle.appendBlock(new PayloadBlock(data));
        bundle.setPriority(priority);
        send(bundle);
    }

    /**
     * Sends a bundle directly to the daemon. The given object has to encode its own payload into base64 data.
     *
     * @param destination the destination of the bundle
     * @param lifetime the lifetime of the bundle
     * @param obj the self-encoding object
     * @throws APIException if the transmission fails
     */
    public void send(EID destination, Integer lifetime, SelfEncodingObject obj) throws APIException {
        send(destination, lifetime, obj, Bundle.Priority.NORMAL);
    }

    /**
     * Sends a bundle directly to the daemon. The given object has to encode its own payload into base64 data.
     *
     * @param destination the destination of the bundle
     * @param lifetime the lifetime of the bundle
     * @param obj the self-encoding object
     * @param priority the priority of the bundle
     * @throws APIException if the transmission fails
     */
    public void send(EID destination, Integer lifetime, SelfEncodingObject obj, Bundle.Priority priority) throws APIException {
        // wrapper to the send(Bundle) function
        Bundle bundle = new Bundle(destination, lifetime);
        bundle.appendBlock(new PayloadBlock(new SelfEncodingObjectBlockData(obj)));
        bundle.setPriority(priority);
        send(bundle);
    }

    /**
     * Sends a bundle directly to the daemon. The given stream has to provide already base64 encoded data.
     *
     * @param destination the destination of the bundle
     * @param lifetime the lifetime of the bundle
     * @param stream the stream containing the encoded payload data
     * @param length the length of the un-encoded payload
     * @throws APIException if the transmission fails
     */
    @Deprecated
    public void sendBase64(EID destination, Integer lifetime, InputStream stream, Long length) throws APIException {
        class ImplSelfEncodingObject implements SelfEncodingObject {

            private InputStream stream = null;
            private Long length = 0L;

            public ImplSelfEncodingObject(InputStream stream, Long length) {
                this.stream = stream;
                this.length = length;
            }

            @Override
            public Long getLength() {
                return this.length;
            }

            @Override
            public void encode64(OutputStream output) throws IOException {
                byte[] buf = new byte[65535];
                Integer len = 0;

                while ((len = this.stream.read(buf)) > 0) {
                    output.write(buf, 0, len);
                }
            }
        }

        send(destination, lifetime, new ImplSelfEncodingObject(stream, length));
    }

    /**
     * Sends a bundle directly to the daemon. The given stream is used as payload of the bundle.
     *
     * @param destination the destination of the bundle
     * @param lifetime the lifetime of the bundle
     * @param stream the stream containing the payload data
     * @param length the length of the payload
     * @throws APIException if the transmission fails
     */
    public void send(EID destination, Integer lifetime, InputStream stream, Long length) throws APIException {
        send(destination, lifetime, stream, length, Bundle.Priority.NORMAL);
    }

    /**
     * Send a bundle directly to the daemon. The given stream is used as payload of the bundle.
     *
     * @param destination the destination of the bundle
     * @param lifetime the lifetime of the bundle
     * @param stream the stream containing the payload data
     * @param length the length of the payload
     * @param priority the priority of the bundle
     * @throws APIException if the transmission fails
     */
    public void send(EID destination, Integer lifetime, InputStream stream, Long length, Bundle.Priority priority) throws APIException {
        Bundle bundle = new Bundle(destination, lifetime);
        bundle.appendBlock(new PayloadBlock(new InputStreamBlockData(stream, length)));
        bundle.setPriority(priority);
        send(bundle);
    }

    /**
     * Send a bundle directly to the daemon. The given java object gets serialized and the data used as payload.
     *
     * @param destination the destination of the bundle
     * @param lifetime the lifetime of the bundle
     * @param obj the java object to serialize
     * @throws APIException if the transmission fails
     */
    public void send(EID destination, Integer lifetime, Object obj) throws APIException {
        send(destination, lifetime, obj, Bundle.Priority.NORMAL);
    }

    /**
     * Send a bundle directly to the daemon. The given java object gets serialized and the data used as payload.
     *
     * @param destination the destination of the bundle
     * @param lifetime the lifetime of the bundle
     * @param obj the java object to serialize
     * @param priority the priority of the bundle
     * @throws APIException if the transmission fails
     */
    public void send(EID destination, Integer lifetime, Object obj, Bundle.Priority priority) throws APIException {
        Bundle bundle = new Bundle(destination, lifetime);
        bundle.appendBlock(new PayloadBlock(obj));
        bundle.setPriority(priority);
        send(bundle);
    }

    /**
     * Decodes an object out of the base64 encoded payload data.
     *
     * @param data the base64 encoded payload data
     * @return the decoded object
     * @throws APIException if the decode process fails
     */
    public Object decodeObject(String data) throws IOException, ClassNotFoundException {
        // decode Base64 payload
        byte[] base64_data = null;

        base64_data = Base64.decode(data.getBytes());

        // deserialize the byte array to an object
        ObjectInputStream obj;

        ByteArrayInputStream ba = new ByteArrayInputStream(base64_data);
        obj = new ObjectInputStream(ba);
        return obj.readObject();
    }

    private void startBundle(Integer procflags, EID destination, Integer lifetime, Integer numBlocks) throws APIException {
        // upload bundle to the daemon
        send("Destination: " + destination.toString());
        send("Lifetime: " + String.valueOf(lifetime));
        send("Processing flags: " + String.valueOf(procflags));
        send("Blocks: " + String.valueOf(numBlocks));
        send("");
    }

    private void startBlock(Integer type, Long length, Boolean lastblock) throws APIException {
        send("Block: " + String.valueOf(type));

        if (lastblock) {
            send("Flags: LAST_BLOCK");
        }

        send("Length: " + String.valueOf(length));
        send("");
    }

    /**
     * Sets the application endpoint ID for this client connection to a concatenation of the daemons EID and the given
     * string. This EID will be used as source address of outgoing bundles.
     *
     * @param id the application name that will be concatenated to the daemon's EID
     * @throws APIException if the request fails
     */
    public synchronized void setEndpoint(String id) throws APIException {
        // throw exception if not connected
        if (state != State.CONNECTED) {
            throw new APIException("not connected");
        }

        // send endpoint request
        if (query("set endpoint " + id) != 200) {
            // error
            throw new APIException("set endpoint failed");
        }
    }

    /**
     * Adds a registration to the client connection. The registration will not be used as the source when sending
     * bundles,
     *
     * @see setEndpoint for this purpose
     *
     * @param id the application name that will be concatenated to the daemon's EID
     * @throws APIException if the request fails
     */
    public synchronized void addEndpoint(String id) throws APIException {
        // throw exception if not connected
        if (state != State.CONNECTED) {
            throw new APIException("not connected");
        }

        // send endpoint request
        if (query("endpoint add " + id) != 200) {
            // error
            throw new APIException("set endpoint failed");
        }

        logger.log(Level.INFO, "Endpoint ''{0}'' registered.", id);
    }

    /**
     * Returns the primary endpoint for this client connection in the form 'dtn://$nodename/$id'.
     *
     * @return the primary EID ('dtn://$nodename/$id')
     * @throws APIException if the request fails
     */
    public synchronized EID getEndpoint() throws APIException {
        // throw exception if not connected
        if (state != State.CONNECTED) {
            throw new APIException("not connected");
        }

        // send endpoint request
        Response resp = queryWithResponse("endpoint get");
        if (resp.getCode() != 200) {
            throw new APIException("endpoint get failed");
        }

        String[] responseWords = resp.getData().split(" ");
        if (responseWords.length < 3) {
            throw new APIException("endpoint get returned not enough data");
        }

        return new SingletonEndpoint(responseWords[2]);
    }

    /**
     * Removes a registration from the client connection.
     *
     * @param id the application name that will be concatenated to the daemons EID
     * @throws APIException if the request fails
     */
    public synchronized void removeEndpoint(String id) throws APIException {
        // throw exception if not connected
        if (state != State.CONNECTED) {
            throw new APIException("not connected");
        }

        // send endpoint request
        if (query("endpoint del " + id) != 200) {
            // error
            throw new APIException("set endpoint failed");
        }

        logger.log(Level.INFO, "Endpoint ''{0}'' removed.", id);
    }

    /**
     * Registers a group endpoint in the form 'dtn://$name[/$id]'.
     *
     * This registration therefore is independent from the node name and can be used for group communications.
     *
     * @param eid the EID to subscribe to
     * @throws APIException if the request fails
     */
    public synchronized void addRegistration(GroupEndpoint eid) throws APIException {
        // throw exception if not connected
        if (state != State.CONNECTED) {
            throw new APIException("not connected");
        }

        // send endpoint request
        if (query("registration add " + eid.toString()) != 200) {
            // error
            throw new APIException("registration add failed");
        }

        logger.log(Level.INFO, "GroupEndpoint ''{0}'' registered.", eid);
    }

    /**
     * Removes a group endpoint registration in the form 'dtn://$name[/$id]' from this client connection.
     *
     * @param eid the EID to unsubscribe from
     * @throws APIException if the request fails
     */
    public synchronized void removeRegistration(GroupEndpoint eid) throws APIException {
        // throw exception if not connected
        if (state != State.CONNECTED) {
            throw new APIException("not connected");
        }

        // send endpoint request
        send("registration del " + eid.toString());

        // read answer
        if (_receiver.getResponse().getCode() != 200) {
            // error
            throw new APIException("registration removal failed");
        }

        logger.log(Level.INFO, "GroupEndpoint ''{0}'' removed.", eid);
    }

    /**
     * Returns all registrations of this connection, including group endpoints.
     *
     * @return a list of all the application's registered EIDs
     * @throws APIException if the request fails
     */
    public synchronized List<String> getRegistrations() throws APIException {
        // throw exception if not connected
        if (state != State.CONNECTED) {
            throw new APIException("not connected");
        }

        // query for registration list
        if (query("registration list") != 200) {
            throw new APIException("registration get failed");
        }

        return _receiver.getList();
    }

    /**
     * Gets all neighbors of the daemon.
     *
     * @return A list of EIDs. Each of them is an available neighbor.
     * @throws APIException if the request fails
     */
    public synchronized List<String> getNeighbors() throws APIException {
        // throw exception if not connected
        if (state != State.CONNECTED) {
            throw new APIException("not connected");
        }

        // query for registration list
        if (query("neighbor list") != 200) {
            throw new APIException("neighbor list failed");
        }

        return _receiver.getList();
    }

    /**
     * Gets all neighbor connections of the daemon.
     *
     * NOTE: Only works with Rotti's <rottmann@ibr.cs.tu-bs.de> adapted daemon.
     *
     * @return A list of nodes with connection information.
     * @throws APIException if the request fails
     */
    public synchronized List<Node> getNeighborConnections() throws APIException {
        // throw exception if not connected
        if (state != State.CONNECTED) {
            throw new APIException("not connected");
        }

        // query for registration list connections
        if (query("neighbor list connections") != 200) {
            throw new APIException("neighbor list connections failed");
        }

        List<Node> nodes = new LinkedList<>();
        for (String s : _receiver.getList()) {
            Node node = new Node(s);
            nodes.add(node);
        }

        return nodes;
    }

    /**
     * Gets the EID of the DTN daemon in the form 'dtn://$name'.
     *
     * @return the daemons EID
     * @throws APIException if the request fails
     */
    public synchronized EID getNodeName() throws APIException {
        if (remoteEID != null) {
            return remoteEID;
        }

        // throw exception if not connected
        if (state != State.CONNECTED) {
            throw new APIException("not connected");
        }

        Response resp = queryWithResponse("nodename");
        // query for remote's EID
        if (resp.getCode() != 200) {
            throw new APIException("nodename failed");
        }

        String[] responseWords = resp.getData().split(" ");
        if (responseWords.length < 2) {
            throw new APIException("nodename returned not enough data");
        }

        remoteEID = new SingletonEndpoint(responseWords[1]);

        return remoteEID;
    }

    /**
     * Loads a bundle into the remote register.
     *
     * @param id the bundle ID of the bundle to load
     * @throws APIException if the request fails
     */
    public synchronized void loadBundle(BundleID id) throws APIException {
        // throw exception if not connected
        if (state != State.CONNECTED) {
            throw new APIException("not connected");
        }

        send("bundle load " + id.toString());

        // read answer
        if (_receiver.getResponse().getCode() != 200) {
            // error
            throw new APIException("bundle load failed");
        }
    }

    /**
     * Loads the next bundle in the queue into the remote register.
     *
     * @throws APIException if the request fails
     */
    public synchronized void loadBundle() throws APIException {
        // throw exception if not connected
        if (state != State.CONNECTED) {
            throw new APIException("not connected");
        }

        // load bundle
        if (query("bundle load queue") != 200) {
            // error
            throw new APIException("bundle load failed");
        }
    }

    /**
     * Sends a summary (without payload) of the bundle in the remote register to the client (in plain text format).
     *
     * @throws APIException if the request fails
     */
    public synchronized void getBundleInfo() throws APIException {
        // throw exception if not connected
        if (state != State.CONNECTED) {
            throw new APIException("not connected");
        }

        // query bundle
        if (query("bundle info") != 200) {
            // error
            throw new APIException("bundle info failed");
        }
    }

    /**
     * Loads the next bundle in the queue into the remote register and starts the API transfer of the bundle.
     *
     * @throws APIException if the request fails
     */
    public synchronized void loadAndGetBundle() throws APIException {
        // throw exception if not connected
        if (state != State.CONNECTED) {
            throw new APIException("not connected");
        }

        // load bundle
        int ret = query("bundle load queue");

        if (ret == 200) {
            // query bundle
            if (query("bundle get") != 200) {
                // error
                throw new APIException("bundle get failed");
            }
        } else {
            if (ret == 400) {
                throw new APIException("no bundle available");
            } else {
                // error
                throw new APIException("bundle load failed");
            }
        }
    }

    /**
     * Starts the API transfer of the bundle in the remote register to the client (in plain text format).
     *
     * @throws APIException if the request fails
     */
    public synchronized void getBundle() throws APIException {
        // throw exception if not connected
        if (state != State.CONNECTED) {
            throw new APIException("not connected");
        }

        // query bundle
        if (query("bundle get") != 200) {
            // error
            throw new APIException("bundle get failed");
        }
    }

    /**
     * Starts the API transfer of the payload in the remote register to the client (in plain text format).
     *
     * @throws APIException if the request fails
     */
    public void getPayload() throws APIException {
        getPayload(0, 0, 0);
    }

    /**
     * Starts the API transfer of the bundle in the remote register to the client (in plain text format).
     *
     * @throws APIException if the request fails
     */
    public synchronized void getPayload(int blockOffset, int dataOffset, int length) throws APIException {
        // throw exception if not connected
        if (state != State.CONNECTED) {
            throw new APIException("not connected");
        }

        StringBuilder sb = new StringBuilder("payload ");
        sb.append(blockOffset).append(" ");
        sb.append("get");
        if (dataOffset != 0) {
            sb.append(" ").append(dataOffset);
        }
        if (length != 0) {
            sb.append(" ").append(length);
        }

        // query payload
        if (query(sb.toString()) != 200) {
            // error
            throw new APIException("payload get failed");
        }
    }

    /**
     * Deletes the bundle in the remote register. Also deletes the bundle in the daemon storage.
     *
     * @throws APIException if the request fails
     */
    public synchronized void freeBundle() throws APIException {
        // throw exception if not connected
        if (state != State.CONNECTED) {
            throw new APIException("not connected");
        }

        // query bundle
        if (query("bundle free") != 200) {
            // error
            throw new APIException("bundle free failed");
        }
    }

    /**
     * Marks the given bundle as delivered. This may delete the bundle in the storage of the daemon.
     *
     * @throws APIException if the request fails
     */
    public synchronized void markDelivered(BundleID id) throws APIException {
        // throw exception if not connected
        if (state != State.CONNECTED) {
            throw new APIException("not connected");
        }

        // query bundle
        if (query("bundle delivered " + id.toString()) != 200) {
            // error
            throw new APIException("bundle delivered failed");
        }
    }

    /**
     * Sends a command to the daemon.
     *
     * @param cmd the command to send
     * @throws IOException
     */
    private synchronized void send(String cmd) throws APIException {
        try {
            debug("[Send] " + cmd);
            _writer.write(cmd);
            _writer.newLine();
            _writer.flush();
        } catch (IOException e) {
            throw new APIException("send failed: " + cmd);
        }
    }

    /**
     * Sends a command to the daemon.
     *
     * @param cmd the command to send
     * @throws IOException
     */
    private synchronized Integer query(String cmd) throws APIException {
        return queryWithResponse(cmd).getCode();
    }

    private synchronized Response queryWithResponse(String cmd) throws APIException {
        try {
            debug("[Query] " + cmd);
            _writer.write(cmd);
            _writer.newLine();
            _writer.flush();

            return _receiver.getResponse();
        } catch (IOException e) {
            throw new APIException("query failed: " + cmd);
        }
    }
}
