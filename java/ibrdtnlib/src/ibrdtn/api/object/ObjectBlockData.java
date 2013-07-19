/*
 * ObjectBlockData.java
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

import ibrdtn.api.NullOutputStream;
import java.io.IOException;
import java.io.ObjectOutputStream;
import java.io.OutputStream;

/**
 * This class wraps an object to be used as block data. The object will be serialized when written to a stream.
 */
public class ObjectBlockData extends Block.Data {

    Object _obj;

    public ObjectBlockData(Object obj) {
        _obj = obj;
    }

    @Override
    public long size() {
        NullOutputStream nullStream = new NullOutputStream();
        try {
            writeTo(nullStream);
            nullStream.flush();
            return nullStream.getCount();
        } catch (IOException e) {
            return 0;
        }
    }

    @Override
    public void writeTo(OutputStream stream) throws IOException {
        ObjectOutputStream oos = new ObjectOutputStream(stream);
        oos.writeObject(_obj);
    }
}
