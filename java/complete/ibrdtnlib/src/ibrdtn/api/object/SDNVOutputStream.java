/*
 * SDNVOutputStream.java
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
package ibrdtn.api.object;

import java.io.IOException;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.util.Queue;
import java.util.concurrent.LinkedBlockingQueue;

class SDNVOutputStream extends OutputStream {

	private Queue<SDNV> _queue = new LinkedBlockingQueue<SDNV>();
	private ByteBuffer _currentSDNV = ByteBuffer.allocate(SDNV.MAX_SDNV_BYTES);
	private int _currentIndex = 0;

	private boolean _failed = false;

	public SDNV nextSDNV() throws IOException {
		if(_queue.isEmpty())
			throw new IOException("No SDNV in queue");
		return _queue.remove();
	}

	@Override
	public void write(int b) throws IOException {
		if(_failed)
			throw new IOException("SDNVOutputStream in failed state");
		_currentSDNV.put((byte)b);
		++_currentIndex;

		if(((byte) b & (byte) 0x80) == 0) {
			//last byte, create SDNV
			try {
				_queue.add(new SDNV(_currentSDNV.array()));
				_currentSDNV.clear();
				_currentIndex = 0;
			} catch (NumberFormatException ex) {
				_failed = true;
				throw new IOException(ex.getMessage());
			}
		}
		else if(_currentIndex >= _currentSDNV.capacity()) {
			//SDNV too long, set failed state and throw exception
			_failed = true;
			throw new IOException("SDNVs only supported up to " + _currentSDNV.capacity() + "Bytes.");
		}
	}
}
