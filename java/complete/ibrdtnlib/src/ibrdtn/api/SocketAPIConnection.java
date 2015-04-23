/*
 * SocketAPIConnection.java
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

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class SocketAPIConnection implements APIConnection  {
	
	private java.net.Socket _socket = null;
	private String _host = "localhost";
	private int _port = 4550;
	
	public SocketAPIConnection()
	{
	}
	
	public SocketAPIConnection(String host, int port)
	{
		this._host = host;
		this._port = port;
	}
	
	@Override
	public void open() throws IOException
	{
		if (_socket != null)
		{
			_socket.close();
		}
		_socket = new java.net.Socket(this._host, this._port);
                // Disable Nagle's algorithm, preventing delays in sending bundles to the daemon
                _socket.setTcpNoDelay(true);
	}

	@Override
	public Boolean isConnected() {
		if (_socket == null) return false;
		return _socket.isConnected();
	}

	@Override
	public Boolean isClosed() {
		if (_socket == null) return true;
		return _socket.isClosed();
	}

	@Override
	public OutputStream getOutputStream() throws IOException {
		return _socket.getOutputStream();
	}

	@Override
	public InputStream getInputStream() throws IOException {
		return _socket.getInputStream();
	}

	@Override
	public void close() throws IOException {
		if (_socket != null) _socket.close();
	}

}
