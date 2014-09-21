package de.tubs.ibr.dtn.discovery;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.AlarmManager;
import android.app.PendingIntent;
import android.app.Service;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothManager;
import android.content.Context;
import android.content.Intent;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.SystemClock;
import android.preference.PreferenceManager;
import android.support.v4.content.WakefulBroadcastReceiver;
import android.util.Log;
import android.util.SparseArray;

import de.tubs.ibr.dtn.daemon.Preferences;

@TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
public class DiscoveryService extends Service {

    public static final String TAG = DiscoveryService.class.getSimpleName();

    /**
     * Prefix used in the name of a bluetooth device that announces a DTN node
     */
    public static final String DTN_DISCOVERY_PREFIX = "DTN:";
    
    public static final String ACTION_BLE_NODE_DISCOVERED = "de.tubs.ibr.dtn.Intent.BLE_NODE_DISCOVERED";
    public static final String EXTRA_BLE_NODE = "de.tubs.ibr.dtn.Intent.BLE_NODE";

    private Handler mHandler = new Handler();
    private BluetoothManager mBluetoothManager;
    private BluetoothAdapter mBluetoothAdapter;
    private WifiManager mWifiManager;
    private AlarmManager mAlarmManager;
    private Intent mWakefulIntent;
    private SparseArray<Long> mDiscoveredDtnNodes;

    private BluetoothAdapter.LeScanCallback mLeScanCallback =
            new BluetoothAdapter.LeScanCallback() {
                @Override
                public void onLeScan(final BluetoothDevice device, int rssi, byte[] scanRecord) {
                    Log.i(TAG, "onLeScan: " + device.getAddress() + " (rssi=" + rssi + ")");
                    if (device.getName().startsWith(DTN_DISCOVERY_PREFIX)) {
                        onDtnNodeDiscovered(device.getAddress());
                    }
                }
            };

    private void onDtnNodeDiscovered(String address) {

        // throttle announcements to once per minute
        int nodeHash = address.hashCode();
        Long lastDiscoveryTime = mDiscoveredDtnNodes.get(nodeHash);
        if (lastDiscoveryTime != null && System.currentTimeMillis() < lastDiscoveryTime + 60*1000 ) {
            return;
        }
        mDiscoveredDtnNodes.put(nodeHash, System.currentTimeMillis());

        Log.d(TAG, "found DTN node with address: " + address);
        String ssid = DTN_DISCOVERY_PREFIX + "//" + address;

        WifiConfiguration wifiConfig = new WifiConfiguration();
        wifiConfig.SSID = String.format("\"%s\"", ssid);
        wifiConfig.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.NONE);

        int networkId = mWifiManager.addNetwork(wifiConfig);
        mWifiManager.disconnect();
        mWifiManager.enableNetwork(networkId, true);
        mWifiManager.reconnect();

        // TODO announce endpoint to IBR-DTN (EID == SSID)
        Intent discoveredIntent = new Intent(ACTION_BLE_NODE_DISCOVERED);
        discoveredIntent.putExtra(EXTRA_BLE_NODE, ssid);
        sendBroadcast(discoveredIntent);
    }

    @Override
    public void onCreate() {
        super.onCreate();
        initialize();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        mWakefulIntent = intent;
        startScan();
        return super.onStartCommand(intent, flags, startId);
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    /**
     * Initializes a reference to the local Bluetooth adapter.
     *
     * @return Return true if the initialization is successful.
     */
    @TargetApi(18)
    public boolean initialize() {

        if (Build.VERSION.SDK_INT < 18) {
          // Bluetooth Low Energy is not supported
          return false;
        }

        // For API level 18 and above, get a reference to BluetoothAdapter through
        // BluetoothManager.
        if (mBluetoothManager == null) {
            mBluetoothManager = (BluetoothManager) getSystemService(Context.BLUETOOTH_SERVICE);
            if (mBluetoothManager == null) {
                Log.e(TAG, "Unable to initialize BluetoothManager.");
                return false;
            }
        }

        mBluetoothAdapter = mBluetoothManager.getAdapter();
        if (mBluetoothAdapter == null) {
            Log.e(TAG, "Unable to obtain a BluetoothAdapter.");
            return false;
        }

        mAlarmManager = (AlarmManager) getSystemService(Context.ALARM_SERVICE);

        mWifiManager = (WifiManager) getSystemService(WIFI_SERVICE);
        mDiscoveredDtnNodes = new SparseArray<Long>();

        return true;
    }

    @SuppressLint("NewApi")
	private void startScan() {
      // TODO set proper values for scan duration and delay
        final int duration = Integer.parseInt(PreferenceManager.getDefaultSharedPreferences(getApplicationContext()).getString(Preferences.KEY_SCAN_DURATION, "2200"));
        final int delay = Integer.parseInt(PreferenceManager.getDefaultSharedPreferences(getApplicationContext()).getString(Preferences.KEY_SCAN_DELAY, "2200"));

        if (duration <= 0) {
            Log.i(TAG, "scan duration invalid: " + duration + " ms");
            return;
        }
        Log.i(TAG, "scanning for " + duration + " ms");
        mBluetoothAdapter.startLeScan(mLeScanCallback);
        mHandler.postDelayed(new Runnable() {
            @Override
            public void run() {
                mBluetoothAdapter.stopLeScan(mLeScanCallback);
                if (!PreferenceManager.getDefaultSharedPreferences(getApplicationContext()).getBoolean(Preferences.KEY_BLE_ENABLED, false))
                    return;
                Log.i(TAG, "scanning again in " + delay + " ms");
                // TODO schedule new scan with AlarmManager
                PendingIntent intent = PendingIntent.getBroadcast(getApplicationContext(), 0, new Intent(WakefulDiscoveryReceiver.ACTION_BLE_DISCOVERY), 0);
                if (Build.VERSION.SDK_INT >= 19) {
                    mAlarmManager.setExact(AlarmManager.ELAPSED_REALTIME_WAKEUP, SystemClock.elapsedRealtime() + delay, intent);
                } else {
                    mAlarmManager.set(AlarmManager.ELAPSED_REALTIME_WAKEUP, SystemClock.elapsedRealtime() + delay, intent);
                }
                // release wakelock after scan has finished
                WakefulBroadcastReceiver.completeWakefulIntent(mWakefulIntent);
            }
        }, duration);
    }

}
