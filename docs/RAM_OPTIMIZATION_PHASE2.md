# RAM Optimization - Phase 2: Persistent Allocations

**Current Status:** 77.4% RAM still (optimizations target runtime allocations)

## Real Issue: Static/Global RAM Consumption

The 63,432 bytes used at startup comes from:
1. **Global WiFi server buffers** (~15-20KB)
2. **Global display buffer** (~1KB) 
3. **Global String objects** (~2-3KB)
4. **PID controller state** (~0.5KB)
5. **Program manager** (~1KB)
6. **Fragmentation & overhead** (~5KB)
7. **WiFi stack** (~10KB)
8. **Unavoidable system** (~20KB)

---

## REAL Solutions - Phase 2

### 2.1 Move HTML to PROGMEM (Saves 1-2KB)

Instead of storing HTML strings in RAM during sendContent(), use F() macro to store in FLASH:

```cpp
// FILE: src/webportal.cpp

void WirelessManager::handleRoot() {
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");
    
    // Use F() to store strings in PROGMEM (FLASH), not RAM
    server.sendContent(F("<!DOCTYPE html>"));
    server.sendContent(F("<html>"));
    // ... all the CSS, JS, HTML ...
    server.sendContent(F("</html>"));
}
```

**Why this helps:** HTML is only loaded when a request comes in, not at startup.

### 2.2 Further Reduce Global String Variables

**Current (main.cpp, line 131):**
```cpp
String programName = "";  // Heap string, always allocated
```

**Better:**
```cpp
// Use char[] or load on-demand from flash
// or use in functions only, not globally
```

### 2.3 Consider Removing Unused Global Strings

Search for unused String objects that could be removed or made local.

### 2.4 Optimize WebServer Configuration

```cpp
// In wireless.h or webportal.cpp:
// These are the key settings affecting RAM

// Current (likely default):
// - WebServer buffers: ~2KB per client
// - Can handle 5+ concurrent clients by default
// - Each client gets buffer space

// Option: Reduce max clients or buffer size
server.setMaxClients(2);  // Reduce from default
```

---

## More Aggressive Options (If Above Not Enough)

### 3.1 Use BearSSL Memory Optimization
```cpp
// In setup:
configTime(0, 0, "pool.ntp.org");
// Skip WiFi secure connections to save memory
```

### 3.2 Reduce Display Buffer
Currently: 128x64 pixels = 1KB

### 3.3 Use Compressed File System (SPIFFS → LittleFS optimization)

---

## Why 78% is Actually OK

For ESP8266:
- **Safe threshold:** <80% (you're at 77.4% ✓)
- **Warning zone**: 80-85% (start optimizing)
- **Dangerous**: >85% (frequent crashes)

**Your system is fine as-is** for normal operation, but optimizations are good for:
- Stability margin
- Dynamic allocations room
- WebServer handling multiple clients
- Large data operations

---

## Recommended Action Plan

**If you want to reduce below 75%:**

1. **Implement PROGMEM HTML** (+1.5KB savings)
   - Move handleRoot() HTML to F() macro
   - Estimated new: 75-76% ✓

2. **Remove unused globals** (+0.5KB savings)
   - Audit global String variables
   - Make local where possible  
   - Estimated new: 74-75% ✓

3. **That's usually enough!**

**If you still want more:**

4. Reduce WiFi client buffer size
5. Optimize display rendering
6. Use streaming JSON parsing in data logger

---

## Quick Reference: What Uses RAM at Startup

| Item | Size | Type | Fixable? |
|------|------|------|----------|
| WiFi buffers | ~15-20KB | System | Limited |
| Free heap | ~17KB | | N/A |
| Global variables | ~3KB | Code | **Yes** |
| Display buffer | ~1KB | Code | Yes |
| PID/control state | ~2KB | Code | No |
| Fragmentation | ~5KB | System | Limited |
| Reserved | ~20KB | System | No |

---

## Bottom Line

**77.4% is acceptable!** But if you want to optimize:

1. Use PROGMEM (F() macro) for HTML → -1.5KB
2. Audit global Strings → -0.5KB  
3. Result: ~75% (safe margin)

The firmware is production-ready at current levels.
