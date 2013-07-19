/*
 * Block.java
 * 
 * Copyright (C) 2011 IBR, TU Braunschweig
 *
 * Written-by: 
 *  Johannes Morgenroth <morgenroth@ibr.cs.tu-bs.de>
 *  Julian Timpner      <timpner@ibr.cs.tu-bs.de>
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

public abstract class Block extends BlockHeader {

    public static abstract class Data {

        /**
         * Writes the data encapsulated by this class to an OutputStream
         *
         * @param stream the stream to write to
         * @throws IOException if reading the data or writing to the stream failed
         */
        abstract public void writeTo(OutputStream stream) throws IOException;

        /**
         * @return The size of the data that will be written by writeTo()
         */
        abstract public long size();
    }

    /**
     * Creates a new block from a byte array.
     *
     * @param type The block type
     * @param flags The flags of the block
     * @param data the data
     * @return the created block
     * @throws InvalidDataException if there was not enough data to create a block of the given type
     */
    public static Block createBlock(int type, long flags, byte[] data) throws InvalidDataException {
        return createBlock(type, flags, new ByteArrayBlockData(data));
    }

    /**
     * Creates a new Block from a Block.Data object.
     *
     * @param type The block type
     * @param flags The flags of the block
     * @param data the data
     * @return the created block
     * @throws InvalidDataException if there was not enough data to create a block of the given type
     */
    public static Block createBlock(int type, long flags, Data data) throws InvalidDataException {
        Block block;

        switch (type) {
            case PayloadBlock.type:
                block = new PayloadBlock(data);
                break;
            case ScopeControlHopLimitBlock.type:
                block = new ScopeControlHopLimitBlock(data);
                break;
            default:
                block = new ExtensionBlock(type, data);
                break;
        }

        block.setFlags(flags);
        return block;
    }

    /**
     * Creates a new Block with no data.
     *
     * @param type the block type
     * @return the created block
     */
    public static Block createBlock(int type) {
        Block block;

        switch (type) {
            case PayloadBlock.type:
                block = new PayloadBlock();
                break;
            case ScopeControlHopLimitBlock.type:
                block = new ScopeControlHopLimitBlock();
                break;
            default:
                block = new ExtensionBlock(type);
                break;
        }

        return block;
    }

    /**
     * Creates a new Block from a Block.Data object and a block header.
     *
     * @param header the block header
     * @param data the data
     * @return the created block
     * @throws InvalidDataException if there was not enough data to create a block of the given type
     */
    public static Block createBlock(BlockHeader header, Data data) throws InvalidDataException {
        Block block = createBlock(header.getType(), header.procflags, data);
        block.setEIDs(header._eids);
        return block;
    }

    protected Block(int type) {
        super(type);
    }

    public abstract Data getData();

    public class InvalidDataException extends Exception {

        /**
         * generated serialVersionUID
         */
        private static final long serialVersionUID = 2111512395823250878L;

        public InvalidDataException(String msg) {
            super(msg);
        }
    }
}
