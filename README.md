# 📱 Pixel Mirroring

> **Dein Android-Bildschirm — nativ auf deinem PC.**  
> Ein Open-Source-Pendant zu Apples iPhone Mirroring für Android/Pixel-Geräte.

---

## 🎯 Vision

Pixel Mirroring bringt die nahtlose iPhone-Mirroring-Erfahrung von macOS auf die Android-Welt. Die App ermöglicht es, den Bildschirm deines Android-Geräts in Echtzeit auf deinem Windows- oder macOS-PC zu spiegeln und zu steuern — vollständig nativ, ohne Browser-Technologie, mit minimalem RAM-Verbrauch.

---

## 🏗️ Architektur-Übersicht

Das System besteht aus **zwei Komponenten**, die über das lokale Netzwerk kommunizieren:

```
┌─────────────────────┐         ┌──────────────────────────────┐
│   📱 Android App    │◄───────►│   🖥️ Desktop Client          │
│   (Kotlin/Jetpack)  │  ADB    │   (C++ / Win32 + SDL2)      │
│                     │  TCP/IP │                              │
│  • Background       │         │  • Custom Window (borderless)│
│    Service          │         │  • Aspect-Ratio Lock         │
│  • ADB WiFi Toggle  │         │  • System Tray Integration   │
│  • Ping/Discovery   │         │  • scrcpy Protocol Client    │
│  • Material 3 UI    │         │  • FFmpeg Video Decoder      │
└─────────────────────┘         └──────────────────────────────┘
```

---

## 🔄 Verbindungsablauf

1. **Ersteinrichtung (Einmalig)**
   - Android App installieren und Gerät per USB an den PC anschließen.
   - Desktop Client starten. Dieser erkennt das Gerät und erteilt automatisch die `WRITE_SECURE_SETTINGS` Berechtigung via ADB. Keine Terminal-Eingabe durch den Nutzer nötig!
   - Geräte koppeln.

2. **Automatische Verbindung (Ab dem 2. Mal)**
   - Desktop Client startet → sendet Discovery-Request an alle bekannten IPs (LAN + VPN)
   - Android App empfängt Request im Background → verifiziert lokale Erreichbarkeit
   - App aktiviert ADB over WiFi (via `Settings.Global`)
   - App startet altes ADB TCP/IP Protokoll (`adb tcpip 5555`)
   - Desktop Client verbindet sich via ADB TCP/IP
   - Testbefehl wird ausgeführt (read-only Verifikation)
   - scrcpy-Server wird gepusht und gestartet
   - Video-Stream wird im Custom-Window angezeigt

---

## 📱 Android App

### Technologie
- **Sprache:** Kotlin
- **UI:** Jetpack Compose + Material 3 (Material You)
- **Min SDK:** Android 11 (API 30) — für Wireless Debugging Support
- **Target SDK:** Android 15 (API 35)

### Funktionen
| Funktion | Beschreibung |
|----------|-------------|
| **Background Service** | Foreground Service mit persistenter Notification, lauscht auf Discovery-Requests |
| **ADB WiFi Toggle** | Aktiviert `adb_wifi_enabled` via `Settings.Global` (benötigt `WRITE_SECURE_SETTINGS`) |
| **Netzwerk-Discovery** | Prüft Erreichbarkeit über alle lokalen IPs (LAN, VPN) auf festem Port |
| **Akku-Optimierung** | Minimaler Wakelock, nur aktiv bei eingehendem Request |
| **Ersteinrichtung UI** | Material 3 Wizard für Kopplung, Permission-Grant, und Einstellungen |

### Benötigte Permissions
```xml
<uses-permission android:name="android.permission.INTERNET" />
<uses-permission android:name="android.permission.FOREGROUND_SERVICE" />
<uses-permission android:name="android.permission.FOREGROUND_SERVICE_CONNECTED_DEVICE" />
<uses-permission android:name="android.permission.WRITE_SECURE_SETTINGS" />  <!-- via ADB -->
<uses-permission android:name="android.permission.RECEIVE_BOOT_COMPLETED" />
<uses-permission android:name="android.permission.WAKE_LOCK" />
<uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />
<uses-permission android:name="android.permission.ACCESS_WIFI_STATE" />
```

---

## 🖥️ Desktop Client

### Technologie
- **Sprache:** C++ (C++20)
- **Build System:** CMake (Cross-Platform)
- **Video:** FFmpeg (libavcodec, libavformat) für H.264/H.265 Decoding
- **Rendering:** SDL2 für Hardware-beschleunigte Video-Darstellung
- **Window Management (Windows):** Win32 API für Custom Borderless Window
- **Window Management (macOS):** Native Cocoa (Standard Window)
- **System Tray:** Native OS APIs (Win32 Shell_NotifyIcon / NSStatusItem)

