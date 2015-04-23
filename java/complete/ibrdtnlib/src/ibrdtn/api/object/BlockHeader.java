/*
 * BlockHeader.java
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

import java.util.Collections;
import java.util.Set;
import java.util.TreeSet;

public class BlockHeader {

    public enum FlagOffset {

        REPLICATE_IN_EVERY_FRAGMENT,
        TRANSMIT_STATUSREPORT_IF_NOT_PROCESSED,
        DELETE_BUNDLE_IF_NOT_PROCESSED,
        LAST_BLOCK,
        DISCARD_IF_NOT_PROCESSED,
        FORWARDED_WITHOUT_PROCESSED,
        BLOCK_CONTAINS_EIDS
    }
    protected long procflags = 0;
    private Long length;
    protected TreeSet<EID> _eids = new TreeSet<EID>();
    private int _type;

    public BlockHeader(int type) {
        _type = type;
    }

    public int getType() {
        return _type;
    }

    public void setType(int _type) {
        this._type = _type;
    }

    public Long getLength() {
        return length;
    }

    public void setLength(Long length) {
        this.length = length;
    }

    /**
     * Sets flags on this BlockHeader. Multiple flags can be concatenated and separated by whitespaces.
     *
     * Flag.Offset.BLOCK_CONTAINS_EIDS is ignored and will automatically be set by addEID and removeEID.
     *
     * @param value the flags to be set/cleared
     */
    public void setFlags(String value) {
        String[] flags = value.split(" ");
        for (String flag : flags) {
            FlagOffset offset = FlagOffset.valueOf(flag);
            setFlag(offset, true);
        }
    }

    /**
     * Set/clear a flag on this BlockHeader. Flag.Offset.BLOCK_CONTAINS_EIDS is ignored and will automatically be set by
     * addEID and removeEID.
     *
     * @param flag the flag to be set/clear
     * @param state true to set, false to clear
     */
    public void setFlag(FlagOffset flag, boolean state) {
        if (flag == FlagOffset.BLOCK_CONTAINS_EIDS) {
            return;
        }
        _setFlag(flag, state);
    }

    private void _setFlag(FlagOffset flag, boolean state) {
        if (state) {
            procflags |= (1 << flag.ordinal());
        } else {
            procflags &= ~(1 << flag.ordinal());
        }
    }

    /**
     * Get the state of a block flag
     *
     * @param flag the flag in question
     * @return true if the flag is set, false otherwise
     */
    public boolean getFlag(FlagOffset flag) {
        return ((procflags >> flag.ordinal()) & 0x1) == 0x1;
    }

    /**
     * add a EID to this Bundle
     *
     * @param eid the EID to add
     */
    public void addEID(EID eid) {
        _eids.add(eid);
        fixEIDFlag();
    }

    /**
     * remove a EID from this Bundle
     *
     * @param eid the EID to remove
     */
    public void removeEID(EID eid) {
        _eids.remove(eid);
        fixEIDFlag();
    }

    /**
     * Set the EIDs in this bundle to the given Set
     *
     * @param eids the eid set
     */
    public void setEIDs(TreeSet<EID> eids) {
        _eids = eids;
        fixEIDFlag();
    }

    private void fixEIDFlag() {
        if (_eids.isEmpty()) {
            _setFlag(FlagOffset.BLOCK_CONTAINS_EIDS, false);
        } else {
            _setFlag(FlagOffset.BLOCK_CONTAINS_EIDS, true);
        }
    }

    /**
     * Get a unmodifiable Set of EIDs associated with this BlockHeader
     *
     * @return the EIDs of this BlockHeader
     */
    public final Set<EID> getEIDS() {
        return Collections.unmodifiableSet(_eids);
    }

    void setFlags(long flags) {
        procflags = flags;
        fixEIDFlag();
    }
}
