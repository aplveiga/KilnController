# WiFi Configuration and OTA Firmware Updates

## Overview

The Kiln Controller includes a comprehensive WiFi management system that provides:

1. **Automatic WiFi Connection** - Attempts to connect to a previously saved WiFi network
2. **Fallback AP Mode** - If no connection is established, the device operates as an Access Point (AP)
3. **Web Configuration Portal** - User-friendly interface to scan and configure WiFi networks
4. **Flash Storage** - Settings are persisted in the ESP8266's flash memory
5. **OTA Firmware Updates** - Upload new firmware wirelessly without physical access

## Features

### 1. Automatic Network Connection

When the device boots:
- It loads the saved WiFi network credentials from flash memory
- Attempts to connect to the configured network (15-second timeout)
- If successful, the device operates in Station (STA) mode with internet connectivity
- If unsuccessful, it falls back to AP mode

### 2. Access Point Mode

The device always provides an Access Point for configuration:
- **SSID**: `KilnController`
- **Password**: `12345678`
- **IP Address**: `192.168.4.1`
- **Frequency**: 2.4 GHz

Even when connected to a WiFi network, the AP mode remains active for administration.

### 3. Web Configuration Portal

Access the configuration portal at `http://192.168.4.1` from any device connected to the KilnController AP:

#### Features:
- **Connection Status** - View current connection state, signal strength, and IP address
- **Network Scanning** - Scan for available WiFi networks with signal strength and channel information
- **Network Selection** - Click on any network in the list to auto-fill the SSID
- **Manual Configuration** - Enter SSID and password manually
- **Reset Configuration** - Clear saved settings and restart in AP mode

### 4. Configuration Storage

WiFi settings are stored in `/wifi_config.json` on the LittleFS flash filesystem:

```json
{
  "ssid": "MyNetworkName",
  "password": "MyPassword",
  "apMode": false
}
```

The configuration persists across device reboots.

### 5. OTA Firmware Updates

The device supports Over-The-Air firmware updates using the Arduino IDE or PlatformIO:

#### Using Arduino IDE:
1. Open the Kiln Controller sketch
2. Go to **Tools** → **Manage Libraries** and install `ArduinoOTA`
3. Connect to the device's WiFi network (or AP mode)
4. Go to **Tools** → **Network Ports** and select the KilnController device
5. Upload the firmware normally

#### Using PlatformIO:
1. Configure the target in `platformio.ini`:
   ```ini
   upload_protocol = espota
   upload_port = 192.168.4.1
   upload_flags = --auth=
   ```
2. Run: `platformio run --target upload`

#### Upload Progress:
Monitor the upload progress on the Serial console (115200 baud):
- `[OTA] Start updating sketch`
- `[OTA] Progress: XX%`
- `[OTA] Update complete!`

## Web Interface

### Status Section
Shows the current WiFi connection state:
- **Connection Status**: Connected / Disconnected
- **Signal Strength**: RSSI in dBm (only when connected)
- **IP Address**: Current IP (192.168.x.x for STA mode or 192.168.4.1 for AP mode)

### Network Scanning
1. Click **"Scan Networks"** to discover available WiFi networks
2. The list shows:
   - Network name (SSID)
   - Signal strength (RSSI) in dBm
   - WiFi channel

### Connecting to a Network
1. Click on a network in the list, or manually type the SSID
2. Enter the password
3. Click **"Connect"**
4. The device will save the credentials and attempt to connect
5. If successful, it will switch to Station mode
6. If unsuccessful, it remains in AP mode for retry

### Resetting Configuration
1. Click **"Reset"**
2. Confirm the action
3. All saved settings will be erased
4. The device will restart in AP mode

## Serial Monitor Output

Monitor the WiFi operations via Serial at 115200 baud:

```
[WiFi] LittleFS initialized
[WiFi] Configuration loaded: SSID=MyNetwork
[WiFi] Initialization complete
[WiFi] AP SSID: KilnController
[WiFi] Setup web server on http://192.168.4.1
[OTA] OTA updates enabled
[WiFi] Attempting to connect to: MyNetwork
[WiFi] Connected successfully!
[WiFi] IP address: 192.168.1.100
[WiFi] Signal strength: -55 dBm
[WiFi] AP Mode started
[WiFi] AP IP: 192.168.4.1
```

