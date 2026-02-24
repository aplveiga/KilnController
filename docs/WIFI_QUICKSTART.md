# WiFi Configuration - Quick Start Guide

## Getting Started (30 seconds)

### Step 1: Power on the Device
- Connect power to your Kiln Controller
- Wait for the display to show the startup screen
- The device will try to connect to any saved WiFi network

### Step 2: Access the Configuration Portal
- Open WiFi settings on your smartphone, tablet, or computer
- Look for a network called **`KilnController`**
- Connect to it using password: **`12345678`**

### Step 3: Open the Web Interface
- Open a web browser (Chrome, Firefox, Safari, Edge, etc.)
- Navigate to: **`http://192.168.4.1`**
- You should see the WiFi configuration page

### Step 4: Connect to Your WiFi
**Option A: Automatic**
1. Click **"Scan Networks"** button
2. Click on your WiFi network in the list
3. Type your WiFi password
4. Click **"Connect"**

**Option B: Manual**
1. Type your WiFi network name (SSID)
2. Type your WiFi password
3. Click **"Connect"**

### Step 5: Verify Connection
- Check the status at the top of the page
- Once connected, you'll see:
  - Connection Status: **Connected**
  - Your IP address (something like 192.168.1.x)
  - Signal strength (RSSI)

## You're Done!

Your Kiln Controller is now connected to WiFi and ready for:
- ✅ Remote monitoring via OTA
- ✅ Firmware updates over WiFi
- ✅ Future MQTT integration

## What if it doesn't work?

### Network not appearing?
- Make sure your WiFi router is turned on
- Try scanning networks again (sometimes takes a few seconds)
- Ensure you're on the same floor/room as the device

### "Wrong password" error?
- Double-check password capitalization
- Avoid special characters if possible
- Try a simpler test network first

### Can't find 192.168.4.1?
- Make sure you're connected to the `KilnController` WiFi network
- Try opening the browser again
- Check if your device has a captive portal popup

### Still having trouble?
1. Click **"Reset"** button to clear all settings
2. Device will restart in AP mode (192.168.4.1)
3. Try connecting again with a different network

## Next Steps

### Upload Firmware Updates Over WiFi
Once connected, you can update firmware without cables:

**Using Arduino IDE:**
1. Connect to KilnController WiFi
2. Upload as normal (will use WiFi instead of cable)

**Using PlatformIO:**
1. Update upload_port in platformio.ini: `upload_port = 192.168.4.1`
2. Run: `pio run --target upload`

### Monitor Serial Output
To see WiFi connection details:
1. Connect USB cable to device
2. Open Serial Monitor (Tools > Serial Monitor)
3. Set baud rate to **115200**
4. You'll see WiFi status messages

## Advanced: Change Default AP Password

To change the WiFi password from `12345678` to something else:

1. Edit `include/wireless.h`
2. Find line: `const char* AP_PASSWORD = "12345678";`
3. Change to: `const char* AP_PASSWORD = "YourNewPassword";`
4. Rebuild and upload firmware

## Specifications

| Feature | Detail |
|---------|--------|
| **AP SSID** | KilnController |
| **AP Password** | 12345678 |
| **AP IP Address** | 192.168.4.1 |
| **Configuration URL** | http://192.168.4.1 |
| **WiFi Standard** | 802.11 b/g/n (2.4 GHz) |
| **Serial Baud Rate** | 115200 |
| **OTA Updates** | Enabled by default |
| **Settings Storage** | LittleFS Flash (persists after reboot) |

## Support

For more detailed information, see `WIFI_CONFIG.md` in the docs folder.
