/*
 * Copyright (C) 2012 The CyanogenMod Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
 
package com.cyanogenmod.settings.device;

import android.os.AsyncTask;
import android.os.IBinder;
import android.os.SystemProperties;
import android.util.Log;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.ServiceManager;
import android.os.Parcel;
import android.os.RemoteException;
import com.cyanogenmod.settings.device.CMDProcessor;
import com.cyanogenmod.settings.device.CMDProcessor.CommandResult;
/**
 * Created with IntelliJ IDEA.
 * User: zyr3x
 * Date: 19.11.12
 * Time: 10:19
 * To change this template use File | Settings | File Templates.
 */
public class BootService extends Service  {

        public static boolean servicesStarted = false;
     
        @Override
        public int onStartCommand(Intent intent, int flags, int startId) {
            if (intent == null) {
                stopSelf();
            }
            new BootWorker(this).execute();
            return START_STICKY;
        }

        @Override
        public IBinder onBind(Intent intent) {
            return null;
        }

        class BootWorker extends AsyncTask<Void, Void, Void> {

            Context c;

            public BootWorker(Context c) {
                this.c = c;
            }
            
            private String getProp(String key,String def)
            {
		CMDProcessor cmd = new CMDProcessor();
		CommandResult result = cmd.su.runWaitFor("getprop "+key);
		return (result.getOutput().getFirst().equals("") || result.getOutput().getFirst() == null) ? def : result.getOutput().getFirst();
            }

            @Override
            protected Void doInBackground(Void... args) {
                return null;
            }
            

            @Override
            protected void onPostExecute(Void result) {
                super.onPostExecute(result);
                servicesStarted = true;
                stopSelf();
            }
        }

        @Override
        public void onDestroy() {
            super.onDestroy();
        }
}
