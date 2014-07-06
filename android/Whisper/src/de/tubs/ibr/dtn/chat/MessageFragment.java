package de.tubs.ibr.dtn.chat;

import android.app.AlertDialog;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.IntentSender.SendIntentException;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.IBinder;
import android.support.v4.app.Fragment;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import de.tubs.ibr.dtn.SecurityService;
import de.tubs.ibr.dtn.SecurityUtils;
import de.tubs.ibr.dtn.Services;
import de.tubs.ibr.dtn.api.ServiceNotAvailableException;
import de.tubs.ibr.dtn.chat.core.Buddy;
import de.tubs.ibr.dtn.chat.service.ChatService;
import de.tubs.ibr.dtn.chat.service.Utils;

public class MessageFragment extends Fragment {
	
	private static final int MESSAGE_LOADER_ID = 1;
	private static final int BUDDY_LOADER_ID = 2;
	
	@SuppressWarnings("unused")
	private final String TAG = "MessageFragment";
	
	private ChatService mService = null;
	private boolean mBound = false;
	
	private Long mBuddyId = null;
	private MessageComposer mComposer = null;
	private MessageList mMessageList = null;
	private BuddyDisplay mBuddyDisplay = null;
	private MenuItem mVoiceMenu = null;
	
	// Security API provided by IBR-DTN
	private SecurityService mSecurityService = null;
	private boolean mSecurityBound = false;
	
	private ServiceConnection mSecurityConnection = new ServiceConnection() {
		public void onServiceConnected(ComponentName name, IBinder service) {
			mSecurityService = SecurityService.Stub.asInterface(service);
			
			// refresh Gui elements
			refreshGui();
		}

		public void onServiceDisconnected(ComponentName name) {
			mSecurityService = null;
		}
	};
	
	private ServiceConnection mConnection = new ServiceConnection() {
		public void onServiceConnected(ComponentName name, IBinder service) {
			mService = ((ChatService.LocalBinder)service).getService();

			mMessageList.setChatService(mService);
			if (mBuddyDisplay != null) mBuddyDisplay.setChatService(mService);
			
			// initialize the loaders
			getLoaderManager().initLoader(MESSAGE_LOADER_ID,  null, mMessageList);
			if (mBuddyDisplay != null) getLoaderManager().initLoader(BUDDY_LOADER_ID,  null, mBuddyDisplay);
			
			// refresh Gui elements
			refreshGui();
			
			// restore draft message
			restoreDraftMessage();
		}

		public void onServiceDisconnected(ComponentName name) {
			getLoaderManager().destroyLoader(MESSAGE_LOADER_ID);
			getLoaderManager().destroyLoader(BUDDY_LOADER_ID);
			mService = null;
		}
	};
	
