/*
 * NullOutputStream.java
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
package ibrdtn.api;

import java.io.IOException;
import java.io.OutputStream;

public class NullOutputStream extends OutputStream {

    private long count = 0;

    public NullOutputStream() {
    }

    public void reset() {
        this.count = 0;
    }

    public long getCount() {
        if(count >= 0 && count <= Long.MAX_VALUE) {
            return count;
        }
        throw new IllegalStateException("out bytes exceeds Long.MAX_VALUE: " + count);
    }

    @Override
    public void write(int b) throws IOException {
        ++count; // just 1 byte is wrote
    }

    @Override
    public void write(byte[] b, int off, int len) throws IOException {
        count += len;
    }

    @Override
    public void write(byte[] b) throws IOException {
        count += b.length;
    }

    @Override
    public void close() throws IOException {
    }

    @Override
    public void flush() throws IOException {
    }
}