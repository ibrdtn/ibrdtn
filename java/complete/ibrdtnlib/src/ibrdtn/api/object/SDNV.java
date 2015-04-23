/*
 * SDNV.java
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

public class SDNV implements Comparable<SDNV> {

	//SIZE-1 since only positive numbers are allowed, rounded up to a multiple of 7
	//divided by 7 to get the number of 7bit blocks;
	public static final int MAX_SDNV_BYTES = ((Long.SIZE-1+((-Long.SIZE)%7))/7);

	private long _value;
	public final int length;

	public SDNV(long value) throws NumberFormatException {
		if(_value < 0) throw new NumberFormatException("SDNVs cannot be negative.");
		_value = value;
		length = calculateLength();
	}

	public SDNV(byte[] data) throws NumberFormatException {
		//check if the given data fits into a long
		if(data.length > MAX_SDNV_BYTES || data.length == 0
				|| Integer.highestOneBit(data[0] & (byte) 0x7f) > 7-((-Long.SIZE)%7))
			throw new NumberFormatException("SDNV length not supported.");
		length = data.length;
		int i = 0;
		for(byte b : data) {
			_value <<= 7;
            _value |= b & (byte) 0x7F;
			if((b & (byte) 0x80) == 0)
				break;
			++i;
		}
		if((data[i] & (byte) 0x80) != 0)
			throw new NumberFormatException("Malformed SDNV.");
	}

	public long getValue() {
		return _value;
	}

	public byte[] getBytes() {
		byte[] ret = new byte[length];

		//this blocks loops _value in 7bit blocks and creates the SDNV bytes
		for(int i = 1; i < length; ++i) {
			//set the most significant bit for all 7bit blocks except the last
			ret[length-(i+1)] = (byte) ((byte) (_value >> i) | (byte) 0x80);
		}
		//the last byte has to have its MSB cleared
		//(this is the i=0 case)
		ret[length-1] = (byte) ((byte) _value & (byte) 0x7f);

		return ret;
	}

	private int calculateLength() {
		//this blocks loops _value in 7bit blocks and looks
		//for the first black that is nonzero
		for(int i = Long.SIZE / 7; i >= 0; --i) {
			byte b = (byte) ((byte) (_value >> i) & (byte) 0x7f);
			if(b != 0)
				return i+1;
		}
		//the number is 0, but we will still need 1 byte to encode it
		return 1;
	}

	@Override
	public int compareTo(SDNV o) {
		return new Long(_value).compareTo(o._value);
	}
}
