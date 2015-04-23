/*
 * ManageClient.java
 * 
 * Copyright (C) 2011 IBR, TU Braunschweig
 *
 * Written-by: Johannes Morgenroth <morgenroth@ibr.cs.tu-bs.de>
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

import ibrdtn.api.object.SingletonEndpoint;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.net.UnknownHostException;

public class P2PExtensionClient extends Client {
	
	private Object _connection_mutex = new Object();
	private APIReader _reader = null;
	private BufferedWriter _writer = null;
	
	private ExtensionType _type = ExtensionType.WIFI;
	
	private Object _listener_mutex = new Object();
	protected P2PEventListener _listener = null;
	
	public enum ExtensionType {
		WIFI("wifi")
	    ;
	    /**
	     * @param text
	     */
	    private ExtensionType(final String text) {
	        this.text = text;
	    }

	    private final String text;

	    /* (non-Javadoc)
	     * @see java.lang.Enum#toString()
	     */
	    @Override
	    public String toString() {
	        return text;
	    }
	}
	
	public enum ConnectionType {
		TCP("tcp"),
		UDP("udp")
	    ;
	    /**
	     * @param text
	     */
	    private ConnectionType(final String text) {
	        this.text = text;
	    }

	    private final String text;

	    /* (non-Javadoc)
	     * @see java.lang.Enum#toString()
	     */
	    @Override
	    public String toString() {
	        return text;
	    }
	}

	/**
	 * This exception is thrown if something went wrong during an API call.
	 */
	public class P2PException extends Exception
	{
		private static final long serialVersionUID = -5548771170909752154L;

		public P2PException(String what)
		{
			super(what);
		}
	}
	
	public P2PExtensionClient(ExtensionType type)
	{
		super();
		_type = type;
	}
	
	/**
	 * Set the handler for incoming API events.
	 * @param handler A handler object derived from SABHandler.
	 */
	public void setP2PEventListener(P2PEventListener listener) {
		synchronized(_listener_mutex) {
			this._listener = listener;
		}
	}
	
	@Override
	public void open() throws UnknownHostException, IOException
	{
		super.open();
		
		// run the notify receiver
		_reader = new APIReader(this);
		_reader.open(istream);
		
		_writer = new BufferedWriter( new OutputStreamWriter(ostream) );
		
		// switch to event protocol
		_writer.write("protocol p2p_extension " + _type.toString());
		
		_writer.newLine();
		_writer.flush();
	}
	
	@Override
	public void close() throws IOException {
		_writer.close();
		_reader.close();
		
		super.close();
	}
	
	public void fireConnected(SingletonEndpoint eid, ConnectionType c, String data) throws IOException
	{
		synchronized(_connection_mutex) {
			_writer.write("connected " + eid.toString() + " " + c.toString() + " " + data);
			_writer.newLine();
			_writer.flush();
		}
	}
	
	public void fireDisconnected(SingletonEndpoint eid, ConnectionType c, String data) throws IOException
	{
		synchronized(_connection_mutex) {
			_writer.write("disconnected " + eid.toString() + " " + c.toString() + " " + data);
			_writer.newLine();
			_writer.flush();
		}
	}
	
	public void fireDiscovered(SingletonEndpoint eid, String data) throws IOException
	{
		synchronized(_connection_mutex) {
			_writer.write("discovered " + eid.toString() + " " + data);
			_writer.newLine();
			_writer.flush();
		}
	}
	
	public void fireInterfaceUp(String iface) throws IOException
	{
		synchronized(_connection_mutex) {
			_writer.write("interface up " + iface);
			_writer.newLine();
			_writer.flush();
		}
	}
	
	public void fireInterfaceDown(String iface) throws IOException
	{
		synchronized(_connection_mutex) {
			_writer.write("interface down " + iface);
			_writer.newLine();
			_writer.flush();
		}
	}
	
	private class APIReader {
		private P2PExtensionClient _client = null;
		private BufferedReader _reader = null;
		
		public APIReader(P2PExtensionClient client)
		{
			_client = client;
		}
		
		public void open(InputStream is)
		{
			_reader = new BufferedReader(new InputStreamReader(is));
			_process.start();
		}
		
		private void fireConnect(String data) {
			synchronized(_client._listener_mutex) {
				if (_client._listener != null)
					_client._listener.connect(data);
			}
		}
		
		private void fireDisconnect(String data) {
			synchronized(_client._listener_mutex) {
				if (_client._listener != null)
					_client._listener.disconnect(data);
			}			
		}
		
		private Thread _process = new Thread() {
			@Override
			public void run() {
				
				try {
					while (true) {
						String data = _reader.readLine();
						
						// if the data is null throw exception
						if (data == null) throw new P2PException("end of stream reached");
						
						// read the payload, skip empty lines
						if (data.length() == 0)
						{
							continue;
						}
						
						// split the line into status code and parameter
						String[] values = data.split(" ", 2);
						
						// check the count of values
						if (values.length == 0)
						{
							// error
							throw new P2PException("invalid data received");
						}
						
						Integer code = Integer.valueOf(values[0]);
						
						switch (code)
						{
						// CONNECT
						case 101:
							fireConnect(values[1].substring(8));
							break;
							
						// DISCONNECT
						case 102:
							fireDisconnect(values[1].substring(11));
							break;
						
						default:
							break;
						}
					}
				} catch (IOException e) {
				} catch (P2PException e) {
				}
			}
		};
		
		public void close()
		{
			try {
				_reader.close();
				_process.join();
			} catch (IOException e) {
				e.printStackTrace();
			} catch (InterruptedException e) {
				e.printStackTrace();
			}
		}
	}
}
