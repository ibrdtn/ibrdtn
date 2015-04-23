/*
 * Bundle.java
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

import ibrdtn.api.Timestamp;
import java.util.LinkedList;
import java.util.logging.Level;
import java.util.logging.Logger;

public class Bundle {

    private static final Logger logger = Logger.getLogger(Bundle.class.getName());
    protected EID destination;
    protected SingletonEndpoint source;
    protected SingletonEndpoint custodian;
    protected SingletonEndpoint reportto;
    protected long lifetime;
    protected Timestamp timestamp;
    protected Long sequenceNumber;
    protected long procFlags;
    private Long appDataLength = null;
    private Long fragmentOffset = null;
    protected LinkedList<Block> blocks = new LinkedList<Block>();

    public enum Priority {

        BULK,
        NORMAL,
        EXPEDITED
    }

    public enum Flags {

        FRAGMENT(0),
        ADM_RECORD(1),
        NO_FRAGMENT(2),
        CUSTODY_REQUEST(3),
        DESTINATION_IS_SINGLETON(4),
        APP_ACK_REQUEST(5),
        RESERVED_6(6),
        PRIORITY_BIT1(7),
        PRIORITY_BIT2(8),
        CLASSOFSERVICE_9(9),
        CLASSOFSERVICE_10(10),
        CLASSOFSERVICE_11(11),
        CLASSOFSERVICE_12(12),
        CLASSOFSERVICE_13(13),
        RECEPTION_REPORT(14),
        CUSTODY_REPORT(15),
        FORWARD_REPORT(16),
        DELIVERY_REPORT(17),
        DELETION_REPORT(18),
        // DTNSEC FLAGS (these are customized flags and not written down in any draft)
        DTNSEC_REQUEST_SIGN(26),
        DTNSEC_REQUEST_ENCRYPT(27),
        DTNSEC_STATUS_VERIFIED(28),
        DTNSEC_STATUS_CONFIDENTIAL(29),
        DTNSEC_STATUS_AUTHENTICATED(30),
        COMPRESSION_REQUEST(31);
        private int offset;

        Flags(int offset) {
            this.offset = offset;
        }

        public int getOffset() {
            return offset;
        }
    }

    public Bundle() {
    }

    public Bundle(EID destination, long lifetime) {
        setDestination(destination);
        this.lifetime = lifetime;
        setPriority(Priority.NORMAL);
    }

    /**
     * Set a flag in the bundle
     *
     * @param flag a constant from Bundle.Flags
     * @param value true to set the flag, false to clear it
     */
    public void setFlag(Flags flag, boolean value) {
        logger.log(Level.FINE, "Setting {0}", flag.toString());
        if (value) {
            procFlags |= 0b1L << flag.getOffset();
        } else {
            procFlags &= ~(0b1L << flag.getOffset());
        }
    }

    public Boolean getFlag(Flags flag) {
        return (flag.getOffset() & this.procFlags) > 0;
    }

    public Priority getPriority() {
        if (getFlag(Flags.PRIORITY_BIT1)) {
            return Priority.NORMAL;
        }

        if (getFlag(Flags.PRIORITY_BIT2)) {
            return Priority.EXPEDITED;
        }

        return Priority.BULK;
    }

    public void setPriority(Priority p) {
        switch (p) {
            case BULK:
                setFlag(Flags.PRIORITY_BIT1, false);
                setFlag(Flags.PRIORITY_BIT2, false);
                break;

            case EXPEDITED:
                setFlag(Flags.PRIORITY_BIT1, false);
                setFlag(Flags.PRIORITY_BIT2, true);
                break;

            case NORMAL:
                setFlag(Flags.PRIORITY_BIT1, true);
                setFlag(Flags.PRIORITY_BIT2, false);
                break;
        }
    }

    public EID getDestination() {
        return destination;
    }

    public void setDestination(EID destination) {
        this.destination = destination;
        if (destination instanceof SingletonEndpoint) {
            setFlag(Flags.DESTINATION_IS_SINGLETON, true);
        }
    }

    public SingletonEndpoint getSource() {
        return source;
    }

    public void setSource(SingletonEndpoint source) {
        this.source = source;
    }

    public SingletonEndpoint getCustodian() {
        return custodian;
    }

    /**
     * Usually, the custodian is only set by the daemon.
     *
     * @param custodian
     */
    public void setCustodian(SingletonEndpoint custodian) {
        this.custodian = custodian;
    }

    public SingletonEndpoint getReportto() {
        return reportto;
    }

    public void setReportto(SingletonEndpoint reportto) {
        this.reportto = reportto;
    }

    public long getLifetime() {
        return lifetime;
    }

    public void setLifetime(long lifetime) {
        this.lifetime = lifetime;
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

    public void setSequenceNumber(Long sequenceNumber) {
        this.sequenceNumber = sequenceNumber;
    }

    public Long getAppDataLength() {
        return appDataLength;
    }

    public void setAppDataLength(Long appDataLength) {
        this.appDataLength = appDataLength;
    }

    public Long getFragmentOffset() {
        return fragmentOffset;
    }

    public void setFragmentOffset(Long fragmentOffset) {
        this.fragmentOffset = fragmentOffset;
    }

    public long getProcFlags() {
        return procFlags;
    }

    public void setProcFlags(long procFlags) {
        this.procFlags = procFlags;
    }

    public boolean isSingleton() {
        int single = (int) (procFlags >> Flags.DESTINATION_IS_SINGLETON.getOffset()) & 0b11;
        if (single == 0) {
            return false;
        } else {
            return true;
        }
    }

    /**
     * Appends a new Block to this bundle
     *
     * @param block the block to append
     */
    public void appendBlock(Block block) {
        if (blocks.size() > 0) {
            blocks.getLast().procflags &= ~(1 << Block.FlagOffset.LAST_BLOCK.ordinal());
        }
        block.procflags |= (1 << Block.FlagOffset.LAST_BLOCK.ordinal());
        blocks.addLast(block);
    }

    public LinkedList<Block> getBlocks() {
        return blocks;
    }

    /**
     * Returns the bundle's payload block, or null if no payload block exists.
     *
     * It it assumed that a maximum of one payload block exists.
     *
     * @return
     */
    public PayloadBlock getPayloadBlock() {
        PayloadBlock payload = null;

        for (Block block : getBlocks()) {
            if (block.getType() == PayloadBlock.type) {
                payload = (PayloadBlock) block;
            }
        }
        return payload;
    }

    @Override
    public String toString() {
        StringBuilder builder = new StringBuilder();
        builder.append("Bundle: ");
        builder.append(source);
        builder.append(" -> ");
        builder.append(destination);
        builder.append(" ");
        for (Block block : blocks) {
            builder.append("[");
            builder.append(block);
            builder.append("]");
        }
        return builder.toString();
    }
}
