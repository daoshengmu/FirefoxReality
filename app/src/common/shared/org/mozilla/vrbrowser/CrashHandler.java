package org.mozilla.vrbrowser;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.os.StrictMode;
import android.support.annotation.Nullable;
import android.util.Log;

import org.mozilla.geckoview.CrashReporter;
import org.mozilla.geckoview.GeckoRuntime;

public class CrashHandler extends Service {
    private static final String LOGTAG = "CrashHandler";
    private Intent mCrashIntent;

    public CrashHandler() {
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Log.e(LOGTAG, "CrashHandler::onStartCommand(), type: " + intent.getAction());
        if (intent == null) {
            stopSelf();
            return Service.START_NOT_STICKY;
        }

        if (GeckoRuntime.ACTION_CRASHED.equals(intent.getAction())) {
            mCrashIntent = intent;

            Log.d(LOGTAG, "Dump File: " +
                    mCrashIntent.getStringExtra(GeckoRuntime.EXTRA_MINIDUMP_PATH));
            Log.d(LOGTAG, "Extras File: " +
                    mCrashIntent.getStringExtra(GeckoRuntime.EXTRA_EXTRAS_PATH));
            Log.d(LOGTAG, "Dump Success: " +
                    mCrashIntent.getBooleanExtra(GeckoRuntime.EXTRA_MINIDUMP_SUCCESS, false));
            Log.d(LOGTAG, "Fatal: " +
                    mCrashIntent.getBooleanExtra(GeckoRuntime.EXTRA_CRASH_FATAL, false));

            StrictMode.ThreadPolicy oldPolicy = null;
            if (BuildConfig.DEBUG) {
                oldPolicy = StrictMode.getThreadPolicy();
                // We do some disk I/O and network I/O on the main thread, but it's fine.
                StrictMode.setThreadPolicy(new StrictMode.ThreadPolicy.Builder(oldPolicy)
                        .permitDiskReads()
                        .permitDiskWrites()
                        .permitNetwork()
                        .build());
            }

            try {
                Log.e(LOGTAG, "Sending crash reports.");
                CrashReporter.sendCrashReport(this, mCrashIntent, "FirefoxReality");
            } catch (Exception e) {
                Log.e(LOGTAG, "Failed to send crash report", e);
            }

            if (oldPolicy != null) {
                StrictMode.setThreadPolicy(oldPolicy);
            }

            stopSelf();
        }

        return Service.START_NOT_STICKY;
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

}
