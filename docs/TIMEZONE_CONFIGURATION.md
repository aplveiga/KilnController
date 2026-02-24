# Timezone Configuration Guide

## Quick Reference

The NTP synchronization uses UTC (Coordinated Universal Time) by default. To use a different timezone, modify the timezone offset in the `syncTimeWithNTP()` function.

## Modifying Timezone

### Step 1: Locate the Configuration
Find line in `src/wireless.cpp` in the `syncTimeWithNTP()` function:

```cpp
configTime(0, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
```

### Step 2: Replace with Your Timezone
Change the first `0` to your timezone offset in seconds:

```cpp
configTime(TIMEZONE_OFFSET_SECONDS, DST_OFFSET_SECONDS, "pool.ntp.org", "time.nist.gov", "time.google.com");
```

### Step 3: Recompile and Upload
```bash
platformio run --target upload
```

## Common Timezones

### Americas

| Region | Timezone | Winter Offset | DST Offset | Code |
|--------|----------|---------------|-----------|------|
| Hawaii | HST | UTC-10 | -36000 | `configTime(-10*3600, 0, ...)`  |
| Alaska | AKST | UTC-9 | -32400 | `configTime(-9*3600, 0, ...)` |
| Pacific | PST | UTC-8 | -28800 | `configTime(-8*3600, 0, ...)` |
| Mountain | MST | UTC-7 | -25200 | `configTime(-7*3600, 0, ...)` |
| Central | CST | UTC-6 | -21600 | `configTime(-6*3600, 0, ...)` |
| Eastern | EST | UTC-5 | -18000 | `configTime(-5*3600, 0, ...)` |
| Brazil | BRT | UTC-3 | -10800 | `configTime(-3*3600, 0, ...)` |
| Argentina | ART | UTC-3 | -10800 | `configTime(-3*3600, 0, ...)` |

### Europe

| Region | Timezone | Winter Offset | DST Offset | Code |
|--------|----------|---------------|-----------|------|
| London | GMT | UTC+0 | 0 | `configTime(0, 0, ...)` |
| Ireland | GMT | UTC+0 | 0 | `configTime(0, 0, ...)` |
| CET | CET | UTC+1 | 3600 | `configTime(1*3600, 0, ...)` |
| Germany | CET | UTC+1 | 3600 | `configTime(1*3600, 0, ...)` |
| France | CET | UTC+1 | 3600 | `configTime(1*3600, 0, ...)` |
| Spain | CET | UTC+1 | 3600 | `configTime(1*3600, 0, ...)` |
| Eastern Europe | EET | UTC+2 | 7200 | `configTime(2*3600, 0, ...)` |
| Russia | MSK | UTC+3 | 10800 | `configTime(3*3600, 0, ...)` |

### Asia

| Region | Timezone | Offset | Code |
|--------|----------|--------|------|
| Dubai | GST | UTC+4 | `configTime(4*3600, 0, ...)` |
| India | IST | UTC+5:30 | `configTime(5*3600+30*60, 0, ...)` |
| Pakistan | PKT | UTC+5 | `configTime(5*3600, 0, ...)` |
| Bangladesh | BDT | UTC+6 | `configTime(6*3600, 0, ...)` |
| Thailand | ICT | UTC+7 | `configTime(7*3600, 0, ...)` |
| China | CST | UTC+8 | `configTime(8*3600, 0, ...)` |
| Japan | JST | UTC+9 | `configTime(9*3600, 0, ...)` |
| Australia (Sydney) | AEDT | UTC+11 | `configTime(11*3600, 0, ...)` |
| Australia (Perth) | AWST | UTC+8 | `configTime(8*3600, 0, ...)` |

### Africa & Middle East

| Region | Timezone | Offset | Code |
|--------|----------|--------|------|
| Egypt | EET | UTC+2 | `configTime(2*3600, 0, ...)` |
| South Africa | SAST | UTC+2 | `configTime(2*3600, 0, ...)` |
| Nigeria | WAT | UTC+1 | `configTime(1*3600, 0, ...)` |

### Pacific

| Region | Timezone | Offset | Code |
|--------|----------|--------|------|
| New Zealand | NZDT | UTC+13 | `configTime(13*3600, 0, ...)` |
| Fiji | FJT | UTC+12 | `configTime(12*3600, 0, ...)` |

## Daylight Saving Time (DST)

Some regions observe daylight saving time. The second parameter controls DST offset:

```cpp
configTime(TIMEZONE_OFFSET, DST_OFFSET, "pool.ntp.org", "time.nist.gov", "time.google.com");
```

### Examples with DST

**US Eastern Time (EST in winter, EDT in summer):**
```cpp
// EST is UTC-5, EDT is UTC-4 (which is UTC-5 + 3600 seconds)
configTime(-5*3600, 1*3600, "pool.ntp.org", "time.nist.gov", "time.google.com");
```

**Europe Central Time (CET in winter, CEST in summer):**
```cpp
// CET is UTC+1, CEST is UTC+2 (which is UTC+1 + 3600 seconds)
configTime(1*3600, 1*3600, "pool.ntp.org", "time.nist.gov", "time.google.com");
```

**Australia Eastern Time (AEST in winter, AEDT in summer):**
```cpp
// AEST is UTC+10, AEDT is UTC+11 (which is UTC+10 + 3600 seconds)
configTime(10*3600, 1*3600, "pool.ntp.org", "time.nist.gov", "time.google.com");
```

## Manual Calculation

To calculate offset in seconds:
1. Determine your UTC offset (e.g., EST = UTC-5)
2. Convert hours to seconds: -5 * 3600 = -18000
3. Use this value in the code

**Example: UTC-5:30 (India)**
```cpp
configTime(-5*3600 - 30*60, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
```

**Example: UTC+5:45 (Nepal)**
```cpp
configTime(5*3600 + 45*60, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
```

## Verification

After changing timezone, verify the time is correct:

1. Recompile and upload firmware
2. Connect to WiFi
3. Wait for "[NTP] Time synchronized" message
4. Check Serial output for time display
5. Verify time matches your local time

Example Serial output:
```
[NTP] Time synchronized: Sat Feb 22 15:30:45 2026
```

## Notes

- **Automatic DST Adjustment**: The `configTime()` function does NOT automatically adjust for DST. If your region observes DST, you must manually set the DST offset.
- **Consistency**: Use the same timezone offset throughout the year, or manually update it when DST starts/ends.
- **UTC Recommended**: For data logging, UTC is recommended as it's unambiguous and doesn't change.
- **Local Display**: If you need local time for display, apply offset when formatting output for users.

## Troubleshooting

**Time is 1 hour off**: You may have DST offset set when it shouldn't be, or vice versa.

**Time is many hours off**: Double-check your timezone offset calculation. 
- Positive offsets are EAST of UTC
- Negative offsets are WEST of UTC
- Multiply hours by 3600 to convert to seconds

**Time keeps resetting**: Ensure DST offset matches your region's current DST rules (or set to 0 if not applicable).

## Future Enhancement

A web interface option to configure timezone without recompiling would be ideal. This could be added to:
- Settings page in web interface
- Configuration file in LittleFS
- User preference storage

Currently, timezone requires code modification and recompilation.
