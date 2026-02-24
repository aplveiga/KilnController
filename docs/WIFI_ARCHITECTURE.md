# WiFi System Architecture Diagram & Flow

## System Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                      ESP8266 Kiln Controller                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                   │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │              WirelessManager Class                        │  │
│  │  (include/wireless.h + src/wireless.cpp)                │  │
│  │                                                           │  │
│  │  ┌─────────────────────────────────────────────────┐   │  │
│  │  │  WiFi Connection Management                    │   │  │
│  │  │  • connectToKnownNetworks()                     │   │  │
│  │  │  • startAPMode()                               │   │  │
│  │  │  • handleWiFi() [called in loop]               │   │  │
│  │  └─────────────────────────────────────────────────┘   │  │
│  │                                                           │  │
│  │  ┌─────────────────────────────────────────────────┐   │  │
│  │  │  Configuration Storage (LittleFS)               │   │  │
│  │  │  • loadConfig() → /wifi_config.json             │   │  │
│  │  │  • saveConfig()                                 │   │  │
│  │  │  • resetConfig()                                │   │  │
│  │  └─────────────────────────────────────────────────┘   │  │
│  │                                                           │  │
│  │  ┌─────────────────────────────────────────────────┐   │  │
│  │  │  Web Server (ESP8266WebServer)                  │   │  │
│  │  │  • Port 80 (HTTP)                               │   │  │
│  │  │  • handleRoot() → HTML/JS                       │   │  │
│  │  │  • handleScan() → /api/scan                     │   │  │
│  │  │  • handleConnect() → /api/connect               │   │  │
│  │  │  • handleReset() → /api/reset                   │   │  │
│  │  └─────────────────────────────────────────────────┘   │  │
│  │                                                           │  │
│  │  ┌─────────────────────────────────────────────────┐   │  │
│  │  │  OTA Firmware Updates (ArduinoOTA)              │   │  │
│  │  │  • setupOTA()                                   │   │  │
│  │  │  • Hostname: kilncontroller                     │   │  │
│  │  │  • Port 3232 (OTA protocol)                     │   │  │
│  │  └─────────────────────────────────────────────────┘   │  │
│  │                                                           │  │
│  │  ┌─────────────────────────────────────────────────┐   │  │
│  │  │  Network Scanning (ESP8266WiFi)                 │   │  │
│  │  │  • scanNetworks()                               │   │  │
│  │  │  • Returns SSID, RSSI, channel                  │   │  │
│  │  └─────────────────────────────────────────────────┘   │  │
│  │                                                           │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                   │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │         Main Application (KilnController.cpp)            │  │
│  │                                                           │  │
│  │  setup() calls: wirelessManager.begin()                 │  │
│  │  loop() calls: wirelessManager.handleWiFi()             │  │
│  │                                                           │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                   │
└─────────────────────────────────────────────────────────────────┘
                              │
                    ┌─────────┴────────┐
                    │                  │
         ┌──────────▼────────┐  ┌──────▼─────────┐
         │  Hardware WiFi    │  │   LittleFS     │
         │  (ESP8266 built-in)│  │   Flash Mem    │
         │                   │  │                │
         │  • STA Mode       │  │  Config File:  │
         │  • AP Mode (2.4G) │  │  /wifi_config. │
         │  • 802.11 b/g/n   │  │   json         │
         │                   │  │                │
         └───────────────────┘  └────────────────┘
```

## Boot Sequence Diagram

```
┌─────────────┐
│  Power On   │
└──────┬──────┘
       │
       ▼
┌──────────────────────┐
│ Initialize Hardware  │
│ • SSD1306 display    │
│ • MAX6675 thermocouple
│ • SSR control        │
│ • LittleFS           │
└──────┬───────────────┘
       │
       ▼
