/*
 * CountingOutputStream.java
 * 
 * Copyright (C) 2011 IBR, TU Braunschweig
 *
 * Written-by: Johannes Morgenroth <morgenroth@ibr.cs.tu-bs.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
package ibrdtn.api;

import java.io.IOException;
import java.io.OutputStream;

public class CountingOutputStream extends OutputStream {

    private final OutputStream proxy;
    private long count = 0;

    public CountingOutputStream(final OutputStream out) {
        assert (out != null);
        this.proxy = out;
    }

    public void reset() {
        this.count = 0;
    }

    public long getCount() {
        if (count >= 0 && count <= Long.MAX_VALUE) {
            return count;
        }
        throw new IllegalStateException("out bytes exceeds Long.MAX_VALUE: " + count);
    }

    @Override
    public void write(int b) throws IOException {
        ++count; // just 1 byte is wrote
        proxy.write(b);
    }

    @Override
    public void write(byte[] b, int off, int len) throws IOException {
        count += len;
        proxy.write(b, off, len);
    }

    @Override
    public void write(byte[] b) throws IOException {
        count += b.length;
        proxy.write(b);
    }

    @Override
    public void close() throws IOException {
        proxy.close();
    }

    @Override
    public void flush() throws IOException {
        proxy.flush();
    }
}