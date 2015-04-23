/*
 * SABHandler.java
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
/**
 * Simple API for Bundle Protocol (SAB)
 * This is a handler for Simple API events.
 * 
 * @author Johannes Morgenroth <morgenroth@ibr.cs.tu-bs.de>
 */

package ibrdtn.api.sab;

import java.util.List;

public interface SABHandler {
	public void startStream();
	public void endStream();
	
	public void startBundle();
	public void endBundle();
	
	public void startBlock(Integer type);
	public void endBlock();
	
	public void attribute(String keyword, String value);
	public void characters(String data) throws SABException;
	public void notify(Integer type, String data);
	
	public void response(Integer type, String data);
	public void response(List<String> data);
}
