/*
 * DaemonMainThread.java
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

import java.io.File;
import java.util.Calendar;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.util.Log;
import de.tubs.ibr.dtn.DaemonState;
import de.tubs.ibr.dtn.api.Node;
import de.tubs.ibr.dtn.api.SingletonEndpoint;
import de.tubs.ibr.dtn.daemon.Preferences;
import de.tubs.ibr.dtn.keyexchange.KeyExchangeService;
import de.tubs.ibr.dtn.swig.DaemonRunLevel;
import de.tubs.ibr.dtn.swig.NativeDaemon;
import de.tubs.ibr.dtn.swig.NativeDaemonCallback;
import de.tubs.ibr.dtn.swig.NativeDaemonException;
import de.tubs.ibr.dtn.swig.NativeEventCallback;
import de.tubs.ibr.dtn.swig.NativeKeyInfo;
import de.tubs.ibr.dtn.swig.NativeNode;
import de.tubs.ibr.dtn.swig.NativeStats;
import de.tubs.ibr.dtn.swig.StringVec;

public class DaemonProcess {
	private final static String TAG = "DaemonProcess";

	private NativeDaemon mDaemon = null;
	private DaemonProcessHandler mHandler = null;
	private Context mContext = null;
	private DaemonState mState = DaemonState.OFFLINE;
	private Boolean mDiscoveryEnabled = null;
	private Boolean mDiscoveryActive = null;
	private Boolean mLeModeEnabled = false;
	private boolean mGlobalConnected = false;
	
	private WifiManager.MulticastLock mMcastLock = null;

	private final static String CPPLIB_NAME = "c++_shared";
	private final static String CRYPTO_NAME = "cryptox";
	private final static String SSL_NAME = "ssl";
	private final static String IBRCOMMON_NAME = "ibrcommon";
	private final static String IBRDTN_NAME = "ibrdtn";
	private final static String DTND_NAME = "dtnd";
	private final static String ANDROID_GLUE_NAME = "android-glue";

    public interface OnRestartListener {
        public void OnStop(DaemonRunLevel previous, DaemonRunLevel next);
        public void OnReloadConfiguration();
        public void OnStart(DaemonRunLevel previous, DaemonRunLevel next);
    };

	/**
	 * Loads all shared libraries in the right order with System.loadLibrary()
	 */
	private static void loadLibraries()
	{
		try
		{
			System.loadLibrary(CPPLIB_NAME);

			System.loadLibrary(CRYPTO_NAME);
			System.loadLibrary(SSL_NAME);

			System.loadLibrary(IBRCOMMON_NAME);
			System.loadLibrary(IBRDTN_NAME);
			System.loadLibrary(DTND_NAME);

			System.loadLibrary(ANDROID_GLUE_NAME);
		} catch (UnsatisfiedLinkError e)
		{
			Log.e(TAG, "UnsatisfiedLinkError! Are you running special hardware?", e);
		} catch (Exception e)
		{
			Log.e(TAG, "Loading the libraries failed!", e);
		}
	}
	
	static
	{
		// load libraries on first use of this class
		loadLibraries();
	}

	public DaemonProcess(Context context, DaemonProcessHandler handler) {
		this.mDaemon = new NativeDaemon(mDaemonCallback, mEventCallback);
		this.mContext = context;
		this.mHandler = handler;
		this.mDiscoveryEnabled = false;
		this.mDiscoveryActive = true;
	}
	
	public String[] getVersion() {
        StringVec version = mDaemon.getVersion();
        return new String[] { version.get(0), version.get(1) };
	}
	
	public synchronized NativeStats getStats() {
	    return mDaemon.getStats();
	}
	
	public synchronized List<Node> getNeighbors() {
        List<Node> ret = new LinkedList<Node>();
        StringVec neighbors = mDaemon.getNeighbors();
        for (int i = 0; i < neighbors.size(); i++) {
        	String eid = neighbors.get(i);
        	
        	try {
            	// get extended info
				NativeNode nn = mDaemon.getInfo(eid);
				
            	Node n = new Node();
            	n.endpoint = new SingletonEndpoint(eid);
            	n.type = nn.getType().toString();
                ret.add(n);
			} catch (NativeDaemonException e) { }
        }

        return ret;
	}
	
	public synchronized void clearStorage() {
		mDaemon.clearStorage();
	}
	
	public DaemonState getState() {
	    return mState;
	}
	
    private void setState(DaemonState newState) {
        if (mState.equals(newState)) return;
        mState = newState;
        mHandler.onStateChanged(mState);
    }
    
    public synchronized void initiateConnection(String endpoint) {
    	if (getState().equals(DaemonState.ONLINE)) {
    		mDaemon.initiateConnection(endpoint);
    	}
    }
    
    public synchronized void startKeyExchange(String eid, int protocol, String password) {
    	mDaemon.onKeyExchangeBegin(eid, protocol, password);
    }
    
    public synchronized void givePasswordResponse(String eid, int session, String password) {
    	mDaemon.onKeyExchangeResponse(eid, 2, session, 0, "");
    }
    
    public synchronized void giveHashResponse(String eid, int session, int equals) {
    	mDaemon.onKeyExchangeResponse(eid, 100, session, equals, "");
    }
    
    public synchronized void giveNewKeyResponse(String eid, int session, int newKey) {
    	mDaemon.onKeyExchangeResponse(eid, 101, session, newKey, "");
    }
    
    public synchronized void giveQRResponse(String eid, String data) {
    	mDaemon.onKeyExchangeBegin(eid, 4, data);
    }
    
    public synchronized void giveNFCResponse(String eid, String data) {
    	mDaemon.onKeyExchangeBegin(eid, 5, data);
    }
    
    public synchronized void removeKey(SingletonEndpoint endpoint) {
    	try {
			mDaemon.removeKey(endpoint.toString());
		} catch (NativeDaemonException e) {
			Log.e(TAG, "", e);
		}
    }
    
    public synchronized Bundle getKeyInfo(SingletonEndpoint endpoint) {
		try {
			NativeKeyInfo info = mDaemon.getKeyInfo(endpoint.toString());
			Bundle ret = new Bundle();
			ret.putString(KeyExchangeService.EXTRA_ENDPOINT, info.getEndpoint());
			ret.putString(KeyExchangeService.EXTRA_FINGERPRINT, info.getFingerprint());
			ret.putString(KeyExchangeService.EXTRA_DATA, info.getData());
			ret.putLong(KeyExchangeService.EXTRA_FLAGS, info.getFlags());
			
    		long flags = info.getFlags();
    		int trustlevel = 0;
    		
    		boolean pNone = (flags & 0x01) > 0;
    		boolean pDh = (flags & 0x02) > 0;
    		boolean pJpake = (flags & 0x04) > 0;
    		boolean pHash = (flags & 0x08) > 0;
    		boolean pQrCode = (flags & 0x10) > 0;
    		boolean pNfc = (flags & 0x20) > 0;
    		
    		if (pNfc || pQrCode) {
    			trustlevel = 100;
    		}
    		else if (pHash || pJpake) {
    			trustlevel = 60;
    		}
    		else if (pDh) {
    			trustlevel = 10;
    		}
    		else if (pNone) {
    			trustlevel = 1;
    		}
			
			ret.putInt("trustlevel", trustlevel);
			return ret;
		} catch (NativeDaemonException e) {
			return null;
		}
    }
    
    public synchronized void initialize() {
    	// lower the thread priority
    	android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_BACKGROUND);
    	
    	// clear blob path
    	DaemonStorageUtils.clearBlobPath(mContext);
    	
    	// get daemon preferences
        SharedPreferences prefs = mContext.getSharedPreferences("dtnd", Context.MODE_PRIVATE);
        
        // enable debug based on prefs
        int logLevel = 0;
        try {
            logLevel = Integer.valueOf(prefs.getInt(Preferences.KEY_LOG_OPTIONS, 0));
        } catch (java.lang.NumberFormatException e) {
            // invalid number
        }
        
        int debugVerbosity = 0;
        try {
            debugVerbosity = Integer.valueOf(prefs.getInt(Preferences.KEY_LOG_DEBUG_VERBOSITY, 0));
        } catch (java.lang.NumberFormatException e) {
            // invalid number
        }
        
        // disable debugging if the log level is lower than 3
        if (logLevel < 3) debugVerbosity = 0;
        
        // set logging options
        mDaemon.setLogging("Core", logLevel);

        // set logfile options
        String logFilePath = null;
        
        if (prefs.getBoolean(Preferences.KEY_LOG_ENABLE_FILE, false)) {
            File logPath = DaemonStorageUtils.getLogPath(mContext);
            if (logPath != null) {
                logPath.mkdirs();
                Calendar cal = Calendar.getInstance();
                String time = "" + cal.get(Calendar.YEAR) + cal.get(Calendar.MONTH) + cal.get(Calendar.DAY_OF_MONTH) + cal.get(Calendar.DAY_OF_MONTH)
                        + cal.get(Calendar.HOUR) + cal.get(Calendar.MINUTE) + cal.get(Calendar.SECOND);
                
                logFilePath = logPath.getPath() + File.separatorChar + "ibrdtn_" + time + ".log";
            } else {
                Log.e(TAG, "External media for logging is not mounted");
            }
        }

        if (logFilePath != null) {
            // enable file logging
            mDaemon.setLogFile(logFilePath, logLevel);
        } else {
            // disable file logging
            mDaemon.setLogFile("", 0);
        }

        // set debug verbosity
        mDaemon.setDebug(debugVerbosity);
        
        // initialize daemon configuration
        onConfigurationChanged();
        
        try {
            mDaemon.init(DaemonRunLevel.RUNLEVEL_API);
        } catch (NativeDaemonException e) {
            Log.e(TAG, "error while initializing the daemon process", e);
        }
    }
	
	public synchronized void start() {
        // reload daemon configuration
        onConfigurationChanged();
        
	    try {
            mDaemon.init(DaemonRunLevel.RUNLEVEL_ROUTING_EXTENSIONS);
        } catch (NativeDaemonException e) {
            Log.e(TAG, "error while starting the daemon process", e);
        }
	}
	
	public synchronized void stop() {
	    // stop the running daemon
	    try {
            mDaemon.init(DaemonRunLevel.RUNLEVEL_API);
        } catch (NativeDaemonException e) {
            Log.e(TAG, "error while stopping the daemon process", e);
        }
	}
	
	public synchronized void onPreferenceChanged(String prefkey, OnRestartListener listener) {
		if (prefkey.startsWith("interface_")) prefkey = "interface_";
		if (!mRestartMap.containsKey(prefkey)) return;
		
		int runlevel = mRestartMap.get(prefkey).swigValue() - 1;
		restart(runlevel, listener);
	}
	
	public synchronized void restart(Integer runlevel, OnRestartListener listener) {
	    // restart the daemon
        DaemonRunLevel restore = mDaemon.getRunLevel();
        DaemonRunLevel rl = DaemonRunLevel.swigToEnum(runlevel);
        
        // do not restart if the current runlevel is below or equal
        if (restore.swigValue() <= runlevel) {
            // reload configuration
            onConfigurationChanged();
            if (listener != null) listener.OnReloadConfiguration();
            return;
        }
        
	    try {
	        // bring the daemon down
	        if (listener != null) listener.OnStop(restore, rl);
	        mDaemon.init(rl);
	        
	        // reload configuration
	        onConfigurationChanged();
	        if (listener != null) listener.OnReloadConfiguration();
	        
	        // restore the old runlevel
	        mDaemon.init(restore);
	        if (listener != null) listener.OnStart(rl, restore);
	    } catch (NativeDaemonException e) {
            Log.e(TAG, "error while restarting the daemon process", e);
        }
	}
	
    public synchronized void destroy() {
        // stop the running daemon
        try {
            mDaemon.init(DaemonRunLevel.RUNLEVEL_ZERO);
        } catch (NativeDaemonException e) {
            Log.e(TAG, "error while destroying the daemon process", e);
        }
    }
    
	private final static HashMap<String, DaemonRunLevel> mRestartMap = initializeRestartMap();

	private final static HashMap<String, DaemonRunLevel> initializeRestartMap() {
		HashMap<String, DaemonRunLevel> ret = new HashMap<String, DaemonRunLevel>();

		ret.put(Preferences.KEY_ENDPOINT_ID, DaemonRunLevel.RUNLEVEL_CORE);
		ret.put(Preferences.KEY_ROUTING, DaemonRunLevel.RUNLEVEL_ROUTING_EXTENSIONS);
		ret.put("interface_", DaemonRunLevel.RUNLEVEL_NETWORK);
		ret.put(Preferences.KEY_TIMESYNC_MODE, DaemonRunLevel.RUNLEVEL_API);
		ret.put(Preferences.KEY_STORAGE_MODE, DaemonRunLevel.RUNLEVEL_CORE);
		ret.put(Preferences.KEY_UPLINK_MODE, DaemonRunLevel.RUNLEVEL_NETWORK);

		return ret;
	}
    
    public synchronized void setLogFile(String path, int logLevel) {
    	mDaemon.setLogFile(path, logLevel);
    }
    
    public synchronized void setLogging(String defaultTag, int logLevel) {
    	mDaemon.setLogging(defaultTag, logLevel);
    }
    
    public synchronized void setDebug(int level) {
    	mDaemon.setDebug(level);
    }
    
	public synchronized void onConfigurationChanged() {
        String configPath = DaemonStorageUtils.getConfigurationFile(mContext);
        
        // set configuration file
        mDaemon.setConfigFile(configPath);
	}
	
	private NativeEventCallback mEventCallback = new NativeEventCallback() {
        @Override
        public void eventRaised(String eventName, String action, StringVec data) {
            Intent event = new Intent(de.tubs.ibr.dtn.Intent.EVENT);
            Intent neighborIntent = null;
            Intent keyExchangeIntent = null;

            event.addCategory(Intent.CATEGORY_DEFAULT);
            event.putExtra("name", eventName);

            if ("NodeEvent".equals(eventName)) {
                neighborIntent = new Intent(de.tubs.ibr.dtn.Intent.NEIGHBOR);
                neighborIntent.addCategory(Intent.CATEGORY_DEFAULT);
            }
            else if ("KeyExchangeEvent".equals(eventName)) {
            	keyExchangeIntent = new Intent(KeyExchangeService.INTENT_KEY_EXCHANGE);
            	keyExchangeIntent.addCategory(Intent.CATEGORY_DEFAULT);
            }

            // place the action into the intent
            if (action.length() > 0)
            {
                event.putExtra("action", action);
                
                if (neighborIntent != null) {
                	neighborIntent.putExtra("action", action);
                }
                else if (keyExchangeIntent != null) {
                	keyExchangeIntent.putExtra("action", action);
                }
            }

            // put all attributes into the intent
            for (int i = 0; i < data.size(); i++) {
                String entry = data.get(i);
                String entry_data[] = entry.split(": ", 2);
                
                // skip invalid entries
                if (entry_data.length < 2) continue;

                event.putExtra("attr:" + entry_data[0], entry_data[1]);
                if (neighborIntent != null) {
                    neighborIntent.putExtra("attr:" + entry_data[0], entry_data[1]);
                }
                else if (keyExchangeIntent != null) {
                	keyExchangeIntent.putExtra(entry_data[0], entry_data[1]);
                }
            }

            // send event intent
            mHandler.onEvent(event);

            if (neighborIntent != null) {
                mHandler.onEvent(neighborIntent);
                mHandler.onNeighborhoodChanged();
            }
            else if (keyExchangeIntent != null) {
            	mHandler.onEvent(keyExchangeIntent);
            }

            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "EVENT intent broadcasted: " + eventName + "; Action: " + action);
            }
        }
	};
	
	private NativeDaemonCallback mDaemonCallback = new NativeDaemonCallback() {
		@Override
		public void levelChanged(DaemonRunLevel level) {
			if (DaemonRunLevel.RUNLEVEL_ROUTING_EXTENSIONS.equals(level)) {
			    setState(DaemonState.ONLINE);
			}
			else if (DaemonRunLevel.RUNLEVEL_API.equals(level)) {
			    setState(DaemonState.OFFLINE);
			}
			else if (DaemonRunLevel.RUNLEVEL_NETWORK.equals(level)) {
			    // restore previous discovery state
			    if (mDiscoveryEnabled) {
			        startDiscovery();
			    } else {
			        stopDiscovery();
			    }
			}
		}
	};
	
	public synchronized void setLeMode(boolean enable) {
		if (mLeModeEnabled == enable) return;
		
		// switch between LE and interactive mode
		mDaemon.setLeMode(enable);
		
		mLeModeEnabled = enable;
	}

	public synchronized void startDiscovery() {
	    if (mDiscoveryActive) return;
	    
        // set discovery flag to true
        mDiscoveryEnabled = true;
        
        WifiManager wifi_manager = (WifiManager)mContext.getSystemService(Context.WIFI_SERVICE);

        if (mMcastLock == null) {
            // listen to multicast packets
            mMcastLock = wifi_manager.createMulticastLock(TAG);
            mMcastLock.acquire();
        }
        
        // start discovery mechanism in the daemon
        mDaemon.startDiscovery();
        
        mDiscoveryActive = true;
	}
	
	public synchronized void stopDiscovery() {
	    if (!mDiscoveryActive) return;
	    
	    // set discovery flag to false
	    mDiscoveryEnabled = false;
	    
	    // stop discovery mechanism in the daemon
	    mDaemon.stopDiscovery();
	    
	    // release multicast lock
        if (mMcastLock != null) {
            mMcastLock.release();
            mMcastLock = null;
        }
        
        mDiscoveryActive = false;
	}
	
	public synchronized boolean getGloballyConnected() {
		return mGlobalConnected;
	}
	
	public synchronized void setGloballyConnected(boolean newState, OnRestartListener listener) {
		if (mGlobalConnected == newState) return;
		mDaemon.setGloballyConnected(newState);
		mGlobalConnected = newState;
		
		if (!newState) {
			restart(DaemonRunLevel.RUNLEVEL_NETWORK.swigValue() - 1, listener);
		}
	}
}
