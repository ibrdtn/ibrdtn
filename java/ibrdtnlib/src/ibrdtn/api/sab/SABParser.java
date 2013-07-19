/*
 * SABParser.java
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
package ibrdtn.api.sab;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.LinkedList;
import java.util.List;

public class SABParser {

    private enum State {

        PARSER_RESPONSE,
        PARSER_LIST,
        PARSER_DATA_INITIAL,
        PARSER_DATA_NEXT,
        PARSER_DATA_INITIAL_PAYLOAD,
        PARSER_DATA_NEXT_PAYLOAD,
        PARSER_DATA_PAYLOAD_HEADER,
        PARSER_BUNDLE,
        PARSER_BUNDLE_INFO,
        PARSER_BLOCK,
        PARSER_BLOCK_INFO
    }
    private State state = State.PARSER_RESPONSE;
    private Boolean lastblock = false;
    private Boolean mAbort = false;

    public void reset() {
        this.state = State.PARSER_RESPONSE;
        this.lastblock = false;
    }

    public void abort() {
        this.mAbort = true;
    }

    public void parse(InputStream is, SABHandler handler) throws SABException {
        BufferedReader reader = new BufferedReader(new InputStreamReader(is));

        try {
            handler.startStream();
            while (!mAbort) {
                readData(reader, handler);
            }
        } finally {
            handler.endStream();
        }
    }

    private void readData(BufferedReader reader, SABHandler handler) throws SABException {
        switch (state) {
            case PARSER_RESPONSE:
                readResponse(reader, handler);
                break;

            case PARSER_LIST:
                LinkedList<String> ret = new LinkedList<String>();
                readList(reader, ret);
                handler.response(ret);
                break;

            case PARSER_DATA_PAYLOAD_HEADER:
            case PARSER_BLOCK_INFO:
            case PARSER_BLOCK:
                readBlock(reader, handler);
                break;

            case PARSER_BUNDLE_INFO:
            case PARSER_BUNDLE:
                readBundle(reader, handler);
                break;

            case PARSER_DATA_INITIAL_PAYLOAD:
            case PARSER_DATA_NEXT_PAYLOAD:
            case PARSER_DATA_INITIAL:
            case PARSER_DATA_NEXT:
                readPayload(reader, handler);
                break;

            default:
                break;
        }
    }

    private void readList(BufferedReader reader, List<String> data) throws SABException {
        try {
            String line = null;
            while ((line = reader.readLine()) != null) {
                if (line.length() == 0) {
                    break;
                }
                data.add(line);
            }
        } catch (IOException e) {
            throw new SABException(e.toString());
        } finally {
            this.state = State.PARSER_RESPONSE;
        }
    }

    private void readResponse(BufferedReader reader, SABHandler handler) throws SABException {
        try {
            String data = reader.readLine();
            // System.out.println("Debug received: " + data);

            // if the data is null throw exception
            if (data == null) {
                throw new SABException("end of stream reached");
            }

            // skip empty lines
            while (data.length() == 0) {
                data = reader.readLine();
                if (data == null) {
                    throw new SABException("end of stream reached");
                }
            }

            // split the line into status code and parameter
            String[] values = data.split(" ", 2);

            // check the count of values
            if (values.length == 0) {
                // error
                throw new SABException("invalid data received");
            }

            Integer code = Integer.valueOf(values[0]);

            if (code >= 600) {
                // notify message
                handler.notify(code, values[1]);
                return;
            }

            // forward response code
            handler.response(code, values[1]);

            // except a list as next
            if ((code == 200) && values[1].equals("REGISTRATION LIST")) {
                this.state = State.PARSER_LIST;
            } else if ((code == 200) && values[1].equals("NEIGHBOR LIST")) {
                this.state = State.PARSER_LIST;
            } else if ((code == 200) && values[1].startsWith("BUNDLE GET")) {
                this.state = State.PARSER_BUNDLE;
                handler.startBundle();
            } else if ((code == 200) && values[1].startsWith("BUNDLE INFO")) {
                this.state = State.PARSER_BUNDLE_INFO;
                handler.startBundle();
            } else if ((code == 200) && values[1].startsWith("PAYLOAD GET")) {
                this.state = State.PARSER_DATA_PAYLOAD_HEADER;
            }
        } catch (IOException e) {
            throw new SABException(e.toString());
        } catch (NumberFormatException e) {
            throw new SABException("invalid data received - could not parse number");
        }
    }

    private void readBundle(BufferedReader reader, SABHandler handler) throws SABException {
        try {
            String data = reader.readLine();

            // if the data is null throw exception
            if (data == null) {
                throw new SABException("end of stream reached");
            }

            // read the block, if an empty line was received
            if (data.length() == 0) {
                switch (this.state) {
                    case PARSER_BUNDLE:
                        this.state = State.PARSER_BLOCK;
                        break;
                    case PARSER_BUNDLE_INFO:
                        this.state = State.PARSER_BLOCK_INFO;
                        break;
                }
                return;
            }

            // search for the delimiter
            int delimiter = data.indexOf(':');

            // if the is no delimiter, ignore the line
            if (delimiter == -1) {
                throw new SABException("no delimiter found");
            }

            // split the keyword and data pair
            String keyword = data.substring(0, delimiter);
            String value = data.substring(delimiter + 1, data.length()).trim();

            // forward block parameter
            handler.attribute(keyword, value);
        } catch (IOException e) {
            throw new SABException(e.toString());
        }
    }

    private void readBlock(BufferedReader reader, SABHandler handler) throws SABException {
        try {
            String data = reader.readLine();

            // if the data is null throw exception
            if (data == null) {
                throw new SABException("end of stream reached");
            }

            // read the payload, if an empty line was received
            if (data.length() == 0) {
                switch (this.state) {
                    case PARSER_BLOCK:
                        this.state = State.PARSER_DATA_INITIAL;
                        break;
                    case PARSER_BLOCK_INFO:
                        handler.endBlock();
                        if (this.lastblock) {
                            handler.endBundle();
                            this.lastblock = false;
                            this.state = State.PARSER_RESPONSE;
                        } else {
                            this.state = State.PARSER_BLOCK_INFO;
                        }
                        break;
                    case PARSER_DATA_PAYLOAD_HEADER:
                        this.state = State.PARSER_DATA_INITIAL_PAYLOAD;
                        break;
                }
                return;
            }

            // search for the delimiter
            int delimiter = data.indexOf(':');

            // if the is no delimiter, ignore the line
            if (delimiter == -1) {
                throw new SABException("no delimiter found");
            }

            // split the keyword and data pair
            String keyword = data.substring(0, delimiter);
            String value = data.substring(delimiter + 1, data.length()).trim();

            // if this is the first line of the block...
            if (keyword.equalsIgnoreCase("block")) {
                // start block tag
                handler.startBlock(Integer.parseInt(value));
            } else {
                // forward block parameter
                handler.attribute(keyword, value);

                // if the keyword contains the procflags, then check for lastblock bit
                if (keyword.equalsIgnoreCase("flags")) {
                    lastblock = value.contains("LAST_BLOCK");
                }
            }
        } catch (IOException e) {
            throw new SABException(e.toString());
        }
    }

    private void readPayload(BufferedReader reader, SABHandler handler) throws SABException {
        try {
            String data = reader.readLine();

            // if the data is null throw exception
            if (data == null) {
                throw new SABException("end of stream reached");
            }

            // read the payload, if an empty line was received
            if ((data.length() == 0)
                    && (this.state != State.PARSER_DATA_INITIAL)
                    && (this.state != State.PARSER_DATA_INITIAL_PAYLOAD)) {

                handler.endBlock();

                switch (state) {
                    case PARSER_DATA_NEXT_PAYLOAD:
                        this.state = State.PARSER_RESPONSE;
                        break;
                    case PARSER_DATA_NEXT:
                        if (this.lastblock) {
                            handler.endBundle();
                            this.lastblock = false;
                            this.state = State.PARSER_RESPONSE;
                        } else {
                            this.state = State.PARSER_BLOCK;
                        }
                        break;
                }
                return;
            }

            handler.characters(data);

            // next time we're reading not the first line of data
            switch (state) {
                case PARSER_DATA_INITIAL_PAYLOAD:
                    this.state = State.PARSER_DATA_NEXT_PAYLOAD;
                    break;
                case PARSER_DATA_INITIAL:
                    this.state = State.PARSER_DATA_NEXT;
                    break;
            }
        } catch (IOException e) {
            throw new SABException(e.toString());
        }
    }
}
