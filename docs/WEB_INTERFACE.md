# Kiln Controller - Web Interface Documentation

## Overview

The Kiln Controller features a modern, multi-tab web interface accessible via HTTP. The interface provides WiFi configuration, real-time kiln status monitoring, and firmware management capabilities.

## Access Points

- **Connected to Network**: Navigate to `http://192.168.1.X` (your assigned IP) or `http://kilncontroller.local`
- **AP Mode Fallback**: Navigate to `http://192.168.4.1` (default AP IP)

## Interface Layout

The web interface uses a left-side menu with three main tabs:

### 1. 📡 WiFi Setup Tab

**Purpose**: Configure WiFi connectivity and monitor network status

#### Status Information Section
Displays four key status items:
- **Connection Status**: Shows "Connected" or "Disconnected"
- **Current Network**: Displays the SSID of connected network or "KilnController (AP)" if in AP mode
- **Signal Strength**: RSSI value in dBm when connected, "N/A" in AP mode
- **IP Address**: Current IP address (local IP when connected, 192.168.4.1 in AP mode)

#### Available Networks Section
- Click the **Scan Networks** button to discover available WiFi networks
- Networks are listed with:
  - Network name (SSID)
  - Signal strength (RSSI in dBm)
  - Channel number
- Click any network to auto-fill the SSID field

#### Connection Configuration
- **Network Name Field**: Enter or select the SSID to connect to
- **Password Field**: Enter the WiFi password
- **Three Control Buttons**:
  - **Scan Networks**: Discovers available WiFi networks (automatic periodic scanning also occurs)
  - **Connect**: Attempts connection with entered credentials
  - **Reset WiFi**: Clears saved WiFi configuration and restarts device in AP mode

#### Auto-Connect Feature
The device automatically attempts to connect to a saved network if:
1. It's discovered during periodic network scans
2. It's detected when the web interface performs a manual scan
- This happens in the background without requiring user action
- Connection status updates every 5 seconds

---

### 2. 📊 Dashboard Tab

**Purpose**: Monitor real-time kiln operation status from the SSD1306 display

#### Status Display Grid (2-Column Layout)

The dashboard shows six key parameters:

| Parameter | Description | Example |
|-----------|-------------|---------|
| **Temperature** | Current kiln interior temperature | 850 °C |
| **Set Point** | Target temperature for current program segment | 900 °C |
| **Program** | Currently running program identifier | Program 1 |
| **Status** | Current operation state | HEATING / HOLDING / COOLING / IDLE |
| **Rate** | Current temperature rise/fall rate | 120 °C/h |
| **Target** | Final target temperature of current segment | 1200 °C |

#### Real-Time Updates
- Dashboard data updates every 5 seconds via API calls
- Uses `/api/status` endpoint to fetch current values
- Smooth display of changing values without page refresh

---

### 3. ⬆️ Firmware Tab

**Purpose**: Upload and update device firmware

#### Firmware Upload Process

1. **Select Firmware File**
   - Click "Browse" button or click the file input area
   - Select a `.bin` firmware binary file
   - File information (name and size in KB) is displayed after selection

2. **Upload Firmware**
   - Click "Upload & Update" button to initiate transfer
   - Progress bar shows upload completion percentage
   - Upload status messages provide feedback

#### Firmware Sources

You can obtain firmware files from:
- **Arduino IDE**: Sketch → Export Compiled Binary
- **PlatformIO**: Build task creates `.pio/build/[board]/firmware.bin`
- **Pre-compiled Releases**: Check project repository

#### Important Notes

- Device will NOT automatically restart after web upload
- For automatic OTA updates, use:
  - Arduino IDE: Tools → Port → [hostname] (OTA port 3232)
  - PlatformIO: `platformio run --target upload`
- Backup important configuration before major updates

---

## Technical Details

### API Endpoints

The web interface communicates with these HTTP endpoints:

#### `/api/status` (GET)
Returns current connection status as JSON:
```json
{
  "connected": true,
  "mode": "STA",
  "ip": "192.168.1.100",
  "gateway": "192.168.1.1",
  "rssi": -65,
  "ssid": "MyNetwork"
}
```

#### `/api/scan` (GET)
Returns list of available WiFi networks:
```json
{
  "networks": [
    {"ssid": "Network1", "rssi": -45, "channel": 6, "encryption": 4},
    {"ssid": "Network2", "rssi": -72, "channel": 11, "encryption": 4}
  ],
  "autoConnectAttempted": false,
  "connectedSSID": ""
}
```

