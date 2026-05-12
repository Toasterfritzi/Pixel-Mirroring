# Integrationsprobleme zwischen Android App und PC-Client

Bei der Analyse der Codebasis wurden folgende Lücken und fehlende Implementierungen festgestellt, die aktuell eine Verbindung und das Mirroring verhindern:

## 1. Fehlendes Netzwerk-Discovery & API-Aufrufe (PC-Seite)
* **Android-Seite:** Die App startet einen Ktor HTTP-Server auf Port `18294` (in `MirroringService.kt`) und bietet die Endpunkte `/ping`, `/connect` und `/status` an. Dies ist für das Pairing und das Triggern des ADB-WiFi-Toggles zuständig.
* **PC-Seite:** Im C++ Client (`main.cpp`) fehlt diese Logik komplett. Der Client sendet keine HTTP-Requests an die Android-App, um das Pairing einzuleiten, und macht auch keinen Netzwerk-Scan, um die IP der Android-App zu finden.

## 2. Fehlender TCP-Connect für ADB (PC-Seite)
* **Problem:** Selbst wenn ADB over WiFi auf Android aktiv wäre, baut der PC-Client keine Verbindung auf. Es wird nur `adb devices -l` aufgerufen. Der essenzielle Aufruf `adb connect <Android-IP>:5555` fehlt in der `AdbClient`-Klasse.

## 3. Unfertige Scrcpy-Implementierung (Stubs im PC-Client)
Die Datei `scrcpy_client.cpp` besteht zum großen Teil aus leeren Platzhaltern (Stubs):
* **Kein Server-Push:** Die Datei `scrcpy-server.jar` wird nicht auf das Gerät kopiert. Der Code verlässt sich fälschlicherweise darauf, dass die Datei schon da ist.
* **Keine echten Sockets:** Die Methode `connect_sockets()` gibt einfach `true` zurück. Es gibt keine WinSock-Initialisierung und keinen TCP-Verbindungsaufbau (zu `localhost:27183`).
* **Kein Video-Stream:** `read_metadata()`, `video_thread_loop()` und `control_thread_loop()` sind leer. Es werden keine H.264 Video-Daten vom Server empfangen oder an den FFmpeg-Decoder weitergereicht.
* **Kein Forward-Fallback:** Bei der Port-Weiterleitung (`setup_tunnel()`) wird nur `adb reverse` gemacht; der Fallback auf `adb forward` (nötig für ältere Geräte/Android-Versionen) wurde übersprungen.

## 4. Pairing/Autorisierung läuft ins Leere
* Da der PC den `/connect`-Endpunkt (der die Client-ID übertragen sollte) nie aufruft, speichert die Android-App den PC nicht als autorisierten Client im `PairedClientStore`.
