package de.tubs.ibr.dtn.discovery;

import android.content.Context;
import android.content.Intent;
import android.support.v4.content.WakefulBroadcastReceiver;

public class WakefulDiscoveryReceiver extends WakefulBroadcastReceiver {

    public static final String ACTION_BLE_DISCOVERY = "de.tubs.ibr.dtn.Intent.BLE_DISCOVERY";

    @Override
    public void onReceive(Context context, Intent intent) {

        Intent service = new Intent(context, DiscoveryService.class);
        // Start the service, keeping the device awake while it is launching.
        startWakefulService(context, service);
    }
}
