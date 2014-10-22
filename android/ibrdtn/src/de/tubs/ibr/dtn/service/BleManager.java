
package de.tubs.ibr.dtn.service;

import android.annotation.TargetApi;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Build;
import android.util.Log;
import de.tubs.ibr.dtn.discovery.DiscoveryService;
import de.tubs.ibr.dtn.swig.EID;
import de.tubs.ibr.dtn.swig.NativeP2pManager;

@TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
public class BleManager extends NativeP2pManager {

	private final static String TAG = "BleManager";

	private DaemonService mService = null;

	public BleManager(DaemonService service) {
		super("P2P:BT");
		this.mService = service;
		IntentFilter ble_filter = new IntentFilter(DiscoveryService.ACTION_BLE_NODE_DISCOVERED);
		mService.registerReceiver(mBleEventReceiver, ble_filter);
	}

	@Override
	public void connect(String identifier) {
		// Connection is handled within the Bluetooth LE Discovery Service
	}

	private BroadcastReceiver mBleEventReceiver = new BroadcastReceiver() {

		@Override
		public void onReceive(Context context, Intent intent) {
			Log.d(TAG, "BLE action: " + intent);

			String action = intent.getAction();

			if (DiscoveryService.ACTION_BLE_NODE_DISCOVERED.equals(action)) {
				String eid = intent.getExtras().getString(DiscoveryService.EXTRA_BLE_NODE, "");
				Log.d(TAG, "announcing EID: " + eid);
				fireDiscovered(new EID(eid), eid, 90, 10);
			}
		}
	};

}