┌──────────────────────┐
│ wirelessManager.     │
│   begin()            │
├──────────────────────┤
│ • Load config        │
│ • Set WiFi mode      │
│   (AP+STA)           │
│ • Start AP           │
│ • Start web server   │
│ • Setup OTA          │
└──────┬───────────────┘
       │
       ├─ Attempt STA connection ───┐
       │  (15 second timeout)        │
       │                             │
       ▼                             ▼
   ┌─────────┐            ┌──────────────────┐
   │Connected│            │ Connection Failed│
   │ to WiFi │            │ (Stay in AP mode)│
   └────┬────┘            └────────┬─────────┘
        │                          │
        ▼                          ▼
    ┌────────┐              ┌──────────────┐
    │STA+AP  │              │AP Mode Only  │
    │ Running│              │Ready         │
    └────────┘              └──────────────┘
        │                          │
        └──────────┬───────────────┘
                   │
                   ▼
        ┌──────────────────────┐
        │  Ready for Operation │
        │  • Web server active │
        │  • OTA available     │
        │  • Control loop runs │
        └──────────────────────┘
```

## WiFi Connection Flowchart

```
┌──────────────────────┐
│ User opens browser   │
│ http://192.168.4.1   │
└──────────┬───────────┘
           │
           ▼
    ┌──────────────┐
    │  Download    │
    │  HTML/JS UI  │
    │  (minified)  │
    └──────┬───────┘
           │
           ▼
    ┌──────────────────────┐
    │ Display Status Panel │
    │ • Connection status  │
    │ • Current IP         │
    │ • Signal strength    │
    └──────┬───────────────┘
           │
    ┌──────┴──────┐
    │             │
    ▼             ▼
┌─────────┐  ┌──────────────┐
│  Click  │  │ Manual Entry │
│ "Scan"  │  │ (Type SSID/  │
└────┬────┘  │  password)   │
     │       └────────┬─────┘
     │                │
     ▼                │
┌───────────┐         │
│Scan local │         │
│ networks  │         │
└────┬──────┘         │
     │                │
     ▼                │
┌────────────────┐    │
│Display list:   │    │
│• SSID          │    │
│• Signal (RSSI) │    │
│• Channel       │    │
└────┬───────────┘    │
     │                │
     ▼                │
┌──────────────┐      │
│User clicks   │      │
│network or    │      │
│types entry   │      │
└────┬─────────┘      │
     │                │
     └────────┬───────┘
              │
              ▼
    ┌─────────────────┐
    │ User enters     │
    │ password and    │
    │ clicks          │
    │ "Connect"       │
    └────────┬────────┘
             │
             ▼
    ┌──────────────────────┐
    │ Send credentials:    │
    │ POST /api/connect    │
    │ JSON: {ssid, pwd}    │
    └────────┬─────────────┘
             │
             ▼
    ┌──────────────────────┐
    │ Device receives      │
    │ & validates input    │
    └────────┬─────────────┘
             │
             ▼
    ┌──────────────────────┐
    │ Save to flash:       │
    │ /wifi_config.json    │
    └────────┬─────────────┘
             │
             ▼
    ┌──────────────────────┐
    │ Attempt WiFi         │
    │ connection (15s)     │
    └────────┬─────────────┘
             │
        ┌────┴─────┐
        │           │
        ▼           ▼
    ┌────────┐  ┌─────────────┐
    │Success │  │Failed       │
    ├────────┤  ├─────────────┤
    │STA OK  │  │Stay in AP   │
    │Apply on│  │Retry later  │
    │reboot  │  │             │
    └────────┘  └─────────────┘
