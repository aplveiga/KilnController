# NTP Time Synchronization

## Overview
The Kiln Controller now automatically synchronizes its internal clock with NTP (Network Time Protocol) servers when connected to WiFi. This ensures accurate timestamps for all logged data, especially important for the data logger which uses Unix timestamps.

## Features

### Automatic Time Synchronization
- Synchronizes time automatically when WiFi connects
- Uses multiple NTP servers for redundancy:
  - `pool.ntp.org` (primary)
  - `time.nist.gov` (secondary)
  - `time.google.com` (tertiary)
- 1-hour cooldown between sync attempts (prevents excessive network traffic)
- 20-second timeout per sync attempt (non-blocking design)

### Time Validation
- Validates that time is reasonable (after Jan 1, 2000)
- Provides `isTimeValid()` method to check if time is synchronized
- Status logged to Serial for debugging

## How It Works

### Initialization
When the ESP8266 boots:
1. Initial time is set to epoch (Jan 1, 1970, 00:00:00 UTC)
2. Data logger uses this initial time for timestamps
3. When WiFi connects, NTP synchronization is triggered

### Synchronization Process
1. Check if WiFi is connected
2. Check if NTP sync is needed (1-hour cooldown)
3. Call `configTime()` with NTP servers
4. Wait up to 20 seconds for time to be received
5. Validate received time (must be after year 2000)
6. Log result to Serial

### Timezone Support
The current implementation uses UTC (timezone offset = 0). To modify for a specific timezone:

**Example: EST (UTC-5) without DST:**
```cpp
configTime(-5 * 3600, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
```

**Example: CET (UTC+1) with 1-hour DST:**
```cpp
configTime(1 * 3600, 1 * 3600, "pool.ntp.org", "time.nist.gov", "time.google.com");
```

## API

### Public Methods

#### `void syncTimeWithNTP()`
Synchronizes time with NTP server if:
- WiFi is connected
- 1 hour has passed since last sync (or first sync)

Called automatically from `handleWiFi()` - no manual intervention needed.

#### `bool isTimeValid()`
Returns `true` if current system time is after January 1, 2000 (i.e., properly synchronized).

```cpp
if (wirelessManager.isTimeValid()) {
    time_t current = time(nullptr);
    struct tm* timeinfo = localtime(&current);
    Serial.printf("Current time: %s\n", asctime(timeinfo));
} else {
    Serial.println("Time not yet synchronized");
}
```

## Serial Output Examples

### Successful Synchronization
```
[WiFi] Connection established, SSID: MyNetwork
[NTP] Synchronizing time with NTP server...
[NTP] Waiting for NTP time to be set...
[NTP] Time synchronized: Sat Feb 22 15:30:45 2026
```

### Failed Synchronization (no WiFi)
```
[WiFi] AP mode enabled (no network)
[NTP] Skipped (WiFi not connected)
```

### Failed Synchronization (timeout)
```
[NTP] Synchronizing time with NTP server...
[NTP] Waiting for NTP time to be set...
[NTP] Failed to synchronize time
```

## Data Logger Integration

The data logger automatically benefits from NTP synchronization:

1. **Before NTP sync**: Timestamps use system time starting from 1970-01-01 (not useful)
2. **After NTP sync**: Timestamps match actual date/time
3. **Web Interface**: The Data Logger tab displays times in readable format using synchronized timestamps

Example logged data timestamps:
```
Before sync: 1699564800 → Jan 1, 1970 + 1699564800 seconds
After sync:  1771849445 → Feb 22, 2026 15:30:45
```

## Memory and Performance

- **RAM Usage**: ~200 bytes (minimal)
- **ROM Usage**: ~1.5 KB (minimal)
- **Network Traffic**: 1-2 NTP packets per sync (120 bytes total)
- **Blocking Time**: ~20 seconds maximum (only first sync after WiFi connection)
- **CPU Impact**: Negligible (async waiting with yield())

## Troubleshooting

### Time not synchronizing

**Issue**: "Failed to synchronize time" messages repeatedly

**Solutions**:
1. Verify WiFi connection is stable
2. Check that NTP port 123 is open (some networks block it)
3. Try alternative NTP servers:
   - `time.cloudflare.com`
   - `time.apple.com`
   - `ntp.ubuntu.com`

### Timestamps incorrect

**Issue**: Timestamps are off by several hours

**Solution**: Modify timezone offset in `syncTimeWithNTP()`:
```cpp
// For EST (UTC-5)
configTime(-5 * 3600, 0, ...);
// For PST (UTC-8)
configTime(-8 * 3600, 0, ...);
// For CET (UTC+1)
configTime(1 * 3600, 0, ...);
```

### NTP sync happening too frequently

**Issue**: "Time synchronized" messages every few seconds

**Solution**: This is expected after WiFi connects, then the 1-hour cooldown prevents further sync attempts.

## Future Enhancements

1. **Configurable Timezone**: Add web interface to set timezone offset
2. **Daylight Saving Time Support**: Auto-adjust for DST in specific regions
3. **NTP Server Selection**: Allow user to specify custom NTP servers
4. **Time Status Web Page**: Display current time, sync status, timezone in web UI
5. **RTC Backup**: Use external RTC module to maintain time during power loss

## Standards Compliance

- **RFC 5905**: NTP Protocol specification
- **NIST Time Servers**: Compatible with NIST NTP servers
- **Pool.NTP.Org**: Uses public NTP pool project
- **UTC Epoch**: Uses standard Unix timestamp format (seconds since Jan 1, 1970 UTC)

## Testing

To verify NTP synchronization is working:

1. Upload firmware with WiFi credentials configured
2. Device connects to WiFi
3. Watch Serial output for "[NTP] Time synchronized" message
4. Check that logged data in Data Logger tab shows current timestamps
5. Use web interface Data Logger to verify timestamps match current time
