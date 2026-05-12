package dev.pixelmirroring.app.network

import kotlinx.serialization.Serializable

@Serializable
data class ConnectRequest(
    val clientId: String,     // Eindeutige ID des Desktop-Clients
    val clientName: String    // z.B. "DESKTOP-ABC123"
)

@Serializable
data class ConnectResponse(
    val success: Boolean,
    val ips: List<String>,    // Alle erreichbaren IPs des Geräts
    val adbPort: Int,         // 5555
    val deviceName: String    // z.B. "Pixel 9 Pro"
)

@Serializable
data class StatusResponse(
    val adbWifiEnabled: Boolean,
    val hasPermission: Boolean,
    val deviceName: String
)
