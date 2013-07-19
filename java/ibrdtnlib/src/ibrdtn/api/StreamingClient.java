/*
 * StreamingClient.java
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

import ibrdtn.api.Client;
import ibrdtn.api.object.EID;
import ibrdtn.api.object.SingletonEndpoint;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.net.UnknownHostException;

public class StreamingClient extends Client {

    public String endpoint = null;
    public EID destination = null;
    public Integer chunksize = null;
    public Integer lifetime = null;
    public Integer timeout = null;

    @Override
    public void open() throws UnknownHostException, IOException {
        super.open();

        BufferedReader reader = new BufferedReader(new InputStreamReader(istream));
        BufferedWriter writer = new BufferedWriter(new OutputStreamWriter(ostream));

        // switch to event protocol
        writer.write("protocol streaming");
        writer.newLine();
        writer.flush();

        // read confirmation
        if (readResponse(reader) != 200) {
            throw new IOException();
        }

        // set endpoint
        if (endpoint != null) {
            writer.write("set endpoint " + endpoint);
            writer.newLine();
            writer.flush();

            // read confirmation
            if (readResponse(reader) != 200) {
                throw new IOException();
            }
        }

        // set endpoint
        if (destination != null) {
            if (destination instanceof SingletonEndpoint) {
                writer.write("set destination " + destination.toString());
            } else {
                writer.write("set group " + destination.toString());
            }
            writer.newLine();
            writer.flush();

            // read confirmation
            if (readResponse(reader) != 200) {
                throw new IOException();
            }
        }

        // set lifetime
        if (lifetime != null) {
            writer.write("set lifetime " + lifetime.toString());
            writer.newLine();
            writer.flush();

            // read confirmation
            if (readResponse(reader) != 200) {
                throw new IOException();
            }
        }

        // set timeout
        if (timeout != null) {
            writer.write("set timeout " + timeout.toString());
            writer.newLine();
            writer.flush();

            // read confirmation
            if (readResponse(reader) != 200) {
                throw new IOException();
            }
        }

        // set chunksize
        if (chunksize != null) {
            writer.write("set chunksize " + chunksize.toString());
            writer.newLine();
            writer.flush();

            // read confirmation
            if (readResponse(reader) != 200) {
                throw new IOException();
            }
        }

        // finally connect
        writer.write("connect");
        writer.newLine();
        writer.flush();

        // read confirmation
        if (readResponse(reader) != 100) {
            throw new IOException();
        }
    }

    private Integer readResponse(BufferedReader reader) throws IOException {
        String data = reader.readLine();

        // if the data is null, just return
        if (data == null) {
            return null;
        }

        // empty line are unexpected here
        if (data.length() == 0) {
            return null;
        }

        // split the line into status code and parameter
        String[] values = data.split(" ", 2);

        // check the count of values
        if (values.length == 0) {
            // error
            return null;
        }

        Integer code = Integer.valueOf(values[0]);
        return code;
    }

    public OutputStream getOutputStream() {
        return this.ostream;
    }

    public InputStream getInputStream() {
        return this.istream;
    }
}
