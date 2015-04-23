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
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.net.UnknownHostException;
import java.util.LinkedList;
import java.util.List;

public class ManageClient extends Client {
	
	BufferedReader _reader = null;
	BufferedWriter _writer = null;
	
	public ManageClient()
	{
		super();
	}
	
	@Override
	public void open() throws UnknownHostException, IOException
	{
		super.open();
		_reader = new BufferedReader( new InputStreamReader(istream) );
		_writer = new BufferedWriter( new OutputStreamWriter(ostream) );
		
		// switch to event protocol
		_writer.write("protocol management");
		_writer.newLine();
		_writer.flush();
		
		// read confirmation
		_reader.readLine();
	}
	
	@Override
	public void close() throws IOException {
		_writer.close();
		_reader.close();
		
		super.close();
	}
	
	public List<String> getLog()
	{
		LinkedList<String> ret = new LinkedList<String>();
		
		try {
			_writer.write("logcat");
			_writer.newLine();
			_writer.flush();
			
			String line = null;
			while ((line = _reader.readLine()) != null)
			{
				if (line.length() == 0) break;
				ret.add(line);
			}
		} catch (IOException e) { }
		
		return ret;
	}
	
	public void addLink(String iface, String address)
	{
		try {
			_writer.write("interface address add " + iface + " ipv4 " + address);
			_writer.newLine();
			_writer.flush();

			// read response
			String line = _reader.readLine();

			// throw exception if there is no response
			if (line == null) throw new IOException("no return code received");
			
			if (!line.startsWith("200 ")) throw new IOException("wrong return code received");
		} catch (IOException e) { }
	}
	
	public void removeLink(String iface, String address)
	{
		try {
			_writer.write("interface address del " + iface + " ipv4 " + address);
			_writer.newLine();
			_writer.flush();

			// read response
			String line = _reader.readLine();

			// throw exception if there is no response
			if (line == null) throw new IOException("no return code received");
			
			if (!line.startsWith("200 ")) throw new IOException("wrong return code received");
		} catch (IOException e) { }
	}
	
	public void addConnection(SingletonEndpoint eid, String protocol, String address, String port) {
		try {
			_writer.write("connection " + eid.toString() + " " + protocol + " add " + address + " " + port);
			_writer.newLine();
			_writer.flush();

			// read response
			String line = _reader.readLine();

			// throw exception if there is no response
			if (line == null) throw new IOException("no return code received");
			
			if (!line.startsWith("200 ")) throw new IOException("wrong return code received");
		} catch (IOException e) { }
	}
	
	public void removeConnection(SingletonEndpoint eid, String protocol, String address, String port) {
		try {
			_writer.write("connection " + eid.toString() + " " + protocol + " del " + address + " " + port);
			_writer.newLine();
			_writer.flush();

			// read response
			String line = _reader.readLine();

			// throw exception if there is no response
			if (line == null) throw new IOException("no return code received");
			
			if (!line.startsWith("200 ")) throw new IOException("wrong return code received");
		} catch (IOException e) { }
	}
	
	public LinkedList<String> getNeighbors()
	{
		LinkedList<String> ret = new LinkedList<String>();
		
		try {
			_writer.write("neighbor list");
			_writer.newLine();
			_writer.flush();
			
			// read response
			_reader.readLine();
			
			String line = null;
			while ((line = _reader.readLine()) != null)
			{
				if (line.length() == 0) break;
				ret.add(line);
			}
		} catch (IOException e) { }
		
		return ret;
	}
	
	public void resume()
	{
		if (_writer == null) return;
		try {
			_writer.write("core wakeup");
			_writer.newLine();
			_writer.flush();
			
			// read response
			String line = _reader.readLine();

			// throw exception if there is no response
			if (line == null) throw new IOException("no return code received");
			
			if (!line.startsWith("200 ")) throw new IOException("wrong return code received");
		} catch (IOException e) { }
	}
	
	public void suspend()
	{
		if (_writer == null) return;
		try {
			_writer.write("core suspend");
			_writer.newLine();
			_writer.flush();
			
			// read response
			String line = _reader.readLine();

			// throw exception if there is no response
			if (line == null) throw new IOException("no return code received");
			
			if (!line.startsWith("200 ")) throw new IOException("wrong return code received");
		} catch (IOException e) { }
	}
	
	public void shutdown()
	{
		if (_writer == null) return;
		try {
			_writer.write("core shutdown");
			_writer.newLine();
			_writer.flush();
			
			// read response
			String line = _reader.readLine();

			// throw exception if there is no response
			if (line == null) throw new IOException("no return code received");
			
			if (!line.startsWith("200 ")) throw new IOException("wrong return code received");
		} catch (IOException e) { }
	}

}
