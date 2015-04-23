/*
 * DefaultSABHandler.java
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
package ibrdtn.api.sab;

import java.util.List;

public class DefaultSABHandler implements SABHandler {

	@Override
	public void startStream() {
	}

	@Override
	public void endStream() {
	}

	@Override
	public void startBundle() {
	}

	@Override
	public void endBundle() {
	}

	@Override
	public void startBlock(Integer type) {
	}

	@Override
	public void endBlock() {
	}

	@Override
	public void attribute(String keyword, String value) {
	}

	@Override
	public void characters(String data) throws SABException {
	}

	@Override
	public void notify(Integer type, String data) {
	}

	@Override
	public void response(Integer type, String data) {
	}

	@Override
	public void response(List<String> data) {
	}

}
