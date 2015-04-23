/*
 * EventClient.java
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
import java.io.BufferedWriter;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.net.UnknownHostException;
import java.util.HashMap;
import java.util.Map;

public class EventClient extends Client {
	
	BufferedReader _reader = null;
	BufferedWriter _writer = null;
	
	private EventListener _event_listener = null;
	
	public EventClient(EventListener listener)
	{
		super();
		_event_listener = listener;
	}
	
	@Override
	public void open() throws UnknownHostException, IOException
	{
		super.open();
		_reader = new BufferedReader( new InputStreamReader(istream) );
		_writer = new BufferedWriter( new OutputStreamWriter(ostream) );
		
		// switch to event protocol
		_writer.write("protocol event");
		_writer.newLine();
		_writer.flush();
		
		// read confirmation
		_reader.readLine();
		
		// start the receiver thread
		_receiver.start();
	}

	private Thread _receiver = new Thread() {
		@Override
		public void run() {
			String line = null;
			String name = null;
			String action = null;
			Map<String, String> attrs = new HashMap<String, String>();
			
			try {
				while ((line = _reader.readLine()) != null)
				{
					// if the line is empty
					if (line.length() == 0)
					{
						// create the event object
						Event evt = new Event(name, action, attrs);
						_event_listener.eventRaised(evt);
						
						// clear all variables
						name = null;
						action = null;
						attrs = new HashMap<String, String>();
						
						// continue with the next line
						continue;
					}
					
					// search for the delimiter
					int delimiter = line.indexOf(':');
					
					// if the is no delimiter, ignore the line
					if (delimiter == -1) continue;
					
					// split the keyword and data pair
					String keyword = line.substring(0, delimiter);
					String data = line.substring(delimiter + 1, line.length()).trim();
					
					if (keyword.equalsIgnoreCase("EVENT"))
					{
						name = data;
					}
					else if (keyword.equalsIgnoreCase("ACTION"))
					{
						action = data;
					}
					else
					{
						attrs.put(keyword, data);
					}
				}
			} catch (IOException e) {
				e.printStackTrace();
			}
		}
	};
}