### Custom Window Design (Windows)

Das Fenster hat **kein Standard-Titelbanner**. Stattdessen:

```
╭─────────────────────────────────────────────────╮
│                                    ⠿  ─  □  ✕  │  ← Kleine abgerundete Leiste
│                                                 │     oben rechts
│                                                 │
│              📱 Mirrored Screen                 │  ← Seitenverhältnis bleibt
│              (Aspect Ratio Lock)                │     immer erhalten
│                                                 │
│                                                 │
╰─────────────────────────────────────────────────╯
```

- **⠿** = Drag-Handle (einziger Bereich zum Fenster-Bewegen)
- **─ □ ✕** = Minimize, Maximize (mit Win11 Snap Layouts), Close
- Fenster frei skalierbar, aber Video-Stream behält immer das Seitenverhältnis
- Automatische Rotation wenn das Handy gedreht wird (Portrait ↔ Landscape)

### Implementierung (Win32)
- `WS_THICKFRAME | WS_CAPTION` Style mit `WM_NCCALCSIZE` Override
- `WM_NCHITTEST` für Custom Hit-Testing (Drag, Resize, Snap Layouts via `HTMAXBUTTON`)
- `DwmExtendFrameIntoClientArea` für native Schatten
- SDL2 Rendering in Win32 Child Window

### Features
| Feature | Beschreibung |
|---------|-------------|
| **System Tray** | Icon in Taskleiste (Win) / Menüleiste (macOS), schneller Zugriff |
| **Auto-Reconnect** | Bei Start automatische Verbindung zum letzten bekannten Gerät |
| **Aspect Ratio Lock** | Video-Stream behält immer korrektes Seitenverhältnis |
| **Auto-Rotate** | Fenster dreht sich mit dem Handy (Portrait ↔ Landscape) |
| **Audio Mirroring** | Der Ton des Android-Geräts wird ebenfalls auf den PC gestreamt |
| **Input Forwarding** | Maus/Tastatur-Eingaben werden an das Android-Gerät weitergeleitet |
| **Low Latency** | Direkte H.264 Hardware-Dekodierung, kein Browser-Overhead |

---

## 📁 Projektstruktur

```
Pixel-Mirroring/
├── README.md                    ← Diese Datei
├── Android/                     ← Android App (Kotlin/Jetpack Compose)
│   ├── app/
│   │   ├── src/main/
│   │   │   ├── java/.../pixelmirroring/
│   │   │   │   ├── service/     ← Background Service, ADB WiFi Toggle
│   │   │   │   ├── network/     ← Discovery, Ping, Connection Management
│   │   │   │   ├── ui/          ← Material 3 Compose Screens
│   │   │   │   └── util/        ← Helper, Settings, Crypto
│   │   │   └── AndroidManifest.xml
│   │   └── build.gradle.kts
│   └── build.gradle.kts
│
├── Client/                      ← Desktop Client (C++)
│   ├── CMakeLists.txt
│   ├── src/
│   │   ├── main.cpp             ← Entry Point, System Tray
│   │   ├── window/              ← Custom Window (Win32 / Cocoa)
│   │   ├── adb/                 ← ADB Protocol, Device Management
│   │   ├── stream/              ← scrcpy Protocol, FFmpeg Decoder
│   │   ├── input/               ← Input Forwarding (Mouse, Keyboard, Touch)
│   │   └── config/              ← Settings, Device Storage, Auto-Reconnect
│   ├── assets/                  ← Icons, Fonts
│   └── vendor/                  ← SDL2, FFmpeg (als Git Submodules)
│
└── scrcpy-server/               ← Angepasster scrcpy-Server (.jar)
    └── build.gradle.kts
```

---

## 🛠️ Build-Anforderungen

### Android
- Android Studio Ladybug+
- JDK 17+
- Android SDK 35
- Gradle 8.x

### Desktop (Windows)
- Visual Studio 2022+ oder MinGW/MSYS2
- CMake 3.25+
- FFmpeg 6.x+ Development Libraries
- SDL2 2.28+
- Windows SDK 10.0.22621+

### Desktop (macOS)
- Xcode 15+
- CMake 3.25+
- FFmpeg (via Homebrew)
- SDL2 (via Homebrew)

---

## 📋 Status

| Komponente | Status |
|-----------|--------|
| Projektplanung | ✅ Abgeschlossen |
| Android App | 🔲 Noch nicht begonnen |
| Desktop Client (Windows) | 🔲 Noch nicht begonnen |
| Desktop Client (macOS) | 🔲 Noch nicht begonnen |
| scrcpy Server Integration | 🔲 Noch nicht begonnen |

---

## 📜 Lizenz

Siehe [LICENSE](./LICENSE) für Details.
