/*
 * SABHandler.java
 * 
 * Copyright (C) 2011 IBR, TU Braunschweig
 *
 * Written-by: Julian Timpner <timpner@ibr.cs.tu-bs.de>
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
/**
 * This is a handler that client applications can implement in order to receive Simple API events.
 *
 * @author Julian Timpner <timpner@ibr.cs.tu-bs.de>
 */
package ibrdtn.api.sab;

import ibrdtn.api.object.Block;
import ibrdtn.api.object.Bundle;
import ibrdtn.api.object.BundleID;
import java.io.OutputStream;

public interface CallbackHandler {

    public void notify(BundleID id);

    public void notify(StatusReport r);

    public void notify(Custody c);

    public void startBundle(Bundle bundle);

    public void endBundle();

    public void startBlock(Block block);

    public void endBlock();

    public OutputStream startPayload();

    public void endPayload();

    public void progress(long pos, long total);
}
