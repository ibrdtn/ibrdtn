/*
 * DaemonService.java
 * 
 * Copyright (C) 2013 IBR, TU Braunschweig
 *
 * Written-by: Dominik Schürmann <dominik@dominikschuermann.de>
 * 	           Johannes Morgenroth <morgenroth@ibr.cs.tu-bs.de>
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

package de.tubs.ibr.dtn.service;

import java.util.ArrayList;
import java.util.Calendar;
import java.util.Date;
import java.util.List;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.PendingIntent;
import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.net.ConnectivityManager;
import android.net.wifi.WifiManager;
import android.os.Binder;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.Parcelable;
import android.os.PowerManager;
import android.os.RemoteException;
import android.preference.PreferenceManager;
import android.util.Log;
import android.widget.Toast;
import de.tubs.ibr.dtn.DTNService;
import de.tubs.ibr.dtn.DaemonState;
import de.tubs.ibr.dtn.R;
import de.tubs.ibr.dtn.SecurityService;
import de.tubs.ibr.dtn.SecurityUtils;
import de.tubs.ibr.dtn.Services;
import de.tubs.ibr.dtn.api.DTNSession;
import de.tubs.ibr.dtn.api.Node;
import de.tubs.ibr.dtn.api.Registration;
import de.tubs.ibr.dtn.api.SingletonEndpoint;
import de.tubs.ibr.dtn.daemon.Preferences;
import de.tubs.ibr.dtn.daemon.api.SelectNeighborActivity;
import de.tubs.ibr.dtn.keyexchange.KeyExchangeManager;
import de.tubs.ibr.dtn.keyexchange.KeyExchangeService;
import de.tubs.ibr.dtn.keyexchange.KeyInformationActivity;
import de.tubs.ibr.dtn.stats.ConvergenceLayerStatsEntry;
import de.tubs.ibr.dtn.stats.StatsContentProvider;
import de.tubs.ibr.dtn.stats.StatsEntry;
import de.tubs.ibr.dtn.swig.DaemonRunLevel;
import de.tubs.ibr.dtn.swig.NativeStats;
import de.tubs.ibr.dtn.swig.StringVec;

public class DaemonService extends Service {
	private static final String ACTION_INITIALIZE = "de.tubs.ibr.dtn.action.INITIALIZE";

	public static final String ACTION_STARTUP = "de.tubs.ibr.dtn.action.STARTUP";
	public static final String ACTION_SHUTDOWN = "de.tubs.ibr.dtn.action.SHUTDOWN";
	public static final String ACTION_PREFERENCE_CHANGED = "de.tubs.ibr.dtn.action.PREFERENCE_CHANGED";

	public static final String ACTION_NETWORK_CHANGED = "de.tubs.ibr.dtn.action.NETWORK_CHANGED";

	public static final String ACTION_UPDATE_NOTIFICATION = "de.tubs.ibr.dtn.action.UPDATE_NOTIFICATION";
	public static final String ACTION_INITIATE_CONNECTION = "de.tubs.ibr.dtn.action.INITIATE_CONNECTION";
	public static final String ACTION_CLEAR_STORAGE = "de.tubs.ibr.dtn.action.CLEAR_STORAGE";

	public static final String ACTION_STORE_STATS = "de.tubs.ibr.dtn.action.STORE_STATS";

	public static final String ACTION_START_DISCOVERY = "de.tubs.ibr.dtn.action.START_DISCOVERY";
	public static final String ACTION_STOP_DISCOVERY = "de.tubs.ibr.dtn.action.STOP_DISCOVERY";
	public static final String EXTRA_DISCOVERY_DURATION = "de.tubs.ibr.dtn.intent.DISCOVERY_DURATION";
	
	public static final String ACTION_START_KEY_EXCHANGE = "de.tubs.ibr.dtn.action.START_KEY_EXCHANGE";
	public static final String ACTION_GIVE_PASSWORD_RESPONSE = "de.tubs.ibr.dtn.action.GIVE_PASSWORD_RESPONSE";
	public static final String ACTION_GIVE_HASH_RESPONSE = "de.tubs.ibr.dtn.action.GIVE_HASH_RESPONSE";
	public static final String ACTION_GIVE_NEW_KEY_RESPONSE = "de.tubs.ibr.dtn.action.GIVE_NEW_KEY_RESPONSE";
	public static final String ACTION_GIVE_QR_RESPONSE = "de.tubs.ibr.dtn.action.GIVE_QR_RESPONSE";
	public static final String ACTION_GIVE_NFC_RESPONSE = "de.tubs.ibr.dtn.action.GIVE_NFC_RESPONSE";
	
	public static final String ACTION_LOGGING_CHANGED = "de.tubs.ibr.dtn.action.LOGGING_CHANGED";
	public static final String ACTION_CONFIGURATION_CHANGED = "de.tubs.ibr.dtn.action.CONFIGURATION_CHANGED";

	public static final String PREFERENCE_NAME = "de.tubs.ibr.dtn.service_prefs";
	
	public static final String ACTION_REMOVE_KEY = "de.tubs.ibr.dtn.action.ACTION_REMOVE_KEY";

	private final String TAG = "DaemonService";

	private volatile Looper mServiceLooper;
	private volatile ServiceHandler mServiceHandler;
	
	private int MSG_WHAT_STOP_DISCOVERY = 1;
	private int MSG_WHAT_COLLECT_STATS = 2;

	// session manager for all active sessions
	private SessionManager mSessionManager = null;

	// the P2P manager used for wifi direct control
	private P2pManager mP2pManager = null;
	
	// the BLE manager used for node discovery
	private BleManager mBleManager = null;

	// the daemon process
	private DaemonProcess mDaemonProcess = null;

	// time-stamp of the last stats action
	private Date mStatsLastAction = null;
	
	// global discovery state
	private Boolean mDiscoveryState = false;
	
	// Last inspected Wi-Fi SSID
	private String mDiscoverySsid = null;

	// This is the object that receives interactions from clients. See
	// RemoteService for a more complete example.
	private final DTNService.Stub mBinder = new LocalDTNService();

	private class LocalDTNService extends DTNService.Stub {
		@Override
		public DaemonState getState() throws RemoteException {
			return DaemonService.this.mDaemonProcess.getState();
		}

		@Override
		public List<Node> getNeighbors() throws RemoteException {
			return DaemonService.this.mDaemonProcess.getNeighbors();
		}

		@Override
		public DTNSession getSession(String sessionKey) throws RemoteException {
			int caller = Binder.getCallingUid();
			String[] packageNames = DaemonService.this.getPackageManager()
					.getPackagesForUid(caller);

			ClientSession cs = mSessionManager.getSession(packageNames, sessionKey);
			if (cs == null)
				return null;
			return cs.getBinder();
		}

		@Override
		public String[] getVersion() throws RemoteException {
			return DaemonService.this.mDaemonProcess.getVersion();
		}

		@Override
		public String getEndpoint() throws RemoteException {
			return Preferences.getEndpoint(DaemonService.this);
		}

		@Override
		public Bundle getSelectNeighborIntent() throws RemoteException {
			Bundle ret = new Bundle();
			Intent intent = new Intent(DaemonService.this, SelectNeighborActivity.class);
			PendingIntent pi = PendingIntent.getActivity(DaemonService.this, 0, intent, PendingIntent.FLAG_ONE_SHOT);
			ret.putParcelable(de.tubs.ibr.dtn.Intent.EXTRA_PENDING_INTENT, pi);
			return ret;
		}
	};
	
	private final ControlService.Stub mControlBinder = new ControlService.Stub() {
		@Override
		public boolean isP2pActive() throws RemoteException {
			return mP2pManager.isActive();
		}

		@Override
		public String[] getVersion() throws RemoteException {
			return DaemonService.this.mDaemonProcess.getVersion();
		}

		@Override
		public Bundle getStats() throws RemoteException {
			return DaemonService.this.getStats();
		}

		@Override
		public void setP2pEnabled(boolean val) throws RemoteException {
			if (val && mDaemonProcess.getState().equals(DaemonState.ONLINE)) {
				if (mP2pManager != null)
					mP2pManager.setEnabled(true);
			} else {
				if (mP2pManager != null)
					mP2pManager.setEnabled(false);
			}
		}
	};
	
	private final KeyExchangeManager.Stub mKeyExchangeBinder = new KeyExchangeManager.Stub() {
		@Override
		public SingletonEndpoint getEndpoint() throws RemoteException {
			return new SingletonEndpoint(Preferences.getEndpoint(DaemonService.this));
		}

		@Override
		public boolean hasKey(SingletonEndpoint endpoint) throws RemoteException {
			return (mDaemonProcess.getKeyInfo(endpoint) != null);
		}

		@Override
		public Bundle getKeyInfo(SingletonEndpoint endpoint) throws RemoteException {
			return mDaemonProcess.getKeyInfo(endpoint);
		}
	};

	private final SecurityService.Stub mSecurityBinder = new SecurityService.Stub() {
		@Override
		public Bundle execute(Intent intent) throws RemoteException {
			Bundle ret = new Bundle();
			
			if (SecurityUtils.ACTION_INFO_ACTIVITY.equals(intent.getAction())) {
				Intent infoIntent = new Intent(DaemonService.this, KeyInformationActivity.class);
				
				// extract endpoint from extras
				String endpoint = intent.getStringExtra(de.tubs.ibr.dtn.Intent.EXTRA_ENDPOINT);
				de.tubs.ibr.dtn.swig.EID node = null;
				
				// set default node, if not set
				if (endpoint == null) {
					// create local singleton endpoint
					node = new de.tubs.ibr.dtn.swig.EID(Preferences.getEndpoint(DaemonService.this));
					
					// mark the endpoint as local
					infoIntent.putExtra(KeyInformationActivity.EXTRA_IS_LOCAL, true);
				} else {
					// create singleton endpoint
					node = new de.tubs.ibr.dtn.swig.EID(endpoint);
				}
				
				// create new endpoint object using the return value from key-data
				SingletonEndpoint host = new SingletonEndpoint(node.getNode().getString());
				
				// put endpoint into extras of info intent
				infoIntent.putExtra(de.tubs.ibr.dtn.Intent.EXTRA_ENDPOINT, (Parcelable)host);
				
				PendingIntent pi = PendingIntent.getActivity(DaemonService.this, 0, infoIntent, PendingIntent.FLAG_ONE_SHOT | PendingIntent.FLAG_UPDATE_CURRENT);
				ret.putParcelable(de.tubs.ibr.dtn.Intent.EXTRA_PENDING_INTENT, pi);
			}
			else if (SecurityUtils.ACTION_INFO_DATA.equals(intent.getAction())) {
				// extract endpoint from extras
				String endpoint = intent.getStringExtra(de.tubs.ibr.dtn.Intent.EXTRA_ENDPOINT);
				SingletonEndpoint node = null;
				
				// set default node, if not set
				if (endpoint == null) {
					// create local singleton endpoint
					node = new SingletonEndpoint(Preferences.getEndpoint(DaemonService.this));
				} else {
					// create singleton endpoint
					node = new SingletonEndpoint(endpoint);
				}
				
				// get key data
				Bundle keydata = mDaemonProcess.getKeyInfo(node);
				
				// put all data into the bundle				
				if (keydata != null) {
					ret.putAll(keydata);
				}
			}

			return ret;
		}
	};

	@Override
	public IBinder onBind(Intent intent) {
		// the requested service name
		String name = intent.getStringExtra(Services.EXTRA_NAME);
		
		if (KeyExchangeManager.class.getName().equals(name)) {
			return mKeyExchangeBinder;
		} else if (ControlService.class.getName().equals(name)) {
			return mControlBinder;
		} else if (Services.SERVICE_SECURITY.match(intent)) {
			return mSecurityBinder;
		} else if (Services.SERVICE_APPLICATION.match(intent)) {
			return mBinder;
		} else {
			return null;
		}
	}
	
	public static Intent createDtnServiceIntent(Context context) {
		Intent i = new Intent(context, DaemonService.class);

		// set action to make the intent unique
		i.setAction(DTNService.class.getName());

		// add Service name
		i.putExtra(Services.EXTRA_NAME, DTNService.class.getName());

		// add Service version
		i.putExtra(Services.EXTRA_VERSION, Services.VERSION_APPLICATION);

		return i;
	}

	public static Intent createKeyExchangeManagerIntent(Context context) {
		Intent i = new Intent(context, DaemonService.class);

		// set action to make the intent unique
		i.setAction(KeyExchangeManager.class.getName());

		// add Service name
		i.putExtra(Services.EXTRA_NAME, KeyExchangeManager.class.getName());

		return i;
	}
	
	public static Intent createControlServiceIntent(Context context) {
		Intent i = new Intent(context, DaemonService.class);

		// set action to make the intent unique
		i.setAction(ControlService.class.getName());

		// add Service name
		i.putExtra(Services.EXTRA_NAME, ControlService.class.getName());

		return i;
	}

	@SuppressLint("HandlerLeak")
	private final class ServiceHandler extends Handler {
		public ServiceHandler(Looper looper) {
			super(looper);
		}

		@Override
		public void handleMessage(Message msg) {
			Intent intent = (Intent) msg.obj;
			onHandleIntent(intent, msg.arg1);
		}
	}

	/**
	 * Incoming Intents are handled here
	 * 
	 * @param intent
	 */
	@SuppressWarnings("deprecation")
	public void onHandleIntent(Intent intent, int startId) {
		String action = intent.getAction();

		if (ACTION_STARTUP.equals(action)) {
			SharedPreferences prefs = getSharedPreferences("dtnd", Context.MODE_PRIVATE);

			// only start if the daemon is offline or in error state
			// and if the daemon switch is on
			if ((mDaemonProcess.getState().equals(DaemonState.OFFLINE) || mDaemonProcess.getState()
					.equals(DaemonState.ERROR))
					&& prefs.getBoolean(Preferences.KEY_ENABLED, true)) {
				// start-up the daemon
				mDaemonProcess.start();

				final Intent storeStatsIntent = new Intent(this, DaemonService.class);
				storeStatsIntent
						.setAction(de.tubs.ibr.dtn.service.DaemonService.ACTION_STORE_STATS);
				startService(storeStatsIntent);
			}
		} else if (ACTION_SHUTDOWN.equals(action)) {
			// stop main loop
			mDaemonProcess.stop();
		} else if (ACTION_PREFERENCE_CHANGED.equals(action)) {
			// get preference key to determine the restart level
			String prefkey = intent.getStringExtra("prefkey");
			
			// store preference values locally
			SharedPreferences prefs = getSharedPreferences("dtnd", Context.MODE_PRIVATE);
			
			if (intent.hasExtra(Preferences.KEY_ENABLED)) {
				boolean value = intent.getBooleanExtra(Preferences.KEY_ENABLED, false);
				prefs.edit().putBoolean(Preferences.KEY_ENABLED, value).commit();
			}
			if (intent.hasExtra(Preferences.KEY_P2P_ENABLED)) {
				boolean value = intent.getBooleanExtra(Preferences.KEY_P2P_ENABLED, false);
				prefs.edit().putBoolean(Preferences.KEY_P2P_ENABLED, value).commit();
			}
			if (intent.hasExtra(Preferences.KEY_DISCOVERY_MODE)) {
				String value = intent.getStringExtra(Preferences.KEY_DISCOVERY_MODE);
				prefs.edit().putString(Preferences.KEY_DISCOVERY_MODE, value).commit();
			}
			if (intent.hasExtra(Preferences.KEY_LOG_OPTIONS)) {
				String value_s = intent.getStringExtra(Preferences.KEY_LOG_OPTIONS);
				int value = Integer.valueOf(value_s != null ? value_s : "0");
				prefs.edit().putInt(Preferences.KEY_LOG_OPTIONS, value).commit();
			}
			if (intent.hasExtra(Preferences.KEY_LOG_DEBUG_VERBOSITY)) {
				String value_s = intent.getStringExtra(Preferences.KEY_LOG_DEBUG_VERBOSITY);
				int value = Integer.valueOf(value_s != null ? value_s : "0");
				prefs.edit().putInt(Preferences.KEY_LOG_DEBUG_VERBOSITY, value).commit();
			}
			if (intent.hasExtra(Preferences.KEY_LOG_ENABLE_FILE)) {
				boolean value = intent.getBooleanExtra(Preferences.KEY_LOG_ENABLE_FILE, false);
				prefs.edit().putBoolean(Preferences.KEY_LOG_ENABLE_FILE, value).commit();
			}

			// restart the daemon into the given run-level
			mDaemonProcess.onPreferenceChanged(prefkey, new DaemonProcess.OnRestartListener() {
				@Override
				public void OnStop(DaemonRunLevel previous, DaemonRunLevel next) {
					if (next.swigValue() < DaemonRunLevel.RUNLEVEL_API.swigValue() && previous.swigValue() >= DaemonRunLevel.RUNLEVEL_API.swigValue()) {
						// shutdown the session manager
						mSessionManager.destroy();
					}
				}

				@Override
				public void OnStart(DaemonRunLevel previous, DaemonRunLevel next) {
					if (previous.swigValue() < DaemonRunLevel.RUNLEVEL_API.swigValue() && next.swigValue() >= DaemonRunLevel.RUNLEVEL_API.swigValue()) {
						// re-initialize the session manager
						mSessionManager.initialize();
					}
				}

				@Override
				public void OnReloadConfiguration() {
				}
			});

			final Intent storeStatsIntent = new Intent(this, DaemonService.class);
			storeStatsIntent.setAction(de.tubs.ibr.dtn.service.DaemonService.ACTION_STORE_STATS);
			startService(storeStatsIntent);
		} else if (de.tubs.ibr.dtn.Intent.REGISTER.equals(action)) {
			final Registration reg = (Registration) intent.getParcelableExtra("registration");
			final PendingIntent pi = (PendingIntent) intent.getParcelableExtra("app");

			mSessionManager.register(pi.getTargetPackage(), reg);

		} else if (de.tubs.ibr.dtn.Intent.UNREGISTER.equals(action)) {
			final PendingIntent pi = (PendingIntent) intent.getParcelableExtra("app");

			mSessionManager.unregister(pi.getTargetPackage());
		} else if (ACTION_INITIATE_CONNECTION.equals(action)) {
			if (intent.hasExtra("endpoint")) {
				mDaemonProcess.initiateConnection(intent.getStringExtra("endpoint"));
			}
		} else if (ACTION_CLEAR_STORAGE.equals(action)) {
			mDaemonProcess.clearStorage();
			Toast.makeText(this, R.string.toast_storage_cleared, Toast.LENGTH_SHORT).show();
		} else if (ACTION_NETWORK_CHANGED.equals(action)) {
			// This intent tickle the service if something has changed in the
			// network configuration. If the service was enabled but terminated
			// before,
			// it will be started now.

			final Intent storeStatsIntent = new Intent(this, DaemonService.class);
			storeStatsIntent.setAction(de.tubs.ibr.dtn.service.DaemonService.ACTION_STORE_STATS);
			startService(storeStatsIntent);

			SharedPreferences prefs = getSharedPreferences("dtnd", Context.MODE_PRIVATE);

			// if discovery is configured as "smart"
			if ("smart".equals(prefs.getString(Preferences.KEY_DISCOVERY_MODE, "smart"))) {
				// enable discovery for 2 minutes
				final Intent discoIntent = new Intent(DaemonService.this, DaemonService.class);
				discoIntent.setAction(de.tubs.ibr.dtn.service.DaemonService.ACTION_START_DISCOVERY);
				discoIntent.putExtra(EXTRA_DISCOVERY_DURATION, 120L);
				startService(discoIntent);
			}
		} else if (ACTION_STORE_STATS.equals(action)) {
			// cancel the next scheduled collection
			mServiceHandler.removeMessages(MSG_WHAT_COLLECT_STATS);
			
			Calendar now = Calendar.getInstance();
			now.roll(Calendar.MINUTE, -1);

			if (mStatsLastAction == null || mStatsLastAction.before(now.getTime())) {
				// store new stats in database
				Bundle stats = getStats();
				StatsContentProvider.store(DaemonService.this, stats);
				
				// store last stats action
				mStatsLastAction = new Date();
			}

			// schedule next collection in 15 minutes
			Message msg = mServiceHandler.obtainMessage();
			msg.what = MSG_WHAT_COLLECT_STATS;
			msg.arg1 = startId;
			msg.obj = new Intent(ACTION_STORE_STATS);
			mServiceHandler.sendMessageDelayed(msg, 900000);
		} else if (ACTION_INITIALIZE.equals(action)) {
			// initialize configuration
			Preferences.initializeDefaultPreferences(DaemonService.this);
			DaemonService.initializeDtndPreferences(DaemonService.this);

			// initialize the daemon service
			initialize();
		} else if (ACTION_START_DISCOVERY.equals(action)) {
			SharedPreferences prefs = getSharedPreferences("dtnd", Context.MODE_PRIVATE);
			String discoMode = prefs.getString(Preferences.KEY_DISCOVERY_MODE, "smart");
			boolean stayOn = "on".equals(discoMode) && mDiscoveryState;
			
			// create stop discovery intent
			Intent stopIntent = new Intent(this, DaemonService.class);
			stopIntent.setAction(DaemonService.ACTION_STOP_DISCOVERY);
			
			if (intent.hasExtra(EXTRA_DISCOVERY_DURATION) && !stayOn) {
				Long duration = intent.getLongExtra(EXTRA_DISCOVERY_DURATION, 120);
				
				Message msg = mServiceHandler.obtainMessage();
				msg.what = MSG_WHAT_STOP_DISCOVERY;
				msg.arg1 = startId;
				msg.obj = stopIntent;
				mServiceHandler.sendMessageDelayed(msg, duration * 1000);

				Log.i(TAG, "Discovery stop scheduled in " + duration + " seconds.");
			}
			else {
				mServiceHandler.removeMessages(MSG_WHAT_STOP_DISCOVERY);
				Log.i(TAG, "Scheduled discovery stop removed.");
			}

			// only start discovery if not active
			if (!mDiscoveryState) {
				// start P2P discovery and enable IPND
				mDaemonProcess.startDiscovery();
	
				// start Wi-Fi P2P discovery
				if (mP2pManager != null)
					mP2pManager.startDiscovery();
				
				// set global discovery state
				mDiscoveryState = true;
			}
		} else if (ACTION_STOP_DISCOVERY.equals(action)) {
			// only stop discovery if active
			if (mDiscoveryState) {
				// stop P2P discovery and disable IPND
				mDaemonProcess.stopDiscovery();
	
				// stop Wi-Fi P2P discovery
				if (mP2pManager != null)
					mP2pManager.stopDiscovery();
				
				// set global discovery state
				mDiscoveryState = false;
			}
			
			// remove all stop discovery messages
			mServiceHandler.removeMessages(MSG_WHAT_STOP_DISCOVERY);
			Log.i(TAG, "Scheduled discovery stop removed.");
		} else if (ACTION_START_KEY_EXCHANGE.equals(action)) {
			int protocol = intent.getIntExtra("protocol", -1);
			SingletonEndpoint endpoint = intent.getParcelableExtra(de.tubs.ibr.dtn.Intent.EXTRA_ENDPOINT);
			String password = intent.getStringExtra("password");
			
			if (password == null) {
				password = "";
			}
			
			mDaemonProcess.startKeyExchange(endpoint.toString(), protocol, password);
		} else if (ACTION_GIVE_PASSWORD_RESPONSE.equals(action)) {
			SingletonEndpoint endpoint = intent.getParcelableExtra(de.tubs.ibr.dtn.Intent.EXTRA_ENDPOINT);
			int session = intent.getIntExtra("session", -1);
			String password = intent.getStringExtra("password");
			
			if (password == null) {
				password = "";
			}
			
			mDaemonProcess.givePasswordResponse(endpoint.toString(), session, password);
		} else if (ACTION_GIVE_HASH_RESPONSE.equals(action)) {
			SingletonEndpoint endpoint = intent.getParcelableExtra(de.tubs.ibr.dtn.Intent.EXTRA_ENDPOINT);
			int session = intent.getIntExtra("session", -1);
			int equals = intent.getIntExtra("equals", -1);
			
			mDaemonProcess.giveHashResponse(endpoint.toString(), session, equals);
		} else if (ACTION_GIVE_NEW_KEY_RESPONSE.equals(action)) {
			SingletonEndpoint endpoint = intent.getParcelableExtra(de.tubs.ibr.dtn.Intent.EXTRA_ENDPOINT);
			int session = intent.getIntExtra("session", -1);
			int newKey = intent.getIntExtra("newKey", -1);
			
			mDaemonProcess.giveNewKeyResponse(endpoint.toString(), session, newKey);
		} else if (ACTION_GIVE_QR_RESPONSE.equals(action)) {
			SingletonEndpoint endpoint = intent.getParcelableExtra(de.tubs.ibr.dtn.Intent.EXTRA_ENDPOINT);
			String data = intent.getStringExtra(KeyExchangeService.EXTRA_DATA);
			
			mDaemonProcess.giveQRResponse(endpoint.toString(), data);
		} else if (ACTION_GIVE_NFC_RESPONSE.equals(action)) {
			SingletonEndpoint endpoint = intent.getParcelableExtra(de.tubs.ibr.dtn.Intent.EXTRA_ENDPOINT);
			String data = intent.getStringExtra(KeyExchangeService.EXTRA_DATA);
			
			mDaemonProcess.giveNFCResponse(endpoint.toString(), data);
		} else if (ACTION_REMOVE_KEY.equals(action)) {
			SingletonEndpoint endpoint = intent.getParcelableExtra(de.tubs.ibr.dtn.Intent.EXTRA_ENDPOINT);
			mDaemonProcess.removeKey(endpoint);
			
			// send intent to refresh views
			Intent broadcastIntent = new Intent(KeyExchangeService.INTENT_KEY_EXCHANGE);
			broadcastIntent.putExtra(KeyExchangeService.EXTRA_ACTION, KeyExchangeService.ACTION_KEY_UPDATED);
			broadcastIntent.putExtra(KeyExchangeService.EXTRA_ENDPOINT, endpoint.toString());
			sendOrderedBroadcast(broadcastIntent, null);
		} else if (ACTION_CONFIGURATION_CHANGED.equals(action)) {
			// signal a changed configuration to the daemon process
			mDaemonProcess.onConfigurationChanged();
		} else if (ACTION_LOGGING_CHANGED.equals(action)) {
			if (intent.hasExtra("filelogging")) {
				String logFilePath = intent.getStringExtra("logfile");
				int logLevel = intent.getIntExtra("loglevel", 0);
				
				// alter file logging
				mDaemonProcess.setLogFile(logFilePath == null ? "" : logFilePath, logLevel);
			} else {
				if (intent.hasExtra("loglevel")) {
					int logLevel = intent.getIntExtra("loglevel", 0);
					mDaemonProcess.setLogging("Core", logLevel);
				}
				
				if (intent.hasExtra("debug")) {
					int debugVerbosity = intent.getIntExtra("debug", 0);
					mDaemonProcess.setDebug(debugVerbosity);
				}
			}
		}

		// stop the daemon if it should be offline
		if (mDaemonProcess.getState().equals(DaemonState.OFFLINE) && (startId != -1))
			stopSelf(startId);
	}
	
	public Bundle getStats() {
		// retrieve stats of the native daemon
		NativeStats nstats = mDaemonProcess.getStats();
		
		// create a new bundle for statistics
		Bundle stats_bundle = new Bundle();
		
		// create parcable stats object
		stats_bundle.putParcelable("stats", new StatsEntry(nstats));
		
		// create an array for CL stats
		ArrayList<ConvergenceLayerStatsEntry> cl_stats = new ArrayList<ConvergenceLayerStatsEntry>();

		StringVec tags = nstats.getTags();
		for (int i = 0; i < tags.size(); i++) {
			ConvergenceLayerStatsEntry e = new ConvergenceLayerStatsEntry(nstats, tags.get(i), i);
			cl_stats.add(e);
		}
		
		// add CL stats to bundle
		stats_bundle.putParcelableArrayList("clstats", cl_stats);
		
		return stats_bundle;
	}

	public DaemonService() {
		super();
	}
	
	private static void initializeDtndPreferences(Context context) {
		SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(context);
		
		// initialize private preferences for dtnd
		SharedPreferences dtnd_prefs = context.getSharedPreferences("dtnd", Context.MODE_PRIVATE);
		
		// stop here, if these settings are already initialized
		if (dtnd_prefs.getBoolean("initialized", false)) return;
		
		Editor e = dtnd_prefs.edit();
		
		e.putBoolean(Preferences.KEY_ENABLED, prefs.getBoolean(Preferences.KEY_ENABLED, true));
		e.putBoolean(Preferences.KEY_P2P_ENABLED, prefs.getBoolean(Preferences.KEY_P2P_ENABLED, false));
		e.putString(Preferences.KEY_DISCOVERY_MODE, prefs.getString(Preferences.KEY_DISCOVERY_MODE, "smart"));
		
		int log_options = Integer.valueOf(prefs.getString(Preferences.KEY_LOG_OPTIONS, "0"));
		e.putInt(Preferences.KEY_LOG_OPTIONS, log_options);
		
		int debug_verbosity = Integer.valueOf(prefs.getString(Preferences.KEY_LOG_DEBUG_VERBOSITY, "0"));
		e.putInt(Preferences.KEY_LOG_DEBUG_VERBOSITY, debug_verbosity);

		e.putBoolean(Preferences.KEY_LOG_ENABLE_FILE, prefs.getBoolean(Preferences.KEY_LOG_ENABLE_FILE, false));
		
		// set preferences to initialized
		e.putBoolean("initialized", true);
		
		e.commit();
	}

	@Override
	public void onCreate() {
		super.onCreate();
		
		// reset last SSID inspected for discovery
		mDiscoverySsid = null;

		// reset statistic action marker
		mStatsLastAction = null;

		// create daemon main thread
		mDaemonProcess = new DaemonProcess(this, mProcessHandler);

		/*
		 * incoming Intents will be processed by ServiceHandler and queued in
		 * HandlerThread
		 */
		HandlerThread thread = new HandlerThread("DaemonService_IntentThread");
		thread.start();
		mServiceLooper = thread.getLooper();
		mServiceHandler = new ServiceHandler(mServiceLooper);

		// create a session manager
		mSessionManager = new SessionManager(this);

		// create P2P Manager
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN) {
			mP2pManager = new P2pManager(this);
			mP2pManager.create();
		}
		
		// create BLE Manager
				if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2) {
					mBleManager = new BleManager(this);
				}

		// start initialization of the daemon process
		final Intent intent = new Intent(this, DaemonService.class);
		intent.setAction(de.tubs.ibr.dtn.service.DaemonService.ACTION_INITIALIZE);

		// queue the initialization job as the first job of the handler
		Message msg = mServiceHandler.obtainMessage();
		msg.arg1 = -1; // invalid startId (this never leads to a stop of the
						// service)
		msg.obj = intent;
		mServiceHandler.sendMessage(msg);
	}

	/**
	 * Initialize the daemon service This should be the first job of the service
	 * after creation
	 */
	private void initialize() {
		// initialize the basic daemon
		mDaemonProcess.initialize();

		// restore registrations
		mSessionManager.initialize();

		if (Log.isLoggable(TAG, Log.DEBUG))
			Log.d(TAG, "DaemonService created");

		// start daemon if enabled
		SharedPreferences prefs = getSharedPreferences("dtnd", Context.MODE_PRIVATE);

		if (prefs.getBoolean(Preferences.KEY_ENABLED, true)) {
			// startup the daemon process
			final Intent intent = new Intent(this, DaemonService.class);
			intent.setAction(de.tubs.ibr.dtn.service.DaemonService.ACTION_STARTUP);
			startService(intent);
		}
	}

	/**
	 * Called on stopSelf() or stopService()
	 */
	@Override
	public void onDestroy() {
		// disable P2P manager
		if (mP2pManager != null)
			mP2pManager.destroy();

		// stop looper that handles incoming intents
		mServiceLooper.quit();

		// close all sessions
		mSessionManager.destroy();

		// shutdown daemon completely
		mDaemonProcess.destroy();
		mDaemonProcess = null;

		// dereference P2P Manager
		mP2pManager = null;

		// call super method
		super.onDestroy();
	}

	@Override
	public void onStart(Intent intent, int startId) {
		/*
		 * If no explicit intent is given start as ACTION_STARTUP. When this
		 * service crashes, Android restarts it without an Intent. Thus
		 * ACTION_STARTUP is executed!
		 */
		if (intent == null || intent.getAction() == null) {
			Log.d(TAG, "intent == null or intent.getAction() == null -> default to ACTION_STARTUP");

			intent = new Intent(ACTION_STARTUP);
		}

		String action = intent.getAction();

		if (Log.isLoggable(TAG, Log.DEBUG))
			Log.d(TAG, "Received start id " + startId + ": " + intent);
		if (Log.isLoggable(TAG, Log.DEBUG))
			Log.d(TAG, "Intent Action: " + action);

		Message msg = mServiceHandler.obtainMessage();
		msg.arg1 = startId;
		msg.obj = intent;
		mServiceHandler.sendMessage(msg);
	}

	@Override
	public int onStartCommand(Intent intent, int flags, int startId) {
		onStart(intent, startId);
		return START_STICKY;
	}

	private DaemonProcessHandler mProcessHandler = new DaemonProcessHandler() {

		@TargetApi(Build.VERSION_CODES.HONEYCOMB_MR1)
		@Override
		public void onStateChanged(DaemonState state) {
			Log.d(TAG, "DaemonState: " + state);

			// prepare broadcast intent
			Intent broadcastIntent = new Intent();
			broadcastIntent.setAction(de.tubs.ibr.dtn.Intent.STATE);
			broadcastIntent.putExtra("state", state.name());
			broadcastIntent.addCategory(Intent.CATEGORY_DEFAULT);

			SharedPreferences prefs = getSharedPreferences("dtnd", Context.MODE_PRIVATE);

			switch (state) {
				case ERROR:
					break;

				case OFFLINE:
					if (prefs.getBoolean(Preferences.KEY_P2P_ENABLED, true)) {
						if (mP2pManager != null)
							mP2pManager.setEnabled(false);
					}
					
					// unlisten to device state events
					unregisterReceiver(mScreenStateReceiver);
					
					// unlisten to Wi-Fi events
					unregisterReceiver(mNetworkStateReceiver);

					// disable foreground service only if the daemon has been
					// switched off
					if (!prefs.getBoolean(Preferences.KEY_ENABLED, true)) {
						// stop service
						stopSelf();
					}

					// if discovery is configured as some kind of active
					if (!"off".equals(prefs.getString(Preferences.KEY_DISCOVERY_MODE, "smart"))) {
						// disable discovery
						final Intent discoIntent = new Intent(DaemonService.this, DaemonService.class);
						discoIntent.setAction(de.tubs.ibr.dtn.service.DaemonService.ACTION_STOP_DISCOVERY);
						startService(discoIntent);
					}

					break;

				case ONLINE:
					if (prefs.getBoolean(Preferences.KEY_P2P_ENABLED, false)) {
						if (mP2pManager != null)
							mP2pManager.setEnabled(true);
					}
					
					// listen to Wi-Fi events
					IntentFilter netstatefilter = new IntentFilter();
					netstatefilter.addAction(ConnectivityManager.CONNECTIVITY_ACTION);
					netstatefilter.addAction(WifiManager.WIFI_STATE_CHANGED_ACTION);
					registerReceiver(mNetworkStateReceiver, netstatefilter);

					if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB_MR1) {
						// wake-up all apps in stopped-state when going online
						broadcastIntent.addFlags(Intent.FLAG_INCLUDE_STOPPED_PACKAGES);
					}

					// if discovery is configured as "smart"
					if ("smart".equals(prefs.getString(Preferences.KEY_DISCOVERY_MODE, "smart"))) {
						// enable discovery for 2 minutes
						final Intent discoIntent = new Intent(DaemonService.this,
								DaemonService.class);
						discoIntent
								.setAction(de.tubs.ibr.dtn.service.DaemonService.ACTION_START_DISCOVERY);
						discoIntent.putExtra(EXTRA_DISCOVERY_DURATION, 120L);
						startService(discoIntent);
					}
					// if discovery is configured as "on"
					else if ("on".equals(prefs.getString(Preferences.KEY_DISCOVERY_MODE, "smart"))) {
						// enable discovery
						final Intent discoIntent = new Intent(DaemonService.this,
								DaemonService.class);
						discoIntent
								.setAction(de.tubs.ibr.dtn.service.DaemonService.ACTION_START_DISCOVERY);
						startService(discoIntent);
					}
					
					// react to screen on/off events
					IntentFilter dsfilter = new IntentFilter();
					dsfilter.addAction(Intent.ACTION_SCREEN_ON);
					dsfilter.addAction(Intent.ACTION_SCREEN_OFF);
					registerReceiver(mScreenStateReceiver, dsfilter);
					
					// set initial state
					PowerManager pm = (PowerManager)getSystemService(Context.POWER_SERVICE);
					mDaemonProcess.setLeMode(!pm.isScreenOn());
							
					break;

				case SUSPENDED:
					break;
				case UNKOWN:
					break;
				default:
					break;

			}

			// broadcast state change
			sendBroadcast(broadcastIntent);
		}

		@Override
		public void onNeighborhoodChanged() {
			// nothing to do.
		}

		@Override
		public void onEvent(Intent intent) {
			if (KeyExchangeService.INTENT_KEY_EXCHANGE.equals(intent.getAction())) {
				sendOrderedBroadcast(intent, null);
			}
			else {
				sendBroadcast(intent);
			}
		}

		private BroadcastReceiver mNetworkStateReceiver = new BroadcastReceiver() {
			@Override
			public void onReceive(final Context context, final Intent intent) {
				final ConnectivityManager connMgr = (ConnectivityManager) 
						context.getSystemService(Context.CONNECTIVITY_SERVICE);

				final android.net.NetworkInfo wifi = 
						connMgr.getNetworkInfo(ConnectivityManager.TYPE_WIFI);

				// extract SSID
				String ssid = wifi.getExtraInfo();
				
				// forward to the daemon if it is enabled
				if (wifi.isConnected() && ssid != null) {
					if (!ssid.equals(mDiscoverySsid)) {
						// store SSID of last discovery
						mDiscoverySsid = ssid;
	
						// tickle the daemon service
						final Intent wakeUpIntent = new Intent(context, DaemonService.class);
						wakeUpIntent.setAction(de.tubs.ibr.dtn.service.DaemonService.ACTION_NETWORK_CHANGED);
						context.startService(wakeUpIntent);
					}
				} else {
					mDiscoverySsid = null;
				}
			}
		};
		
		private BroadcastReceiver mScreenStateReceiver = new BroadcastReceiver() {
			@Override
			public void onReceive(Context context, Intent intent) {
				PowerManager pm = (PowerManager)getSystemService(Context.POWER_SERVICE);
				mDaemonProcess.setLeMode(!pm.isScreenOn());
			}
		};
	};
}