```

## Main Loop Operation

```
┌─────────────────────────────────────────────────┐
│          Main Loop (repeating)                  │
└────────────────┬────────────────────────────────┘
                 │
                 ▼
    ┌────────────────────────┐
    │ wirelessManager.       │
    │   handleWiFi()         │
    ├────────────────────────┤
    │ • Process web requests │
    │ • Handle OTA data      │
    │ • Monitor connection   │
    │ • Scan networks (30s)  │
    └────────────┬───────────┘
                 │
                 ▼
    ┌────────────────────────┐
    │ Read temperature       │
    │ (1 Hz interval)        │
    └────────────┬───────────┘
                 │
                 ▼
    ┌────────────────────────┐
    │ Compute PID            │
    └────────────┬───────────┘
                 │
                 ▼
    ┌────────────────────────┐
    │ Update SSR output      │
    └────────────┬───────────┘
                 │
                 ▼
    ┌────────────────────────┐
    │ Update display (OLED)  │
    └────────────┬───────────┘
                 │
                 ▼
    ┌────────────────────────┐
    │ Log to serial          │
    └────────────┬───────────┘
                 │
                 ▼
    ┌────────────────────────┐
    │ Save state if dirty    │
    │ (throttled 10s)        │
    └────────────┬───────────┘
                 │
                 ▼
    ┌────────────────────────┐
    │ Continue loop...       │
    └────────────────────────┘
```

## Web API Request Flow

```
Client (Browser/App)
       │
       ├─────────── GET / ────────────────┐
       │                                  │
       │                                  ▼
       │                         ┌──────────────────┐
       │                         │ handleRoot()     │
       │                         │ Returns:         │
       │                         │ • HTML page      │
       │                         │ • JavaScript     │
       │                         │ • Styling        │
       │                         └──────────────────┘
       │
       ├─────────── GET /api/status ──────┐
       │                                  │
       │                                  ▼
       │                         ┌──────────────────┐
       │                         │ getStatusJSON()  │
       │                         │ Returns:         │
       │                         │ {connected,      │
       │                         │  mode, ip, rssi} │
       │                         └──────────────────┘
       │
       ├─────────── GET /api/scan ───────┐
       │                                  │
       │                                  ▼
       │                         ┌──────────────────┐
       │                         │ handleScan()     │
       │                         │ Returns:         │
       │                         │ {networks: [{    │
       │                         │  ssid, rssi,     │
       │                         │  channel}...]}   │
       │                         └──────────────────┘
       │
       ├───────── POST /api/connect ──────┐
       │ {ssid, password}                 │
       │                                  ▼
       │                         ┌──────────────────┐
       │                         │ handleConnect()  │
       │                         │ • Save config    │
       │                         │ • Attempt connect│
       │                         │ Returns: {succ...}
       │                         └──────────────────┘
       │
       └────────── POST /api/reset ───────┐
                                          │
                                          ▼
                                 ┌──────────────────┐
                                 │ handleReset()    │
                                 │ • Delete config  │
                                 │ • Restart        │
                                 │ Returns: {succ...}
                                 └──────────────────┘
