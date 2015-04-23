/*
 * ScopeControlHopLimitBlock.java
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

public class ScopeControlHopLimitBlock extends Block {

    //public static final int type = 9; TODO 9 is already used by the BSP, check new draft versions
    public static final int type = 199;
    private SDNV _hopCount;
    private SDNV _hopLimit;

    public ScopeControlHopLimitBlock() {
        super(type);
    }

    public ScopeControlHopLimitBlock(SDNV hopCount, SDNV hopLimit) {
        super(type);
        _hopCount = hopCount;
        _hopLimit = hopLimit;
    }

    public ScopeControlHopLimitBlock(SDNV hopLimit) {
        this(new SDNV(0l), hopLimit);
    }

    public ScopeControlHopLimitBlock(long hopCount, long hopLimit) {
        this(new SDNV(hopCount), new SDNV(hopLimit));
    }

    public ScopeControlHopLimitBlock(long hopLimit) {
        this(new SDNV(0l), new SDNV(hopLimit));
    }

    public ScopeControlHopLimitBlock(Block.Data data) throws InvalidDataException {
        super(type);
        SDNVOutputStream _stream = new SDNVOutputStream();
        try {
            data.writeTo(_stream);
            _stream.flush();
            _hopCount = _stream.nextSDNV();
            _hopLimit = _stream.nextSDNV();
        } catch (IOException e) {
            throw new InvalidDataException(e.getMessage());
        }
    }

    public Data getData() {
        byte[] data = new byte[_hopCount.length + _hopLimit.length];
        byte[] _hopCountBytes = _hopCount.getBytes();
        byte[] _hopLimitBytes = _hopLimit.getBytes();
        System.arraycopy(_hopCountBytes, 0, data, 0, _hopCountBytes.length);
        System.arraycopy(_hopLimitBytes, 0, data, _hopCountBytes.length, _hopLimitBytes.length);
        return new ByteArrayBlockData(data);
    }

    public SDNV getHopCount() {
        return _hopCount;
    }

    public SDNV getHopLimit() {
        return _hopLimit;
    }

    public String toString() {
        return "ScopeControlHopLimitBlock: length=" + (_hopCount.length + _hopLimit.length)
                + ",hopCount=" + _hopCount.getValue()
                + ",hopLimit=" + _hopLimit.getValue();
    }
}
