package dev.pixelmirroring.app.network

import android.content.Context
import java.net.Inet4Address
import java.net.NetworkInterface

object NetworkScanner {
    fun getAllLocalIps(context: Context): List<String> {
        val ips = mutableListOf<String>()
        try {
            NetworkInterface.getNetworkInterfaces()?.asSequence()
                ?.filter { it.isUp && !it.isLoopback }
                ?.flatMap { it.inetAddresses.asSequence() }
                ?.filter { !it.isLoopbackAddress && it is Inet4Address }
                ?.forEach { ips.add(it.hostAddress!!) }
        } catch (e: Exception) {
            e.printStackTrace()
        }
        return ips
    }
}
