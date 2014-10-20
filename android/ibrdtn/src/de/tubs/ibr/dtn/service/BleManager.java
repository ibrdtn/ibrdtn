
package de.tubs.ibr.dtn.service;

import java.util.Locale;

import android.annotation.TargetApi;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.util.Log;
import de.tubs.ibr.dtn.swig.EID;
import de.tubs.ibr.dtn.swig.NativeP2pManager;

@TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
public class BleManager extends NativeP2pManager {

	private final static String TAG = "BleManager";
	
	public static final String INTENT_BLE_STATE_CHANGED = "de.tubs.ibr.dtn.ble.action.STATE_CHANGED";
	public static final String EXTRA_BLE_STATE = "de.tubs.ibr.dtn.ble.action.EXTRA_STATE";
	
	/**
	 * Prefix used in the name of a Bluetooth device that announces a DTN node
	 */
	public static final String DTN_DISCOVERY_PREFIX = "dtn:";

	private BluetoothManager mBluetoothManager = null;
	private BluetoothAdapter mBluetoothAdapter = null;
	
	private DaemonService mService = null;
	
	private boolean mServiceEnabled = false;
	private boolean mDiscoveryEnabled = false;
	
	private enum ManagerState {
		UNINITIALIZED,
		INACTIVE,
		ACTIVE
	};

	private ManagerState mManagerState = ManagerState.UNINITIALIZED;

	public BleManager(DaemonService service) {
		super("P2P:BT");
		this.mService = service;
	}
	
	public synchronized boolean isActive() {
		return ManagerState.ACTIVE.equals(mManagerState);
	}
	
	public synchronized void setEnabled(boolean enabled) {
		// allow BT-LE scanning
		mServiceEnabled = enabled;

		if (enabled && mDiscoveryEnabled) {
			// start discovery on enable if discovery is active
			startDiscovery();
		}
		else if (!enabled && mDiscoveryEnabled) {
			// stop discovery on disable
			stopDiscovery();
			mDiscoveryEnabled = true;
		}
	}
	
	private void setState(ManagerState state) {
		// new state debug
		Log.d(TAG, "state transition: " + mManagerState.toString() + " -> " + state.toString());
		
		// change state
		mManagerState = state;

		// generate intent
		Intent i = new Intent(INTENT_BLE_STATE_CHANGED);
		i.putExtra(EXTRA_BLE_STATE, ManagerState.ACTIVE.equals(state));
		mService.sendBroadcast(i);
	}
	
	/**
	 * Initializes a reference to the local Bluetooth adapter.
	 *
	 * @return Return true if the initialization is successful.
	 */
	public synchronized void create() {
		// allowed transition from UNINITIALIZED
		if (ManagerState.UNINITIALIZED.equals(mManagerState)) {
			onCreate();
		}
	}
	
	public synchronized void destroy() {
		if (ManagerState.ACTIVE.equals(mManagerState)) {
			onBleDisabled();
		}
		
		// allowed transition from ACTIVE and INACTIVE
		if (ManagerState.INACTIVE.equals(mManagerState)) {
			onDestroy();
		}
	}
	
	private synchronized void onCreate() {
		// For API level 18 and above, get a reference to BluetoothAdapter through
		// BluetoothManager.
		mBluetoothManager = (BluetoothManager) mService.getSystemService(Context.BLUETOOTH_SERVICE);

		// set the current state
		setState(ManagerState.INACTIVE);
		
		// enable blue-tooth
		onBleEnabled();
	}
	
	/**
	 * This method is called if the BT-LE has been initialized
	 */
	private synchronized void onBleEnabled() {
		// transition only from INACTIVE state
		if (!ManagerState.INACTIVE.equals(mManagerState)) return;
		
		// stop here if BT-LE is not supported
		if (!mService.getPackageManager().hasSystemFeature(PackageManager.FEATURE_BLUETOOTH_LE)) return;

		if (mBluetoothManager == null) return;

		// get the blue-tooth adapter
		mBluetoothAdapter = mBluetoothManager.getAdapter();
		
		if (mBluetoothAdapter == null) {
			Log.e(TAG, "Unable to obtain a BluetoothAdapter.");
			return;
		}
		
		// set the current state
		setState(ManagerState.ACTIVE);
		
		// restart discovery
		if (mDiscoveryEnabled) {
			startDiscovery();
		}
	}
	
	private synchronized void onBleDisabled() {
		// transition only from ACTIVE state
		if (!ManagerState.ACTIVE.equals(mManagerState)) return;
		
		// stop scanning
		stopDiscovery();
		
		// reset adapter
		mBluetoothAdapter = null;
		
		// set the current state
		setState(ManagerState.INACTIVE);
	}
	
	private synchronized void onDestroy() {
		mBluetoothManager = null;

		// set the current state
		setState(ManagerState.UNINITIALIZED);
	}

	@Override
	public synchronized void connect(String identifier) {
		// check if service is enabled
		if (!mServiceEnabled) return;
		
		// check if stack is active
		if (!ManagerState.ACTIVE.equals(mManagerState)) return;
		
		WifiManager wifiManager = (WifiManager) mService.getSystemService(Context.WIFI_SERVICE);
		
		// connect to network with matching SSID if not already connected
		WifiInfo wifiInfo = wifiManager.getConnectionInfo();
		
		if (!wifiInfo.getSSID().equals(String.format("\"%s\"", identifier))) {
			Log.d(TAG, "connecting to SSID " + identifier);
			WifiConfiguration wifiConfig = new WifiConfiguration();
			wifiConfig.SSID = String.format("\"%s\"", identifier);
			wifiConfig.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.NONE);

			int networkId = wifiManager.addNetwork(wifiConfig);
			wifiManager.disconnect();
			wifiManager.enableNetwork(networkId, true);
			wifiManager.reconnect();
		}
	}
	
	@Override
	public void disconnect(String data) {
		// check if stack is active
		if (!ManagerState.ACTIVE.equals(mManagerState)) return;

		// disconnect from the peer identified by "data"
		Log.i(TAG, "disconnect request: " + data);
	}
	
	public synchronized void startDiscovery() {
		// set disco marker
		mDiscoveryEnabled = true;
		
		// check if service is enabled
		if (!mServiceEnabled) return;

		// check if stack is active
		if (!ManagerState.ACTIVE.equals(mManagerState)) return;
		
		mBluetoothAdapter.startLeScan(mLeScanCallback);
	}
	
	public synchronized void stopDiscovery() {
		// set disco marker
		mDiscoveryEnabled = false;
		
		// check if stack is active
		if (!ManagerState.ACTIVE.equals(mManagerState)) return;

		mBluetoothAdapter.stopLeScan(mLeScanCallback);
	}

	private BluetoothAdapter.LeScanCallback mLeScanCallback = new BluetoothAdapter.LeScanCallback() {
		@Override
		public void onLeScan(final BluetoothDevice device, int rssi, byte[] scanRecord) {
			// check if service is enabled
			if (!mServiceEnabled) return;
			
			Log.i(TAG, "onLeScan: " + device.getAddress() + " (rssi=" + rssi + ")");

			if (device.getName().toLowerCase(Locale.US).startsWith(DTN_DISCOVERY_PREFIX)) {
				// transform name to EID
				String deviceId = device.getName().replace(" ", "-");
				EID endpointId = new EID("dtn://" + deviceId);

				Log.d(TAG, "found DTN node " + deviceId + " with address " + device.getAddress());

				// announce endpoint to IBR-DTN daemon
				fireDiscovered(endpointId, deviceId, 90, 10);
			}
		}
	};
}
