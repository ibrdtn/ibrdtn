/*
 * InputStreamBlockData.java
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
import java.io.InputStream;
import java.io.OutputStream;

/**
 * This class wraps an InputStream to be used as data for blocks
 */
public class InputStreamBlockData extends Block.Data {
	
	private InputStream _stream;
	private long _length;
	private boolean _resetRequired = false;
	
	/**
	 * Constructor
	 * @param stream the stream to read data from
	 * @param length The length of the data that can be read from the InputStream
	 */
	public InputStreamBlockData(InputStream stream, long length) {
		_stream = stream;
		_length = length;
	}

	@Override
	public void writeTo(OutputStream out) throws IOException {
		if(_resetRequired)
			throw new IOException("The data can not be read twice.");

		byte[] buf = new byte[8192];
		int len;
		int total = 0;
		
		while ((len = _stream.read(buf, 0,
				Math.min(buf.length, (int) Math.min(Integer.MAX_VALUE, _length-total)))
				) > 0) {
			total += len;
		    out.write(buf, 0, len);
		}
		
		_resetRequired = true;
	}

	@Override
	public long size() {
		return _length;
	}

}
