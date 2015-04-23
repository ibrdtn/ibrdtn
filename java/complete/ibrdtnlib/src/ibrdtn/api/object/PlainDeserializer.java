/*
 * PlainDeserializer.java
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
import java.io.OutputStream;

import java.util.List;
import java.util.Queue;
import java.util.concurrent.LinkedBlockingQueue;

import ibrdtn.api.Base64;
import ibrdtn.api.NullOutputStream;
import ibrdtn.api.Timestamp;
import ibrdtn.api.sab.SABException;
import ibrdtn.api.sab.SABHandler;

public class PlainDeserializer implements SABHandler {

    public interface Callback {

        /**
         * This method is called, when block data arrives. The Callback can either provide an OutputStream to write the
         * data to or throw a SkipDataException if the data should be skipped
         *
         * @param bundle The current bundle to which the data belongs to.
         * @param header The block header of the current block
         * @param dataLength the length of the data that is expected (this is not enforced)
         * @return An OutputStream, that the data shall be written to
         * @throws SkipDataException Notifies the caller that the data shall be skipped.
         */
        public OutputStream getBlockDataOutputStream(Bundle bundle, BlockHeader header, int dataLength) throws SkipDataException;

        /**
         * Returns a Block.Data object that encapsulates the data read into the OutputStream that was returned by the
         * last call to getBlockDataOutputStream().
         *
         * @return A Block.Data object containing the data.
         */
        public Block.Data getBlockData();

        static class SkipDataException extends Exception {

            /**
             * generated serialVersionUID
             */
            private static final long serialVersionUID = 5191170130535168593L;

            SkipDataException() {
            }
        }
    }

    private enum State {

        INVALID,
        BUNDLE,
        BLOCK,
    }
    private Callback _callback;
    private Queue<Bundle> _bundleQueue = new LinkedBlockingQueue<Bundle>();
    private Bundle _currentBundle;
    private BlockHeader _currentBlockHeader;
    private int _currentDataLength = 0;
    private Base64.OutputStream _currentDataOutputStream;
    private State _state = State.INVALID;

    public PlainDeserializer(Callback callback) {
        _callback = callback;
    }

    public Bundle nextBundle() {
        return _bundleQueue.remove();
    }

    @Override
    public void attribute(String keyword, String value) {
        try {
            switch (_state) {
                case BUNDLE:
                    if (_currentBundle == null) {
                        break;
                    }
                    if (keyword.equalsIgnoreCase("destination")) {
                        _currentBundle.destination = new SingletonEndpoint(value);
                    } else if (keyword.equalsIgnoreCase("source")) {
                        _currentBundle.source = new SingletonEndpoint(value);
                    } else if (keyword.equalsIgnoreCase("custodian")) {
                        _currentBundle.custodian = new SingletonEndpoint(value);
                    } else if (keyword.equalsIgnoreCase("reportto")) {
                        _currentBundle.reportto = new SingletonEndpoint(value);
                    } else if (keyword.equalsIgnoreCase("lifetime")) {
                        _currentBundle.lifetime = Long.parseLong(value);
                    } else if (keyword.equalsIgnoreCase("timestamp")) {
                        _currentBundle.timestamp = new Timestamp(Long.parseLong(value));
                    } else if (keyword.equalsIgnoreCase("sequencenumber")) {
                        _currentBundle.sequenceNumber = Long.parseLong(value);
                    } else if (keyword.equalsIgnoreCase("procflags")) {
                        _currentBundle.procFlags = Long.parseLong(value);
                    }
                    break;
                case BLOCK:
                    if (_currentBundle == null) {
                        break;
                    }
                    if (keyword.equalsIgnoreCase("flags")) {
                        // split the flags on whitespaces
                        String[] flagArray = value.trim().split("\\s+");
                        for (String flag : flagArray) {
                            try {
                                _currentBlockHeader.setFlag(Block.FlagOffset.valueOf(flag), true);
                            } catch (IllegalArgumentException e) {
                            }
                        }
                    } else if (keyword.equalsIgnoreCase("eid")) {
                        _currentBlockHeader.addEID(new SingletonEndpoint(value));
                    } else if (keyword.equalsIgnoreCase("length")) {
                        _currentDataLength = Integer.parseInt(value);
                    }
                    break;
                default:
                    break;
            }
        } catch (NumberFormatException e) {
            //catch all exceptions from parseInt/parseLong and ignore that attribute
        }
    }

    @Override
    public void characters(String data) throws SABException {
        if (_currentDataLength == 0) {
            return;
        }
        if (_currentDataOutputStream == null) {
            try {
                _currentDataOutputStream = new Base64.OutputStream(
                        _callback.getBlockDataOutputStream(_currentBundle,
                        _currentBlockHeader, _currentDataLength), Base64.DECODE);
            } catch (Callback.SkipDataException ex) {
                _currentDataOutputStream = new Base64.OutputStream(new NullOutputStream());
            }
        }
        try {
            _currentDataOutputStream.write(data.getBytes());
        } catch (IOException ex) {
        }
    }

    @Override
    public void endBlock() {
        try {
            Block.Data blockData;
            if (_currentDataLength == 0) {
                blockData = new Block.Data() {
                    public void writeTo(OutputStream stream) throws IOException {
                    }

                    public long size() {
                        return 0l;
                    }
                };
            } else {
                _currentDataOutputStream.flushBase64();
                _currentDataOutputStream.flush();
                blockData = _callback.getBlockData();
            }
            _currentBundle.appendBlock(Block.createBlock(_currentBlockHeader, blockData));
        } catch (NullPointerException ex) {
        } catch (IOException ex) {
        } catch (Block.InvalidDataException ex) {
        } finally {
            //reset all data associated to the current block
            _currentBlockHeader = null;
            _currentDataOutputStream = null;
            _currentDataLength = 0;
            _state = State.INVALID;
        }
    }

    @Override
    public void endBundle() {
        if (_currentBundle != null) {
            _bundleQueue.add(_currentBundle);
        }
        //reset all data associated to the current bundle
        _currentBundle = null;
        _state = State.INVALID;
    }

    @Override
    public void endStream() {
    }

    @Override
    public void notify(Integer type, String data) {
    }

    @Override
    public void response(Integer type, String data) {
    }

    @Override
    public void response(List<String> data) {
    }

    @Override
    public void startBlock(Integer type) {
        _currentBlockHeader = new BlockHeader(type);
        _state = State.BLOCK;
    }

    @Override
    public void startBundle() {
        _currentBundle = new Bundle();
        _state = State.BUNDLE;
    }

    @Override
    public void startStream() {
    }
}