## Troubleshooting

### Device not appearing in AP list
- Ensure the device is powered and has booted completely
- Try manually entering 192.168.4.1 in your browser address bar
- Check that your device supports 2.4 GHz WiFi (802.11b/g/n)

### Connection fails repeatedly
- Verify the WiFi network is broadcasting (not hidden)
- Check password spelling and capitalization
- Ensure the network uses 2.4 GHz (not 5 GHz)
- Check WiFi signal strength at the device location

### OTA upload fails
- Ensure device is connected to WiFi (STA mode)
- Verify the hostname is reachable: `ping kilncontroller.local`
- Check that the device and your computer are on the same network
- Monitor serial output for OTA errors

### Settings reset unexpectedly
- Check that LittleFS is properly initialized
- Verify adequate flash space is available
- Try manually connecting to AP and resetting configuration via web UI

## API Endpoints

The web server provides the following REST endpoints:

### GET /api/status
Returns JSON with connection status:
```json
{
  "connected": true,
  "mode": "STA",
  "ip": "192.168.1.100",
  "gateway": "192.168.1.1",
  "rssi": -55,
  "ssid": "MyNetwork"
}
```

### GET /api/scan
Returns list of available networks:
```json
{
  "networks": [
    {
      "ssid": "Network1",
      "rssi": -45,
      "channel": 6,
      "encryption": 3
    }
  ]
}
```

### POST /api/connect
Connect to a WiFi network:
```json
{
  "ssid": "NetworkName",
  "password": "Password"
}
```

### POST /api/reset
Reset WiFi configuration and restart device

## Security Notes

- **Default AP Password**: Change the `AP_PASSWORD` constant in `wireless.h` for production use
- **No HTTPS**: The configuration portal uses plain HTTP (suitable for LAN only)
- **Default Credentials**: The AP password `12345678` should be changed
- **WiFi Password Storage**: Passwords are stored in plaintext in flash memory

For enhanced security:
1. Change the default AP password in `wireless.h`
2. Use WPA2 or WPA3 security on your WiFi network
3. Place the device on a secure LAN
4. Restrict access to the configuration portal

## Integration with Main Application

The WiFi manager is automatically initialized in the `setup()` function:

```cpp
// Initialize WiFi and OTA
wirelessManager.begin();
```

In the main loop, the WiFi manager is updated regularly:

```cpp
// Handle WiFi and OTA
wirelessManager.handleWiFi();
```

This ensures that:
- Web server requests are processed
- OTA updates can be received
- WiFi connection state is monitored
- Network scans are refreshed periodically

## Hardware Requirements

- **ESP8266 board** (NodeMCU, ESP-12E, etc.)
- **2.4 GHz WiFi network** (802.11 b/g/n)
- **Adequate power supply** (3.3V, 100+ mA during WiFi transmission)

## Library Dependencies

The following libraries are required (configured in `platformio.ini`):

- `ESP8266WiFi` - Core WiFi functionality
- `ESP8266WebServer` - Web server for configuration
- `ESP8266mDNS` - mDNS support for hostname resolution
- `ArduinoOTA` - Over-The-Air firmware updates
- `ArduinoJson` - JSON parsing for configuration
- `LittleFS` - Flash filesystem for settings storage

## Performance Considerations

- **Network scanning** takes approximately 1-2 seconds
- **Connection timeout** is set to 15 seconds
- **WiFi scans** are refreshed every 30 seconds while in AP mode
- **Web server** has low latency (<100ms response time)
- **OTA updates** can upload at ~500 KB/min over WiFi

## Future Enhancements

Potential improvements:
- MQTT support for remote monitoring
- Multiple saved WiFi networks with priority
- WiFi signal strength display on OLED screen
- Automatic reconnection to lost networks
- Firmware version check and auto-update
- SSL/TLS encryption for web interface