```

## Memory Layout

```
┌─────────────────────────────────────────────────────────┐
│                    Flash Memory (4MB)                    │
├─────────────────────────────────────────────────────────┤
│                                                           │
│  Firmware Code (~256 KB)                                │
│  ┌──────────────────────────────────────────────────┐  │
│  │ • Arduino Core                                   │  │
│  │ • WiFi Stack (ESP8266WiFi, WebServer)          │  │
│  │ • OTA Handler (ArduinoOTA)                      │  │
│  │ • Libraries (Adafruit, MAX6675, PID, etc)      │  │
│  │ • Application Code (~50 KB WiFi Manager)       │  │
│  │ • Kiln Control Code (~60 KB)                   │  │
│  └──────────────────────────────────────────────────┘  │
│                                                           │
│  OTA Temp Buffer (~256 KB)                              │
│  ┌──────────────────────────────────────────────────┐  │
│  │ Used during firmware upload, erased after       │  │
│  └──────────────────────────────────────────────────┘  │
│                                                           │
│  LittleFS Filesystem (~1 MB)                            │
│  ┌──────────────────────────────────────────────────┐  │
│  │ /programs.json          (~5 KB)                 │  │
│  │ /kiln_state.txt         (~1 KB)                 │  │
│  │ /wifi_config.json       (<1 KB)                 │  │
│  │ [Free space for logging/future use]             │  │
│  └──────────────────────────────────────────────────┘  │
│                                                           │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│                    RAM Memory (160 KB)                   │
├─────────────────────────────────────────────────────────┤
│                                                           │
│  Sketch Data / Stack (~80 KB)                           │
│  ┌──────────────────────────────────────────────────┐  │
│  │ • String buffers                                │  │
│  │ • Web server response buffer                    │  │
│  │ • Temperature readings                          │  │
│  │ • Program state                                 │  │
│  │ • JSON parsing buffer (~2 KB)                  │  │
│  └──────────────────────────────────────────────────┘  │
│                                                           │
│  WiFi Stack & Heap (~80 KB)                            │
│  ┌──────────────────────────────────────────────────┐  │
│  │ • WiFi driver (~15 KB)                          │  │
│  │ • Web server connection buffers (~5 KB)         │  │
│  │ • OTA handler (~5 KB)                           │  │
│  │ • General heap for dynamic allocation           │  │
│  └──────────────────────────────────────────────────┘  │
│                                                           │
└─────────────────────────────────────────────────────────┘
```

## OTA Update Process

```
┌──────────────────────────────────┐
│  Developer Machine               │
│  (Arduino IDE or PlatformIO)     │
└──────────────┬───────────────────┘
               │
               ▼
       ┌───────────────┐
       │ Compile Code  │
       │ Binary: .bin  │
       └───────┬───────┘
               │
               ▼
       ┌───────────────────┐
       │ Start OTA Upload  │
       │ mDNS Discovery:   │
       │kilncontroller.local
       └───────┬───────────┘
               │
               ▼ (TCP Port 3232)
┌──────────────────────────────────┐
│  ESP8266 Device                  │
│  (WiFi Connected)                │
└──────────────┬───────────────────┘
               │
               ▼
       ┌───────────────────┐
       │ Receive .bin data │
       │ chunks            │
       └───────┬───────────┘
               │
       ┌──────┴──────┐
       │ Progress... │
       │ 25%, 50%... │
       └──────┬──────┘
               │
               ▼
       ┌───────────────────┐
       │ Flash new firmware│
       │ to OTA slot       │
       └───────┬───────────┘
               │
               ▼
       ┌───────────────────┐
       │ Verify checksum   │
       └───────┬───────────┘
               │
        ┌──────┴──────┐
        │             │
        ▼             ▼
    ┌────────┐   ┌──────────┐
    │ Success│   │  Failed  │
    ├────────┤   ├──────────┤
    │Reboot  │   │ Keep old │
    │w/ new  │   │ firmware │
    │code    │   │ Retry OTA│
    └────────┘   └──────────┘
```

## File Structure

```
KilnController/
├── .pio/                          [PlatformIO build artifacts]
├── .vscode/                       [VS Code settings]
├── include/
│   └── wireless.h                 [WiFi Manager class definition]
├── lib/                           [User libraries]
├── src/
│   ├── KilnController.cpp         [Main application + WiFi integration]
│   └── wireless.cpp               [WiFi Manager implementation]
├── test/                          [Unit tests]
├── docs/
│   ├── IMPLEMENTATION.md          [Technical guide (updated)]
│   ├── WIFI_CONFIG.md             [Complete WiFi documentation]
│   ├── WIFI_QUICKSTART.md         [30-second setup guide]
│   ├── WIFI_IMPLEMENTATION_SUMMARY.md
│   ├── USER_MANUAL.md             [Operator manual]
│   └── Requirements.md            [Original requirements]
├── platformio.ini                 [Project config (updated with WiFi libs)]
└── README.md                      [Project overview]
```

---

**Diagram Version**: 1.0
**Generated**: February 22, 2026
**Status**: Complete
