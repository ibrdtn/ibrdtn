/*
 * Preferences.java
 * 
 * Copyright (C) 2011 IBR, TU Braunschweig
 *
 * Written-by: Johannes Morgenroth <morgenroth@ibr.cs.tu-bs.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

package de.tubs.ibr.dtn.daemon;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintStream;
import java.nio.ByteBuffer;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Date;
import java.text.SimpleDateFormat;
import java.util.HashSet;
import java.util.Map;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.ActionBar;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.ServiceConnection;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager.NameNotFoundException;
import android.net.ConnectivityManager;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Parcelable;
import android.os.RemoteException;
import android.preference.CheckBoxPreference;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.PreferenceActivity;
import android.preference.PreferenceCategory;
import android.preference.PreferenceManager;
import android.preference.SwitchPreference;
import android.provider.Settings.Secure;
import android.util.Log;
import android.view.Gravity;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.widget.CompoundButton;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.Switch;
import de.tubs.ibr.dtn.R;
import de.tubs.ibr.dtn.api.SingletonEndpoint;
import de.tubs.ibr.dtn.keyexchange.KeyInformationActivity;
import de.tubs.ibr.dtn.service.ControlService;
import de.tubs.ibr.dtn.service.DaemonService;
import de.tubs.ibr.dtn.service.DaemonStorageUtils;
import de.tubs.ibr.dtn.service.P2pManager;

public class Preferences extends PreferenceActivity {
	
	private static final String TAG = "Preferences";
	
	public static final String KEY_ENABLED = "enabledSwitch";
	public static final String KEY_ENDPOINT_ID = "endpoint_id";
	public static final String KEY_DISCOVERY_MODE = "discovery_mode";
	public static final String KEY_BEACON_INTERVAL = "beacon_interval";
	public static final String KEY_P2P_ENABLED = "p2p_enabled";
	public static final String KEY_DEBUG_MODE = "debugging";
	
	public static final String KEY_LOG_OPTIONS = "log_options";
	public static final String KEY_LOG_DEBUG_VERBOSITY = "log_debug_verbosity";
	public static final String KEY_LOG_ENABLE_FILE = "log_enable_file";
	
	public static final String KEY_SECURITY_BAB_KEY = "security_bab_key";
	public static final String KEY_ROUTING = "routing";
	public static final String KEY_SECURITY_MODE = "security_mode";
	public static final String KEY_TIMESYNC_MODE = "timesync_mode";
	public static final String KEY_STORAGE_MODE = "storage_mode";
	public static final String KEY_UPLINK_MODE = "uplink_mode";
	
	// CloudUplink Parameter
	private static final SingletonEndpoint __CLOUD_EID__ = new SingletonEndpoint("dtn://cloud.dtnbone.dtn");
	private static final String __CLOUD_PROTOCOL__ = "tcp";
	private static final String __CLOUD_ADDRESS__ = "134.169.35.130"; // quorra.ibr.cs.tu-bs.de";
	private static final String __CLOUD_PORT__ = "4559";

	// These preferences show their value as summary
	private final static String[] mSummaryPrefs = {
		KEY_ENDPOINT_ID, KEY_ROUTING, KEY_SECURITY_MODE, KEY_LOG_OPTIONS, KEY_LOG_DEBUG_VERBOSITY,
		KEY_TIMESYNC_MODE, KEY_STORAGE_MODE, KEY_UPLINK_MODE, KEY_DISCOVERY_MODE, KEY_BEACON_INTERVAL
	};

	private Boolean mBound = false;
	private ControlService mService = null;

	private Switch actionBarSwitch = null;
	private CheckBoxPreference checkBoxPreference = null;
	private InterfacePreferenceCategory mInterfacePreference = null;
	private SwitchPreference mP2pSwitch = null;
	private MenuItem mActionClearStorage = null;

	private ServiceConnection mConnection = new ServiceConnection() {
		@SuppressLint("NewApi")
		public void onServiceConnected(ComponentName name, IBinder service) {
			Preferences.this.mService = ControlService.Stub.asInterface(service);
			
			if (Log.isLoggable(TAG, Log.DEBUG))
				Log.d(TAG, "service connected");

			// enable / disable P2P switch
			if (mP2pSwitch != null) {
				try {
					mP2pSwitch.setEnabled(Preferences.this.mService.isP2pSupported());
				} catch (RemoteException e) {
					mP2pSwitch.setEnabled(false);
				}
			}
		}

		public void onServiceDisconnected(ComponentName name) {
			if (Log.isLoggable(TAG, Log.DEBUG))
				Log.d(TAG, "service disconnected");
			mService = null;
		}
	};

	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		MenuInflater inflater = getMenuInflater();
		inflater.inflate(R.menu.main_menu, menu);
		mActionClearStorage = menu.findItem(R.id.itemClearStorage);
		
		if (Preferences.isDebuggable(this)) {
			mActionClearStorage.setVisible(true);
		} else {
			mActionClearStorage.setVisible(false);
		}
		
		return super.onCreateOptionsMenu(menu);
	}

	@Override
	public boolean onOptionsItemSelected(MenuItem item) {
		// Handle item selection
		switch (item.getItemId()) {
			case R.id.itemShowLog: {
				Intent i = new Intent(Preferences.this, LogActivity.class);
				startActivity(i);
				return true;
			}

			case R.id.itemClearStorage: {
				Intent i = new Intent(Preferences.this, DaemonService.class);
				i.setAction(DaemonService.ACTION_CLEAR_STORAGE);
				startService(i);
				return true;
			}

			case R.id.itemNeighbors: {
				// open neighbor list activity
				Intent i = new Intent(Preferences.this, NeighborActivity.class);
				startActivity(i);
				return true;
			}

			case R.id.itemStats: {
				// open statistic activity
				Intent i = new Intent(Preferences.this, StatsActivity.class);
				startActivity(i);
				return true;
			}
			
			case R.id.itemKeyPanel: {
				// open statistic activity
				Intent i = new Intent(Preferences.this, KeyInformationActivity.class);
				
				// create local singleton endpoint
				SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
				SingletonEndpoint node = new SingletonEndpoint(Preferences.getEndpoint(this, prefs));
				
				i.putExtra(KeyInformationActivity.EXTRA_IS_LOCAL, true);
				i.putExtra(de.tubs.ibr.dtn.Intent.EXTRA_ENDPOINT, (Parcelable)node);
				
				startActivity(i);
				return true;
			}

			default:
				return super.onOptionsItemSelected(item);
		}
	}

	public static void initializeDefaultPreferences(Context context) {
		SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(context);

		if (prefs.getBoolean("initialized", false))
			return;

		Editor e = prefs.edit();
		e.putString(KEY_ENDPOINT_ID, Preferences.getEndpoint(context, prefs).toString());
		
		// set preferences to initialized
		e.putBoolean("initialized", true);

		// commit changes
		e.commit();
		
		// create initial configuration
		createConfig(context);
	}
	
	public static boolean isDebuggable(Context context) {
		if (0 != (context.getApplicationInfo().flags & ApplicationInfo.FLAG_DEBUGGABLE)) {
			SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(context);
			return (prefs.getBoolean(KEY_DEBUG_MODE, false));
		}
		return false;
	}

	@TargetApi(14)
	@SuppressWarnings("deprecation")
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		// set default preference values
		PreferenceManager.setDefaultValues(this, R.xml.preferences, false);

		// initialize default values if configured set already
		initializeDefaultPreferences(this);
		
		// create configuration file
		createConfig(Preferences.this);

		// add standard preferences
		addPreferencesFromResource(R.xml.preferences);
		
		// add debugging preference in debuggable version
		if (0 != (getApplicationInfo().flags & ApplicationInfo.FLAG_DEBUGGABLE)) {
			addPreferencesFromResource(R.xml.preferences_debug);
		}

		mInterfacePreference = (InterfacePreferenceCategory) findPreference("prefcat_interfaces");

		// connect daemon controls
		checkBoxPreference = (CheckBoxPreference) findPreference(KEY_ENABLED);
		if (checkBoxPreference == null) {
			// use custom actionbar switch
			actionBarSwitch = new Switch(this);

			final int padding = this.getResources().getDimensionPixelSize(
					R.dimen.action_bar_switch_padding);
			actionBarSwitch.setPadding(0, 0, padding, 0);
			this.getActionBar().setDisplayOptions(ActionBar.DISPLAY_SHOW_CUSTOM,
					ActionBar.DISPLAY_SHOW_CUSTOM);
			this.getActionBar().setCustomView(actionBarSwitch, new ActionBar.LayoutParams(
					ActionBar.LayoutParams.WRAP_CONTENT,
					ActionBar.LayoutParams.WRAP_CONTENT,
					Gravity.CENTER_VERTICAL | Gravity.RIGHT));

			SharedPreferences prefs = PreferenceManager
					.getDefaultSharedPreferences(Preferences.this);

			// read initial state of the switch
			actionBarSwitch.setChecked(prefs.getBoolean(KEY_ENABLED, true));

			actionBarSwitch.setOnCheckedChangeListener(new OnCheckedChangeListener() {
				public void onCheckedChanged(CompoundButton arg0, boolean val) {

					if (val) {
						// set KEY_ENABLED preference to true
						SharedPreferences prefs = PreferenceManager
								.getDefaultSharedPreferences(Preferences.this);
						prefs.edit().putBoolean(KEY_ENABLED, true).commit();
					}
					else
					{
						// set KEY_ENABLED preference to false
						SharedPreferences prefs = PreferenceManager
								.getDefaultSharedPreferences(Preferences.this);
						prefs.edit().putBoolean(KEY_ENABLED, false).commit();
					}
				}
			});
		}

		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN) {
			// hide P2p control
			mP2pSwitch = (SwitchPreference)findPreference(KEY_P2P_ENABLED);
			mP2pSwitch.setEnabled(false);
		}

		// Bind the summaries of EditText/List/Dialog/Ringtone preferences to
		// their values. When their values change, their summaries are updated
		// to reflect the new value, per the Android Design guidelines.
		for (String prefKey : mSummaryPrefs) {
			bindPreferenceSummaryToValue(findPreference(prefKey));
		}
		
		// register to preference changes
		PreferenceManager.getDefaultSharedPreferences(this).registerOnSharedPreferenceChangeListener(mPrefListener);
	}

	@Override
	public void onDestroy() {
		// get daemon preferences
		SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(this);

		// un-listen to preference changes
		preferences.unregisterOnSharedPreferenceChangeListener(mPrefListener);
		
		if (mBound) {
			// Detach our existing connection.
			unbindService(mConnection);
			mBound = false;
		}

		super.onDestroy();
	}

	@Override
	protected void onPause() {
		super.onPause();

		unregisterReceiver(mNetworkConditionListener);
		unregisterReceiver(mP2pConditionListener);
	}

	@SuppressLint("NewApi")
	@Override
	protected void onResume() {
		if (!mBound) {
			// Establish a connection with the service. We use an explicit
			// class name because we want a specific service implementation that
			// we know will be running in our own process (and thus won't be
			// supporting component replacement by other applications).
	        Intent bindIntent = DaemonService.createControlServiceIntent(this);
	        bindService(bindIntent, mConnection, Context.BIND_AUTO_CREATE);
			mBound = true;
		}

		super.onResume();

		IntentFilter filter = new IntentFilter(ConnectivityManager.CONNECTIVITY_ACTION);
		filter.addAction(WifiManager.WIFI_STATE_CHANGED_ACTION);
		registerReceiver(mNetworkConditionListener, filter);
		
		IntentFilter p2p_filter = new IntentFilter(P2pManager.INTENT_P2P_STATE_CHANGED);
		registerReceiver(mP2pConditionListener, p2p_filter);
		
		// enable / disable P2P switch
		if ((mP2pSwitch != null) && (mService != null)) {
			try {
				mP2pSwitch.setEnabled(mService.isP2pSupported());
			} catch (RemoteException e) {
				mP2pSwitch.setEnabled(false);
			}
		}

		mInterfacePreference.updateInterfaceList();
	}

	private BroadcastReceiver mNetworkConditionListener = new BroadcastReceiver() {
		@Override
		public void onReceive(Context context, Intent intent) {
			runOnUiThread(new Runnable() {
				@Override
				public void run() {
					mInterfacePreference.updateInterfaceList();
				}
			});
		}
	};
	
	private BroadcastReceiver mP2pConditionListener = new BroadcastReceiver() {
		@SuppressLint("NewApi")
		@Override
		public void onReceive(Context context, Intent intent) {
			Log.d(TAG, "p2p changed");
			if (mP2pSwitch == null) return;
			
			// enable / disable P2P switch
			mP2pSwitch.setEnabled(intent.getBooleanExtra(P2pManager.EXTRA_P2P_STATE, false));
		}
	};

	/**
	 * A preference value change listener that updates the preference's summary
	 * to reflect its new value.
	 */
	private static Preference.OnPreferenceChangeListener sBindPreferenceSummaryToValueListener = new Preference.OnPreferenceChangeListener() {
		@Override
		public boolean onPreferenceChange(Preference preference, Object value) {
			boolean ret = true;
			String stringValue = value.toString();

			for (String prefKey : mSummaryPrefs) {
				if (prefKey.equals(preference.getKey())) {
					if (Preferences.KEY_ENDPOINT_ID.equals(prefKey)) {
						// exception for "endpoint_id", it shows the endpoint
						if ("dtn".equals( stringValue )) {
							preference.setSummary( getDefaultEndpoint(preference.getContext(), "dtn") );
						}
						else if ("ipn".equals( stringValue )) {
							preference.setSummary( getDefaultEndpoint(preference.getContext(), "ipn") );
						}
						else {
							preference.setSummary( stringValue );
						}
					} else if (preference instanceof ListPreference) {
						// For list preferences, look up the correct display
						// value in
						// the preference's 'entries' list.
						ListPreference listPreference = (ListPreference) preference;
						int index = listPreference.findIndexOfValue(stringValue);

						// Set the summary to reflect the new value.
						preference.setSummary(
								index >= 0
										? listPreference.getEntries()[index]
										: null);

					} else {
						// For all other preferences, set the summary to the
						// value's
						// simple string representation.
						preference.setSummary(stringValue);
					}
					return ret;
				}
			}

			return ret;
		}
	};

	/**
	 * Binds a preference's summary to its value. More specifically, when the
	 * preference's value is changed, its summary (line of text below the
	 * preference title) is updated to reflect the value. The summary is also
	 * immediately updated upon calling this method. The exact display format is
	 * dependent on the type of preference.
	 * 
	 * @see #sBindPreferenceSummaryToValueListener
	 */
	private static void bindPreferenceSummaryToValue(Preference preference) {
		// Set the listener to watch for value changes.
		preference.setOnPreferenceChangeListener(sBindPreferenceSummaryToValueListener);

		// Trigger the listener immediately with the preference's
		// current value.
		sBindPreferenceSummaryToValueListener.onPreferenceChange(preference,
				PreferenceManager
						.getDefaultSharedPreferences(preference.getContext())
						.getString(preference.getKey(), ""));
	}
	
	public static String getDefaultEndpoint(Context context, String scheme) {
		final String androidId = Secure.getString(context.getContentResolver(), Secure.ANDROID_ID);
		MessageDigest md;
		
		String dtnId;
		try {
			md = MessageDigest.getInstance("MD5");
			byte[] digest = md.digest(androidId.getBytes());
			dtnId = "android-" + toHex(digest).substring(4, 12);
		} catch (NoSuchAlgorithmException e) {
			Log.e(TAG, "md5 not available");
			dtnId = "android-" + androidId.substring(4, 12);
		}
		
		if ("ipn".equals(scheme)) {
			Long number = 0L;
			try {
				md = MessageDigest.getInstance("MD5");
				byte[] digest = md.digest(dtnId.getBytes());
				number += ByteBuffer.wrap(digest).getInt() & 0x7fffffff;
			} catch (NoSuchAlgorithmException e) {
				Log.e(TAG, "md5 not available");
				number += ByteBuffer.wrap(androidId.getBytes()).getInt() & 0x7fffffff;
			}
			number |= 0x80000000L;
			return "ipn:" + number;
		}
		else {
			return "dtn://" + dtnId + ".dtn";
		}
	}
	
	public static String getEndpoint(Context context, SharedPreferences prefs) {
		if (prefs.contains(KEY_ENDPOINT_ID)) {
			String endpointValue = prefs.getString(KEY_ENDPOINT_ID, "dtn");
			if ("dtn".equals( endpointValue )) {
				return getDefaultEndpoint(context, "dtn");
			}
			else if ("ipn".equals( endpointValue )) {
				return getDefaultEndpoint(context, "ipn");
			}
			else {
				return endpointValue;
			}
		}
		
		return getDefaultEndpoint(context, "dtn");
	}
	
	/**
	 * Create Hex String from byte array
	 * 
	 * @param data
	 * @return
	 */
	private static String toHex(byte[] data)
	{
		StringBuffer hexString = new StringBuffer();
		for (int i = 0; i < data.length; i++)
			hexString.append(Integer.toHexString(0xFF & data[i]));
		return hexString.toString();
	}
	
	private final static HashSet<String> mConfigurationSet = initializeConfigurationSet();

	private final static HashSet<String> initializeConfigurationSet() {
		HashSet<String> ret = new HashSet<String>();

		ret.add(KEY_SECURITY_MODE);
		ret.add(KEY_SECURITY_BAB_KEY);
		ret.add(KEY_LOG_OPTIONS);
		ret.add(KEY_LOG_DEBUG_VERBOSITY);
		ret.add(KEY_LOG_ENABLE_FILE);

		return ret;
	}
	
	private SharedPreferences.OnSharedPreferenceChangeListener mPrefListener = new SharedPreferences.OnSharedPreferenceChangeListener() {
		@TargetApi(Build.VERSION_CODES.ICE_CREAM_SANDWICH)
		@Override
		public void onSharedPreferenceChanged(SharedPreferences prefs, String key) {
			/**
			 * Update ON/OFF switch if changed any-where else
			 */
			if (KEY_ENABLED.equals(key)) {
				// update switch state
				if (checkBoxPreference == null) {
					actionBarSwitch.setChecked(prefs.getBoolean(KEY_ENABLED, true));
				} else {
					checkBoxPreference.setChecked(prefs.getBoolean(KEY_ENABLED, true));
				}
			}
			
			/**
			 * Forward preference change to the DTN service
			 */
			final Intent prefChangedIntent = new Intent(Preferences.this, DaemonService.class);
			prefChangedIntent.setAction(de.tubs.ibr.dtn.service.DaemonService.ACTION_PREFERENCE_CHANGED);
			prefChangedIntent.putExtra("prefkey", key);
			
			if (Preferences.KEY_ENABLED.equals(key))
				prefChangedIntent.putExtra(key, prefs.getBoolean(key, false));
			
			if (Preferences.KEY_P2P_ENABLED.equals(key))
				prefChangedIntent.putExtra(key, prefs.getBoolean(key, false));
			
			if (Preferences.KEY_DISCOVERY_MODE.equals(key))
				prefChangedIntent.putExtra(key, prefs.getString(key, "smart"));

			if (Preferences.KEY_BEACON_INTERVAL.equals(key))
				prefChangedIntent.putExtra(key, prefs.getString(key, "5"));
			
			if (Preferences.KEY_LOG_OPTIONS.equals(key))
				prefChangedIntent.putExtra(key, prefs.getString(key, "0"));
			
			if (Preferences.KEY_LOG_DEBUG_VERBOSITY.equals(key))
				prefChangedIntent.putExtra(key, prefs.getString(key, "0"));

			if (Preferences.KEY_LOG_ENABLE_FILE.equals(key))
				prefChangedIntent.putExtra(key, prefs.getBoolean(key, false));
			
			if (Preferences.KEY_UPLINK_MODE.equals(key))
				prefChangedIntent.putExtra(key, prefs.getString(key, "wifi"));
			
			if (Preferences.KEY_ENDPOINT_ID.equals(key))
				prefChangedIntent.putExtra(key, prefs.getString(key, "dtn"));
			
			Preferences.this.startService(prefChangedIntent);
			
			/**
			 * Alter DTN service according to configuration changes
			 */
			if (key.equals(Preferences.KEY_ENABLED))
			{
				Log.d(TAG, "Preference " + key + " has changed to " + String.valueOf(prefs.getBoolean(key, false)));

				if (prefs.getBoolean(key, false)) {
					// startup the daemon process
					final Intent intent = new Intent(Preferences.this, DaemonService.class);
					intent.setAction(de.tubs.ibr.dtn.service.DaemonService.ACTION_STARTUP);
					Preferences.this.startService(intent);
				} else {
					// shutdown the daemon
					final Intent intent = new Intent(Preferences.this, DaemonService.class);
					intent.setAction(de.tubs.ibr.dtn.service.DaemonService.ACTION_SHUTDOWN);
					Preferences.this.startService(intent);
				}
			}
			else if (key.startsWith(KEY_LOG_OPTIONS))
			{
				Log.d(TAG, "Preference " + key + " has changed to " + prefs.getString(key, "<not set>"));

				int logLevel = Integer.valueOf(prefs.getString(KEY_LOG_OPTIONS, "0"));
				int debugVerbosity = Integer.valueOf(prefs.getString(KEY_LOG_DEBUG_VERBOSITY, "0"));

				// disable debugging if the log level is lower than 3
				if (logLevel < 3)
					debugVerbosity = 0;
				
				// send configuration change to DTN service
				final Intent intent = new Intent(Preferences.this, DaemonService.class);
				intent.setAction(DaemonService.ACTION_LOGGING_CHANGED);
				intent.putExtra("loglevel", logLevel);
				intent.putExtra("debug", debugVerbosity);
				Preferences.this.startService(intent);
			}
			else if (key.startsWith(KEY_LOG_DEBUG_VERBOSITY))
			{
				Log.d(TAG,
						"Preference " + key + " has changed to "
								+ prefs.getString(key, "<not set>"));

				int logLevel = Integer.valueOf(prefs.getString(KEY_LOG_OPTIONS, "0"));
				int debugVerbosity = Integer.valueOf(prefs.getString(KEY_LOG_DEBUG_VERBOSITY, "0"));

				// disable debugging if the log level is lower than 3
				if (logLevel < 3)
					debugVerbosity = 0;
				
				// send configuration change to DTN service
				final Intent intent = new Intent(Preferences.this, DaemonService.class);
				intent.setAction(DaemonService.ACTION_LOGGING_CHANGED);
				intent.putExtra("debug", debugVerbosity);
				Preferences.this.startService(intent);
			}
			else if (key.startsWith(KEY_LOG_ENABLE_FILE))
			{
				Log.d(TAG, "Preference " + key + " has changed to " + prefs.getBoolean(key, false));

				// set logfile options
				String logFilePath = null;

				if (prefs.getBoolean(KEY_LOG_ENABLE_FILE, false)) {
					File logPath = DaemonStorageUtils.getLogPath(Preferences.this);
					if (logPath != null) {
						logPath.mkdirs();
						Date now = new Date();
						SimpleDateFormat df = new SimpleDateFormat("yyyyMMddhhmmss");
						String time = df.format(now);

						logFilePath = logPath.getPath() + File.separatorChar + "ibrdtn_" + time
								+ ".log";
					} else {
						Log.e(TAG, "External media for logging is not mounted");
					}
				}
				
				// send configuration change to DTN service
				final Intent intent = new Intent(Preferences.this, DaemonService.class);
				intent.setAction(DaemonService.ACTION_LOGGING_CHANGED);
				
				intent.putExtra("filelogging", logFilePath != null);
				
				if (logFilePath != null) {
					int logLevel = Integer.valueOf(prefs.getString(KEY_LOG_OPTIONS, "0"));
					intent.putExtra("loglevel", logLevel);
					intent.putExtra("logfile", logFilePath);
				}
				
				Preferences.this.startService(intent);
			}
			else if (Preferences.KEY_DISCOVERY_MODE.equals(key))
			{
				final String disco_mode = prefs.getString(key, "smart");

				// if discovery is configured as "on"
				if ("on".equals(disco_mode)) {
					// enable discovery
					final Intent discoIntent = new Intent(Preferences.this, DaemonService.class);
					discoIntent.setAction(de.tubs.ibr.dtn.service.DaemonService.ACTION_START_DISCOVERY);
					startService(discoIntent);
				}
				// if discovery is configured as "off"
				else if ("off".equals(disco_mode)) {
					// disable discovery
					final Intent discoIntent = new Intent(Preferences.this, DaemonService.class);
					discoIntent.setAction(de.tubs.ibr.dtn.service.DaemonService.ACTION_STOP_DISCOVERY);
					startService(discoIntent);
				}
				// if discovery is configured as "smart"
				else if ("smart".equals(disco_mode)) {
					// enable discovery for 2 minutes
					final Intent discoIntent = new Intent(Preferences.this, DaemonService.class);
					discoIntent.setAction(de.tubs.ibr.dtn.service.DaemonService.ACTION_START_DISCOVERY);
					discoIntent.putExtra(DaemonService.EXTRA_DISCOVERY_DURATION, 120L);
					startService(discoIntent);
				}
			}
			else if (Preferences.KEY_DEBUG_MODE.equals(key)) {
				if (mActionClearStorage != null) {
					if (Preferences.isDebuggable(Preferences.this)) {
						mActionClearStorage.setVisible(true);
					} else {
						mActionClearStorage.setVisible(false);
					}
				}
			}
			else if (mConfigurationSet.contains(key))
			{
				Log.d(TAG, "Preference " + key + " has changed");
				
				// create configuration file
				createConfig(Preferences.this);
				
				// send configuration change to DTN service
				final Intent intent = new Intent(Preferences.this, DaemonService.class);
				intent.setAction(DaemonService.ACTION_CONFIGURATION_CHANGED);
				Preferences.this.startService(intent);
			}
			else
			{
				// create configuration file
				createConfig(Preferences.this);
			}
		}
	};
	
	/**
	 * Creates config for dtnd in specified path
	 * 
	 * @param context
	 */
	private static void createConfig(Context context)
	{
		// determine path for the configuration file
		String configPath = DaemonStorageUtils.getConfigurationFile(context);
		
		// load preferences
		SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
		File config = new File(configPath);

		// remove old config file
		if (config.exists()) {
			config.delete();
		}

		try {
			FileOutputStream writer = context.openFileOutput("config", Context.MODE_PRIVATE);

			// set EID
			PrintStream p = new PrintStream(writer);
			p.println("local_uri = " + Preferences.getEndpoint(context, preferences));
			p.println("routing = " + preferences.getString(KEY_ROUTING, "default"));

			// enable traffic stats
			p.println("stats_traffic = yes");

			// limit max. bundle lifetime to 30 days
			p.println("limit_lifetime = 2592000");

			// limit pre-dated timestamp to 2 weeks
			p.println("limit_predated_timestamp = 1209600");

			p.println("discovery_interval = " + preferences.getString(KEY_BEACON_INTERVAL, "5"));

			// specify a security path for keys
			File sec_folder = DaemonStorageUtils.getSecurityPath(context);
			if (!sec_folder.exists() || sec_folder.isDirectory()) {
				p.println("security_path = " + sec_folder.getPath());
			}

			String secmode = preferences.getString(KEY_SECURITY_MODE, "disabled");

			if (secmode.equals("bab")) {
				// write default BAB key to file
				String bab_key = preferences.getString(KEY_SECURITY_BAB_KEY, "");
				File bab_file = new File(DaemonStorageUtils.getSecurityPath(context).getPath() + "/default-bab-key.mac");

				// remove old key file
				if (bab_file.exists())
					bab_file.delete();

				FileOutputStream bab_output = context.openFileOutput("default-bab-key.mac",
						Context.MODE_PRIVATE);
				PrintStream bab_writer = new PrintStream(bab_output);
				bab_writer.print(bab_key);
				bab_writer.flush();
				bab_writer.close();

				if (bab_key.length() > 0) {
					// enable security extension: BAB
					p.println("security_level = 1");

					// add BAB key to the configuration
					p.println("security_bab_default_key = " + bab_file.getPath());
				}
			}

			String timesyncmode = preferences.getString(KEY_TIMESYNC_MODE, "disabled");

			if (timesyncmode.equals("master")) {
				p.println("time_reference = yes");
				p.println("time_discovery_announcements = yes");
				p.println("time_synchronize = no");
				p.println("time_set_clock = no");
			} else if (timesyncmode.equals("slave")) {
				p.println("time_reference = no");
				p.println("time_discovery_announcements = yes");
				p.println("time_synchronize = yes");
				p.println("time_set_clock = no");
				p.println("#time_sigma = 1.001");
				p.println("#time_psi = 0.9");
				p.println("#time_sync_level = 0.15");
			}

			// enable fragmentation support
			p.println("fragmentation = yes");

			// set multicast address for discovery
			p.println("discovery_address = ff02::142 224.0.0.142");

			String ifaces = "";

			Map<String, ?> prefs = preferences.getAll();
			for (Map.Entry<String, ?> entry : prefs.entrySet()) {
				String key = entry.getKey();
				if (key.startsWith("interface_")) {
					if (entry.getValue() instanceof Boolean) {
						if ((Boolean) entry.getValue()) {
							String iface = key.substring(10, key.length());
							ifaces = ifaces + " " + iface;

							p.println("net_" + iface + "_type = tcp");
							p.println("net_" + iface + "_interface = " + iface);
							p.println("net_" + iface + "_port = 4556");
						}
					}
				}
			}

			p.println("net_interfaces = " + ifaces);

			// add static host for cloud uplink
			p.println("static1_address = " + __CLOUD_ADDRESS__);
			p.println("static1_port = " + __CLOUD_PORT__);
			p.println("static1_uri = " + __CLOUD_EID__);
			p.println("static1_proto = " + __CLOUD_PROTOCOL__);
			p.println("static1_immediately = yes");
			p.println("static1_global = yes");

			String storage_mode = preferences.getString(KEY_STORAGE_MODE, "disk-persistent");

			// storage path
			File blobPath = DaemonStorageUtils.getBlobPath(context);

			if ("disk".equals(storage_mode) || "disk-persistent".equals(storage_mode)) {
				if (blobPath != null) {
					p.println("blob_path = " + blobPath.getPath());
				} else {
					Log.e(TAG, "Internal cache directory is not available");
				}
			} else {
				// we want to store everything in memory => clear the blob path
				blobPath = null;
			}

			if (blobPath == null) {
				// if blocks are processed in memory limit these to 50 MB / 250 MB
				p.println("limit_blocksize = 250M");
				p.println("limit_foreign_blocksize = 50M");
			}

			if ("disk-persistent".equals(storage_mode)) {
				File bundlePath = DaemonStorageUtils.getStoragePath(context);
				if (bundlePath != null) {
					p.println("storage_path = " + bundlePath.getPath());
					p.println("use_persistent_bundlesets = yes");
				} else {
					Log.e(TAG, "External media to store bundles is not mounted");
				}
			}

			// enable interface rebind
			p.println("net_rebind = yes");
			
			// increase keep-alive timeout to 3 minutes
			p.println("keepalive_timeout = 180");

			// flush the write buffer
			p.flush();

			// close the filehandle
			writer.close();
		} catch (IOException e) {
			Log.e(TAG, "Problem writing config", e);
		}
	}
}
