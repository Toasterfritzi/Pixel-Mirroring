package dev.pixelmirroring.app.service

import android.app.Service
import android.content.Intent
import android.os.IBinder

class MirroringService : Service() {
    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        // TODO: Start mirroring server
        return START_STICKY
    }
}