	/**
	 * Create a new instance of DetailsFragment, initialized to
	 * show the buddy with buddyId
	 */
	public static MessageFragment newInstance(Long buddyId) {
		MessageFragment f = new MessageFragment();

		// Supply buddyId input as an argument.
		Bundle args = new Bundle();
		if (buddyId != null) args.putLong(ChatService.EXTRA_BUDDY_ID, buddyId);
		f.setArguments(args);

		return f;
	}
	
	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container,
			Bundle savedInstanceState) {
		View v = inflater.inflate(R.layout.message_fragment, container, false);
		
		mComposer = (MessageComposer) v.findViewById(R.id.message_composer);
		mMessageList = (MessageList) v.findViewById(R.id.list_messages);
		mBuddyDisplay = (BuddyDisplay) v.findViewById(R.id.buddy_display);
		
		return v;
	}

	@Override
	public void onActivityCreated(Bundle savedInstanceState) {
		super.onActivityCreated(savedInstanceState);
		
		// enable options menu
		setHasOptionsMenu(true);
		
		// load buddy ID from arguments
		mBuddyId = getArguments().getLong(ChatService.EXTRA_BUDDY_ID, -1);
		if (mBuddyId == -1) mBuddyId = null;
		
		// assign buddy ID to sub-components
		mMessageList.setBuddyId(mBuddyId);
		if (mBuddyDisplay != null) mBuddyDisplay.setBuddyId(mBuddyId);
		
		// disable the composer
		mComposer.setEnabled(false);
		mComposer.setOnActionListener(new MessageComposer.OnActionListener() {
			
			@Override
			public void onSend(String message) {
				final Intent intent = new Intent(getActivity(), ChatService.class);
				intent.setAction(ChatService.ACTION_SEND_MESSAGE);
				intent.putExtra(ChatService.EXTRA_BUDDY_ID, mBuddyId);
				intent.putExtra(ChatService.EXTRA_TEXT_BODY, message);
				getActivity().startService(intent);
			}
			
			@Override
			public void onSecurityClick() {
				openSecurityInfoWindow();
			}
		});
	}
	
	private void restoreDraftMessage() {
		if (mBuddyId != null) {
			// load buddy from roster
			String msg = mService.getRoster().getDraftMessage( mBuddyId );
			
			mComposer.setMessage(msg);
			mComposer.setEnabled(true);
		}
	}
	
	public Long getBuddyId() {
		return mBuddyId;
	}
	
	@Override
	public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
		inflater.inflate(R.menu.message_menu, menu);
		
		// locate voice menu
		mVoiceMenu = menu.findItem(R.id.itemVoiceMessage);
		
		// refresh GUI
		refreshGui();
	}

	public void refreshGui() {
		if ((mService != null) && (mBuddyId != null)) {
			// get buddy object from the roster
			Buddy b = mService.getRoster().getBuddy(mBuddyId);
			
			if ((mVoiceMenu != null) && Utils.isVoiceRecordingSupported(getActivity()) && b.hasVoice()) {
				mVoiceMenu.setVisible(true);
			} else if (mVoiceMenu != null) {
				mVoiceMenu.setVisible(false);
			}
			
			// get security info
			Bundle keyinfo = SecurityUtils.getSecurityInfo(mSecurityService, b.getEndpoint());
			if (keyinfo != null) {
				if (keyinfo.containsKey(SecurityUtils.EXTRA_TRUST_LEVEL)) {
					int trust_level = keyinfo.getInt(SecurityUtils.EXTRA_TRUST_LEVEL);
					// set trust color
					if (trust_level > 67) {
						mComposer.setSecurityHint(R.drawable.ic_action_keylock_closed, R.color.green);
					}
					else if (trust_level > 33) {
						mComposer.setSecurityHint(R.drawable.ic_action_keylock_closed, R.color.yellow);
					}
					else if (trust_level > 0) {
						mComposer.setSecurityHint(R.drawable.ic_action_keylock_closed, R.color.red);
					}
				} else {
					mComposer.setSecurityHint(R.drawable.ic_action_keylock_open, android.R.color.darker_gray);
				}
			}
		} else {
			if (mVoiceMenu != null) mVoiceMenu.setVisible(false);
		}
	}
	
	private DialogInterface.OnClickListener message_delete_dialog = new DialogInterface.OnClickListener() {
		public void onClick(DialogInterface dialog, int which) {
			if (mService == null)
				return;
			
			if (mBuddyId != null) {
				// clear message of the buddy
				mService.getRoster().clearMessages(mBuddyId);
			}
		} 
	};

	@Override
	public boolean onOptionsItemSelected(MenuItem item) {
		switch (item.getItemId()) {
			case R.id.itemClearMessages:
				AlertDialog.Builder builder = new AlertDialog.Builder(getActivity());
				builder.setMessage(getActivity().getResources().getString(R.string.question_delete_messages));
				builder.setPositiveButton(getActivity().getResources().getString(android.R.string.yes), message_delete_dialog);
				builder.setNegativeButton(getActivity().getResources().getString(android.R.string.no), null);
				builder.show();
				return true;
				
			case R.id.itemVoiceMessage:
				startVoiceRecording();
				return true;
			
			default:
				return super.onOptionsItemSelected(item);
		}
	}
	
	@Override
	public void onStart() {
		super.onStart();
		
		// Establish a connection with the security service
		try {
			Services.SERVICE_SECURITY.bind(getActivity(), mSecurityConnection, Context.BIND_AUTO_CREATE);
			mSecurityBound = true;
		} catch (ServiceNotAvailableException e) {
			// Security API not available
		}
	}

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
	}

	@Override
	public void onDestroy() {
		// unbind from service
		if (mBound) {
			getActivity().unbindService(mConnection);
			getLoaderManager().destroyLoader(MESSAGE_LOADER_ID);
			getLoaderManager().destroyLoader(BUDDY_LOADER_ID);
			mBound = false;
		}
		
		super.onDestroy();
	}

	@Override
	public void onStop() {
		if (mSecurityBound) {
		    getActivity().unbindService(mSecurityConnection);
		    mSecurityBound = false;
		}
		
		super.onStop();
	}

	@Override
	public void onPause() {
		if (mBuddyId != null) {
			if (mService != null) {
				if (mComposer.getMessage().length() > 0)
					mService.getRoster().setDraftMessage(mBuddyId, mComposer.getMessage());
				else
					mService.getRoster().setDraftMessage(mBuddyId, null);
			}
		}
		
		// unregister to message updates
		getActivity().unregisterReceiver(mNotificationReceiver);
		
		super.onPause();
	}

	@Override
	public void onResume() {
		super.onResume();
		
		if ((mBuddyId != null) && !mBound) {
			// Establish a connection with the service.  We use an explicit
			// class name because we want a specific service implementation that
			// we know will be running in our own process (and thus won't be
			// supporting component replacement by other applications).
			getActivity().bindService(new Intent(getActivity(), ChatService.class), mConnection, Context.BIND_AUTO_CREATE);
			mBound = true;
		}
		
		// register to notifications
		IntentFilter filter = new IntentFilter(ChatService.ACTION_NEW_MESSAGE);
		filter.setPriority(10);
		getActivity().registerReceiver(mNotificationReceiver, filter);
	}
	
	private BroadcastReceiver mNotificationReceiver = new BroadcastReceiver() {
		@Override
		public void onReceive(Context context, Intent intent) {
			if (ChatService.ACTION_NEW_MESSAGE.equals(intent.getAction())) {
				Long buddyId = intent.getLongExtra(ChatService.EXTRA_BUDDY_ID, -1);
				if (mBuddyId == buddyId) abortBroadcast();
			}
		}
	};
	
	private void startVoiceRecording() {
		if (mService != null) {
			Buddy b = mService.getRoster().getBuddy(mBuddyId);
			if (!b.hasVoice()) return;
			
			final Intent i = new Intent("de.tubs.ibr.dtn.dtalkie.RECORD_MESSAGE");
			i.addCategory(Intent.CATEGORY_DEFAULT);
			i.putExtra("destination", b.getVoiceEndpoint());
			i.putExtra("singleton", true);
			startActivity(i);
		}
	}
	
	private void openSecurityInfoWindow() {
		if (mService != null) {
			Buddy b = mService.getRoster().getBuddy(mBuddyId);
			
			// get pending intent to open security info
			PendingIntent pi = SecurityUtils.getSecurityInfoIntent(mSecurityService, b.getEndpoint());
			try {
				if (pi != null) getActivity().startIntentSenderForResult(pi.getIntentSender(), 1, null, 0, 0, 0);
			} catch (SendIntentException e) {
				// intent error
			}
		}
	}
}
