/*
 * Client.java
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

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.UnknownHostException;

public class Client {

	private APIConnection _socket = null;
	protected InputStream istream = null;
	protected OutputStream ostream = null;

	protected String _header = null;
	private String host = "localhost";
	private Integer port = 4550;
	
	public Client()
	{
	}
	
	public void setConnection(APIConnection sock)
	{
		this._socket = sock;
	}
	
	public void setHost(String host)
	{
		this.host = host;
	}
	
	public void setPort(Integer port)
	{
		this.port = port;
	}
	
	public synchronized Boolean isConnected()
	{
		if (_socket == null) return false;
		return _socket.isConnected();
	}
	
	public synchronized Boolean isClosed()
	{
		if (_socket == null) return true;
		return !_socket.isConnected();
	}

	public synchronized void open() throws UnknownHostException, IOException
	{
		if (_socket == null)
		{
			_socket = new SocketAPIConnection(this.host, this.port);
		}
		
		_socket.open();
		
		ostream = _socket.getOutputStream();
		istream = _socket.getInputStream();
		
		BufferedReader reader = new BufferedReader( new InputStreamReader( istream ) );	
		
		// wait for header
		_header = reader.readLine();
	}
	
	public synchronized void close() throws IOException
	{
		if (ostream != null) ostream.close();
		if (istream != null) istream.close();
		if (_socket != null) _socket.close();
	}
}
