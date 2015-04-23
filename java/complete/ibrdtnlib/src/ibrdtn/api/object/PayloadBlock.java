/*
 * PayloadBlock.java
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

public class PayloadBlock extends Block {

    public static final int type = 1;
    private Data _data;

    public PayloadBlock() {
        super(type);
    }

    public PayloadBlock(Data data) {
        super(type);
        _data = data;
    }

    public PayloadBlock(Object obj) {
        super(type);
        _data = new ObjectBlockData(obj);
    }

    public PayloadBlock(byte[] data) {
        super(type);
        _data = new ByteArrayBlockData(data);
    }

    public void setData(byte[] data) {
        _data = new ByteArrayBlockData(data);
    }

    public void setData(Object data) {
        _data = new ObjectBlockData(data);
    }

    @Override
    public Data getData() {
        return _data;
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder("PayloadBlock");
        if (getLength() != null) {
            sb.append(": length=").append(getLength());
        }
        return sb.toString();
    }
}
