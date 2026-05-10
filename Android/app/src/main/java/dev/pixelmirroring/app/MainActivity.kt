package dev.pixelmirroring.app

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import dev.pixelmirroring.app.service.AdbWifiManager

class MainActivity : ComponentActivity() {

    private lateinit var adbWifiManager: AdbWifiManager

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        
        adbWifiManager = AdbWifiManager(this)

        setContent {
            MaterialTheme {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    SetupScreen(adbWifiManager)
                }
            }
        }
    }
}

@Composable
fun SetupScreen(adbWifiManager: AdbWifiManager) {
    var hasPermission by remember { mutableStateOf(adbWifiManager.hasSecureSettingsPermission()) }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(24.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center
    ) {
        Text(
            text = "Pixel Mirroring",
            style = MaterialTheme.typography.headlineMedium,
            modifier = Modifier.padding(bottom = 32.dp)
        )

        if (hasPermission) {
            Icon(
                imageVector = androidx.compose.material.icons.Icons.Default.CheckCircle,
                contentDescription = "Success",
                tint = MaterialTheme.colorScheme.primary,
                modifier = Modifier.size(64.dp).padding(bottom = 16.dp)
            )
            Text(
                text = "Bereit für Verbindung!",
                style = MaterialTheme.typography.titleLarge
            )
            Text(
                text = "Du kannst nun den Desktop-Client starten.",
                textAlign = TextAlign.Center,
                modifier = Modifier.padding(top = 8.dp)
            )
        } else {
            Card(
                colors = CardDefaults.cardColors(
                    containerColor = MaterialTheme.colorScheme.errorContainer,
                    contentColor = MaterialTheme.colorScheme.onErrorContainer
                )
            ) {
                Column(modifier = Modifier.padding(16.dp)) {
                    Text(
                        text = "Ersteinrichtung nötig",
                        style = MaterialTheme.typography.titleMedium,
                        modifier = Modifier.padding(bottom = 8.dp)
                    )
                    Text(
                        text = "Bitte verbinde dein Gerät per USB mit dem PC und starte dort den Pixel Mirroring Client. Die nötigen Berechtigungen werden dann automatisch eingerichtet."
                    )
                }
            }
            
            Button(
                onClick = { hasPermission = adbWifiManager.hasSecureSettingsPermission() },
                modifier = Modifier.padding(top = 24.dp)
            ) {
                Text("Status aktualisieren")
            }
        }
    }
}