#### `/api/connect` (POST)
Attempts connection to specified network:
```json
Request:  {"ssid": "MyNetwork", "password": "password123"}
Response: {"success": true}
```

#### `/api/reset` (POST)
Resets WiFi configuration and restarts device:
```json
Response: {"success": true}
```

#### `/api/firmware` (POST)
Handles firmware upload (placeholder):
```json
Response: {"success": true, "message": "Use Arduino IDE or PlatformIO for OTA upload"}
```

### Styling & Responsive Design

- **Color Scheme**: Blue (#1976d2) primary, green (#4caf50) success, red (#f44336) danger
- **Layout**: Responsive flexbox design works on desktop and mobile devices
- **Menu**: Fixed left sidebar (200px width) with scrolling tab content
- **Status Boxes**: 2-column grid layout that adapts to screen size

### Auto-Update Features

**WiFi Status**: Updates every 5 seconds
- Connection status changes reflected immediately
- Signal strength and IP address updates

**Network Scanning**: Automatic periodic scans
- Runs every 30 seconds in background
- Detects known networks and auto-connects
- Visible in serial logs: `[WiFi] Known network detected...`

---

## Device Behavior

### Connection Flow

1. **Startup**: Device attempts to connect to saved network (STA mode)
2. **Success**: Switches to STA-only mode for stability
3. **Failure**: Enables AP mode as fallback (SSID: KilnController, IP: 192.168.4.1)
4. **During AP Mode**: Web server accessible at 192.168.4.1
5. **Network Appears**: Auto-connects to known network if found
6. **Restored**: Switches back to STA-only mode automatically

### Connection Timing

- **Initial Connection**: 15 seconds timeout
- **Periodic Scan**: Every 30 seconds
- **Auto-Reconnect**: Enabled by ESP8266 WiFi stack
- **Status Refresh**: Every 5 seconds in web interface

---

## Troubleshooting

### Can't Connect to Web Interface

**Problem**: Device not responding at expected IP
- Verify device is powered and has solid LED indicator
- Check WiFi connection: Look for "KilnController" AP if main network not available
- Access AP at: `http://192.168.4.1`
- Check router's DHCP client list for device IP

### WiFi Connection Keeps Dropping

**Problem**: Frequently disconnects from saved network
- Reduce WiFi interference: Move away from microwave/cordless phone
- Check signal strength (should be > -70 dBm for stable connection)
- Verify password is correct
- Power cycle device and router

### Auto-Connect Not Working

**Problem**: Device stays in AP mode when network is available
- Verify network SSID and password are saved (visible in status)
- Check periodic scan logs on serial monitor
- Manual click of "Scan Networks" triggers immediate auto-connect attempt

### Firmware Upload Fails

**Problem**: Upload shows error
- Verify file is valid `.bin` firmware file
- Check file size is less than available flash space
- Use Arduino IDE or PlatformIO for more advanced OTA features

---

## Advanced Features

### DHCP Server in AP Mode

When operating as an Access Point:
- DHCP server automatically enabled
- IP range: 192.168.4.2 to 192.168.4.254
- Lease time: Standard ESP8266 defaults
- Gateway/DNS: 192.168.4.1

### mDNS Hostname

Device is accessible at: `kilncontroller.local`
- Requires mDNS-compatible network
- Alternative to IP-based access
- Works on most modern routers

### Serial Monitoring

Monitor device activity via USB serial (115200 baud):
```
[WiFi] LittleFS initialized
[WiFi] Configuration loaded: SSID=MyNetwork
[WiFi] Attempting to connect to: MyNetwork
[WiFi] Connected successfully!
[WiFi] IP address: 192.168.1.100
[WiFi] Operating in STA-only mode for stability
```

---

## Security Recommendations

1. **Change Default AP Password**: Modify `AP_PASSWORD` in `wireless.h` from "12345678"
2. **Use Strong WiFi Password**: Protect against unauthorized access
3. **Disable AP Mode**: If only using STA mode, reduce attack surface
4. **Network Isolation**: Keep device on isolated or secure VLAN
5. **Firmware Updates**: Keep software up-to-date with latest patches

---

## Support

For detailed WiFi configuration documentation, see: `WIFI_CONFIG.md`
For OTA firmware update procedures, see: `WIFI_QUICKSTART.md`
