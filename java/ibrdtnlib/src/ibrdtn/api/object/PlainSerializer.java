/*
 * PlainSerializer.java
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

import ibrdtn.api.Base64;

import java.io.IOException;
import java.io.OutputStream;
import java.util.Set;

public class PlainSerializer {
	private OutputStream _stream;
	
	public PlainSerializer(OutputStream stream)
	{
		_stream = stream;
	}
	
	/**
	 * Serialize a bundle into the OutputStream given to the constructor
	 * @param bundle the bundle to serialize
	 * @throws IOException thrown if writing to the OutputStream failed.
	 */
	public void serialize(Bundle bundle) throws IOException
	{
		serializePrimaryBlock(bundle);
		for(Block block : bundle.blocks){
			serializeBlock(block);
		}
		_stream.flush();
	}
	
	private void serializePrimaryBlock(Bundle bundle) throws IOException
	{
		writeString("Processing flags: " + bundle.procFlags + "\n");
		if(bundle.timestamp != null)
			writeString("Timestamp: " + bundle.timestamp + "\n");
		if(bundle.sequenceNumber != null)
			writeString("Sequencenumber: " + bundle.sequenceNumber + "\n");
		if(bundle.source != null)
			writeString("Source: " + bundle.source + "\n");
		if(bundle.destination != null)
			writeString("Destination: " + bundle.destination + "\n");
		if(bundle.reportto != null)
			writeString("Reportto: " + bundle.reportto + "\n");
		if(bundle.custodian != null)
			writeString("Custodian: " + bundle.custodian + "\n");
		writeString("Lifetime: " + bundle.lifetime + "\n");
		writeString("Blocks: " + bundle.blocks.size() + "\n");
		writeString("\n");
	}
	
	private void serializeBlock(Block block) throws IOException
	{
		Block.Data data = block.getData();
		Set<EID> eids = block.getEIDS();

		// write header
		writeString("Block: " + block.getType() + "\n");
		writeString("Flags: " + flagString(block) + "\n");
		if(!eids.isEmpty())
			for(EID eid : eids)
				writeString("EID: " + eid + "\n");
		writeString("Length: " + data.size() + "\n");
		writeString("\n");
		
		if(data instanceof SelfEncodingObjectBlockData) {
			data.writeTo(_stream);
		} else {
			// write data base64 encoded to outputStream
			Base64.OutputStream base64out = new Base64.OutputStream(_stream, Base64.ENCODE | Base64.DO_BREAK_LINES);
			data.writeTo(base64out);
			base64out.flushBase64();
			base64out.flush();
		}

		_stream.write("\n\n".getBytes());
	}
	
	private void writeString(String string) throws IOException {
		_stream.write(string.getBytes());		
	}

	private String flagString(Block block){
		String ret = "";
		for(Block.FlagOffset flag : Block.FlagOffset.values()){
			if(block.getFlag(flag))
				ret += " " + flag;
		}
		return ret;
	}
}
