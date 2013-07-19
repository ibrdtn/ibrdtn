/*
 * BundleID.java
 * 
 * Copyright (C) 2011 IBR, TU Braunschweig
 *
 * Written-by:  Johannes Morgenroth <morgenroth@ibr.cs.tu-bs.de>
 *              Julian Timpner      <timpner@ibr.cs.tu-bs.de>
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

import ibrdtn.api.Timestamp;

public class BundleID {

    private SingletonEndpoint source = null;
    private Timestamp timestamp = null;
    private Long sequenceNumber = null;
    private Long procFlags = null;
    private boolean isFragment = false;
    private Long fragOffset = null;

    public BundleID() {
    }

    public BundleID(Bundle b) {
        this.source = b.getSource();
        this.timestamp = b.getTimestamp();
        this.sequenceNumber = b.getSequenceNumber();

        this.isFragment = b.getFlag(Bundle.Flags.FRAGMENT);
        this.fragOffset = b.getFragmentOffset();
    }

    public BundleID(SingletonEndpoint source, Timestamp timestamp, Long sequenceNumber) {
        this.source = source;
        this.timestamp = timestamp;
        this.sequenceNumber = sequenceNumber;
    }

    public SingletonEndpoint getSource() {
        return source;
    }

    public void setSource(SingletonEndpoint source) {
        this.source = source;
    }

    public Timestamp getTimestamp() {
        return timestamp;
    }

    public void setTimestamp(Timestamp timestamp) {
        this.timestamp = timestamp;
    }

    public Long getSequenceNumber() {
        return sequenceNumber;
    }

    public void setSequenceNumber(Long seq) {
        this.sequenceNumber = seq;
    }

    public long getProcflags() {
        return procFlags;
    }

    public void setProcflags(Long procFlags) {
        this.procFlags = procFlags;
    }

    public void setFragOffset(long fragOffset) {
        this.isFragment = true;
        this.fragOffset = fragOffset;
    }

    public Long getFragOffset() {
        return fragOffset;
    }

    public boolean isFragment() {
        return isFragment;
    }

    public void setFragment(boolean isFragment) {
        this.isFragment = isFragment;
    }

    @Override
    public String toString() {
        if (isFragment) {
            return ((this.timestamp == null) ? "null" : String.valueOf(this.timestamp.getValue()))
                    + " " + String.valueOf(this.sequenceNumber)
                    + " " + String.valueOf(this.fragOffset)
                    + " " + this.source;
        } else {
            return ((this.timestamp == null) ? "null" : String.valueOf(this.timestamp.getValue()))
                    + " " + String.valueOf(this.sequenceNumber)
                    + " " + this.source;
        }
    }

    @Override
    public boolean equals(Object obj) {
        if (obj instanceof BundleID) {
            BundleID foreignId = (BundleID) obj;

            if (timestamp != null) {
                if (!timestamp.equals(foreignId.timestamp)) {
                    return false;
                }
            }

            if (sequenceNumber != null) {
                if (!sequenceNumber.equals(foreignId.sequenceNumber)) {
                    return false;
                }
            }

            if (source != null) {
                if (!source.equals(foreignId.source)) {
                    return false;
                }
            }

            if (isFragment) {
                if (!foreignId.isFragment) {
                    return false;
                }

                if (fragOffset != null) {
                    if (!fragOffset.equals(foreignId.fragOffset)) {
                        return false;
                    }
                }
            }

            return true;
        }
        return super.equals(obj);
    }
}
