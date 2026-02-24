# Firmware Upload Quick Test Guide

## What Was Fixed

The firmware upload feature now properly handles multipart file uploads from the web interface.

**Before**: Error "No firmware file provided"  
**After**: Full OTA (Over-The-Air) firmware update with automatic reboot

## Quick Start

### Step 1: Build Your Firmware
```bash
# In PlatformIO terminal
pio run
```

Find the compiled binary at:
```
.pio/build/esp12e/firmware.bin
```

### Step 2: Access Web Interface
1. Connect device to power
2. Open browser: `http://192.168.4.1` or `http://kilncontroller.local`
3. Click **"⬆️ Firmware"** tab

### Step 3: Upload Firmware
1. Click **"Browse"** button
2. Select your `firmware.bin` file
3. Verify file size displays (e.g., "File: firmware.bin (264KB)")
4. Click **"Upload Firmware"**
5. Confirm the dialog
6. Watch the spinner...

### Step 4: Wait for Reboot
- Upload progress: ~10-15 seconds (depends on file size)
- Device reboots automatically
- Page auto-reloads after 3 seconds
- You should see the new firmware running

## What Happens Behind the Scenes

```
Your Browser                Device (ESP8266)
         |                        |
         |--- Selects .bin file   |
         |                        |
         |--- Sends via multipart |
         |    POST /api/firmware  |
         |----------------------->|
         |                        |
         |               [Parses multipart headers]
         |               [Extracts binary data]
         |               [Update.begin(size)]
         |               [Streams to flash]
         |               [Update.end()]
         |               [ESP.restart()]
         |
         |<--- 200 OK JSON -------|
         |    success: true       |
         |    message: "Rebooting"
         |                        |
         |              [Device boots]
         |              [WiFi reconnects]
         |
         |<--- WiFi up -----------|
         |[Auto-reload page]      |
         |                        |
```

## Validation Checks

### Before Upload (Client-Side)
- ✓ File selected
- ✓ File size < 4MB
- ✓ User confirms dialog

### During Upload (Server-Side)
- ✓ POST method verified
- ✓ multipart/form-data detected
- ✓ Binary extracted from multipart
- ✓ Flash write successful
- ✓ Update.end() confirmed

## Expected Results

### Successful Upload
```
✓ Upload successful
Message: "Firmware updated successfully. Rebooting..."
[Device reboots in ~3 seconds]
[Page auto-reloads]
[New firmware running]
```

### File Too Large
```
✗ Upload failed
Error: "Firmware file too large (max 4MB)"
[File size checked before upload]
```

### Upload Failed
```
✗ Upload error
Error: "Firmware write failed"
[Serial monitor shows error details]
[Device remains on old firmware]
```

## Monitoring Upload

### Option 1: Serial Monitor
1. Open Arduino IDE or PlatformIO
2. Monitor speed: **115200 baud**
3. Watch for:
```
[OTA] Starting firmware update, size: 262144 bytes
[OTA] Firmware update successful, rebooting...
ets Jan  8 2013,rst cause:2, boot mode:(3,6)
load 0x4010e000 at 0x00000000, len 1216, cnt = 1
```

### Option 2: Browser Console
Press F12 in browser, check Console tab for detailed fetch requests:
```javascript
// Request
POST /api/firmware
Content-Type: multipart/form-data

// Response
200 OK
{
  "success": true,
  "message": "Firmware updated successfully. Rebooting..."
}
```

## Typical Upload Timeline

| Time | Event |
|------|-------|
| 0s | Upload starts |
| 1-8s | Sending firmware binary |
| 8-10s | Parsing multipart, writing to flash |
| 10s | Update.end() completes, ESP.restart() called |
| 10-15s | Device boots up |
| 15-17s | WiFi reconnects |
| 17-20s | Web page reloads |
| 20s+ | New firmware running! |

**Total time: 15-25 seconds depending on file size**

## File Size Reference

| Board | Max OTA | Typical Bin |
|-------|---------|------------|
| NodeMCU (4MB flash) | 1-2MB | 256-300KB |
| Wemos (4MB flash) | 1-2MB | 256-300KB |
| ESP-01S (1MB flash) | 256KB | 150-200KB |

Your device's maximum: **4MB** (check at compile time)

## Troubleshooting Checklist

| Issue | Solution |
|-------|----------|
| "No firmware file provided" | ✓ Fixed! Use updated code |
| File too large | Disable unused features, reduce code |
| Upload hangs | Wait 30s, power cycle, retry |
| Device doesn't reboot | Check serial monitor, may have flash error |
| New firmware not running | Old partition may still be active, force reflash |
| Can't access web interface after | Wait 60s, hard power cycle, try again |

## Testing Checklist

- [ ] Firmware compiles without errors
- [ ] Open web interface at http://192.168.4.1
- [ ] Navigate to "⬆️ Firmware" tab
- [ ] Click "Browse" and select firmware.bin
- [ ] File size displays correctly
- [ ] Click "Upload Firmware"
- [ ] Confirm dialog appears
- [ ] Spinner shows upload progress
- [ ] Success message appears
- [ ] Device reboots (serial monitor confirms)
- [ ] Web page auto-reloads
- [ ] New firmware features working
- [ ] Dashboard displays updated data

## Common Firmware File Names

These are correct to upload:
- ✓ `firmware.bin`
- ✓ `esp8266.bin`
- ✓ `KilnController.ino.generic.bin`
- ✓ `.pio/build/esp12e/firmware.bin`

**NOT correct** (these are intermediate files):
- ✗ `.elf` files
- ✗ `.o` object files
- ✗ `.a` archive files
- ✗ `.hex` files (use .bin instead)

## Advanced Testing

### Test 1: Verify Multipart Parsing
Upload a small file and watch serial output for multipart parsing:
```
[OTA] Starting firmware update, size: ...
```

### Test 2: Verify Flash Write
Check that data is written to correct partition:
```
esptool.py --port COM3 read_flash 0 8192 backup.bin
```

### Test 3: Verify Reboot
Monitor for automatic restart:
```
esp_loader.py --port COM3 read_status
```

### Test 4: Verify Persistence
After reboot, confirm new firmware is running:
- Check Dashboard for latest features
- Verify git commit hash (if displayed)
- Check firmware version in logs

## Performance Notes

- **Speed**: ~30-50 KB/s (typical for WiFi + flash write)
- **For 256KB file**: ~6-8 seconds
- **For 512KB file**: ~12-15 seconds
- **For 1MB file**: ~25-30 seconds

Actual speed depends on:
- WiFi signal strength
- Flash chip speed
- SPI bus load
- Available memory

## Success Indicators ✓

After successful firmware upload, you should see:

1. **Web Interface**: 
   - Page shows "Firmware uploaded successfully!"
   - Auto-reload after 3 seconds

2. **Device Status**:
   - LED blink pattern (if configured)
   - WiFi reconnects to network
   - Dashboard updates with new data

3. **Serial Monitor** (115200 baud):
   ```
   [OTA] Starting firmware update, size: 262144 bytes
   [OTA] Firmware update successful, rebooting...
   ets Jan  8 2013,rst cause:2, boot mode:(3,6)
   load 0x4010e000 at 0x00000000
   [... boot messages ...]
   [WiFi] Connected!
   ```

## Questions?

Refer to `FIRMWARE_UPLOAD_FIX.md` for complete technical details.
