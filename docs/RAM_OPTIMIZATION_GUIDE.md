# RAM Optimization Guide for KilnController

**Current Status:** 77.4% RAM utilized (63,432 / 81,920 bytes)  
**Target:** Reduce to <70% (≤57,000 bytes)  
**Estimated Savings:** 6-8 KB possible

---

## 1. CRITICAL ISSUES - High Impact, Easy Fix

### 1.1 Large JSON Documents (Biggest Impact: ~4KB saved)

**Problem:** Multiple `DynamicJsonDocument(4096)` allocations
- `kiln_data_logger.cpp` lines 123, 214, 290: `DynamicJsonDocument doc(4096)` (4KB each)
- `program_manager.cpp` line 141: `DynamicJsonDocument doc(2048)` (2KB)
- `program_manager.cpp` lines 35, 87: `DynamicJsonDocument doc(1024)` (1KB each)

**Why:** These are heap allocations every time the function is called. The entire JSON file is loaded into memory.

**Recommended Fixes:**

**Option A: Just Read What You Need (BEST)**
```cpp
// Before (kiln_data_logger.cpp line 123)
DynamicJsonDocument doc(4096);  // Loads entire 4KB file

// After - Stream parse only the array
DeserializationError error = deserializeJson(doc, f, DeserializationOption::Filter(filter));
```

**Option B: Reduce Buffer Size**
```cpp
// Before: 4096 bytes
DynamicJsonDocument doc(4096);

// After: Target actual max size based on your data
// Most data logs won't exceed 1024 bytes
DynamicJsonDocument doc(1024);

// For program list (line 141)
// Before: 2048 bytes  
DynamicJsonDocument doc(2048);

// After: Only need space for one program at a time
DynamicJsonDocument doc(512);
```

**Savings:** 3-4 KB per allocation

---

### 1.2 HTML in PROGMEM (Savings: ~2-3KB)

**Problem:** Large HTML strings stored in RAM via `sendContent()`

**Current Code (webportal.cpp):**
```cpp
server.sendContent(
    "<!DOCTYPE html><html><head>"
    // ... 600+ lines of HTML
    "</body></html>"
);
```

**Recommended Fix - Use PROGMEM:**
```cpp
// 1. Create external file: data/index.html.gz (pre-compressed)
// 2. Or use F() macro for string literals:

void WirelessManager::handleRoot() {
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");
    
    // Use F() macro to store strings in FLASH instead of RAM
    server.sendContent(F("<!DOCTYPE html><html><head>"));
    server.sendContent(F("<meta charset='UTF-8'>"));
    // ... etc
    
    server.sendContent(F("</body></html>"));
}
```

**Alternative - Serve from LittleFS:**
Transfer HTML to `/data/index.html` and serve directly:
```cpp
void WirelessManager::handleRoot() {
    File file = LittleFS.open("/index.html", "r");
    if (file) {
        server.streamFile(file, "text/html");
        file.close();
    }
}
```

**Savings:** 2-3 KB (HTML is only loaded during request, not stored in RAM)

---

### 1.3 Reduce StaticJsonDocument Sizes (Savings: ~1-2KB)

**Problem:** `StaticJsonDocument<2048>` pre-allocates 2KB even if only 256 bytes needed

**Current Issues:**
- `webportal.cpp` line 772: `StaticJsonDocument<2048> doc` (scan networks)
- `WebPortalWiFi.cpp` line 19: `StaticJsonDocument<2048> doc`

**Fix:**
```cpp
// Before
StaticJsonDocument<2048> doc;  // Allocates 2KB stack

// After - Set to realistic max
StaticJsonDocument<512> doc;   // Allocates 512 bytes

// Network scan result (max ~256 bytes per network)
StaticJsonDocument<1024> doc;  // Increased but still safe
```

**Savings:** ~1-1.5 KB for stack allocations

---

## 2. IMPORTANT - Medium Impact

### 2.1 Eliminate Redundant String Allocations

**Problem:** Temporary String objects created unnecessarily

**Example (webportal.cpp, line 779):**
```cpp
// Before - Creates temp String
String scannedSSID = WiFi.SSID(i);

// After - Direct comparison
if (WiFi.SSID(i).equals(config.ssid.c_str())) {
    // ...
}
```

**Savings per occurrence:** 32-64 bytes (stack)

---

### 2.2 Use const char* Instead of String Where Possible

**Issues:**
- `program_manager.cpp` lines 17, 22, 32, 73, 124, 148, 151: String path allocations
- `main.cpp` line 131, 140: String programName allocation

**Fix:**
```cpp
// Before
void handleProgramLoad() {
    String name = server.arg("name");
    extern ProgramManager programManager;
    Program program;
}

// After - Use c_str() directly
void handleProgramLoad() {
    const char* name = server.arg("name").c_str();
    extern ProgramManager programManager;
    Program program;
}
```

**Limitation:** Only works if string lifetime is short (within function scope)

**Savings:** 64-128 bytes per String object

---

### 2.3 Global String Variables

**Current (main.cpp line 131):**
```cpp
String programName = "";
```

**Better:**
```cpp
char programName[64] = "";  // Fixed size, no heap allocation
```

**Savings:** 32-64 bytes per String → char[]

---

## 3. OPTIMIZATION STRATEGY - Implement in Order

### Phase 1 (Easy, ~3-4KB saved):
1. Reduce `DynamicJsonDocument(4096)` to `DynamicJsonDocument(1024)` in kiln_data_logger.cpp
2. Reduce `StaticJsonDocument<2048>` to `StaticJsonDocument<512>` in webportal.cpp

### Phase 2 (Medium, ~2KB saved):
3. Move HTML to PROGMEM using F() macro
4. Convert StaticJsonDocument to 512-1024 bytes where possible
5. Reduce global String to char[64]

### Phase 3 (Advanced, ~1-2KB saved):
6. Implement streaming JSON parsing for data logger
7. Move large HTML completely to LittleFS or PROGMEM

---

## 4. MEMORY BREAKDOWN - What's Using RAM?

| Component | Current | Max | Typical |
|-----------|---------|-----|---------|
| **Program Stack** | ~12KB | 8KB | 8KB |
| **Global Variables** | ~8KB | - | 8KB |
| **Dynamic Allocations** | ~10KB | - | 10KB |
| **WiFi/Web Server Buffers** | ~15KB | - | 15KB |
| **ArduinoJSON Documents** | ~4-8KB | - | 6KB |
| **Heap Fragments** | ~5KB | - | 5KB |
| **Free** | ~17KB | - | 17KB |
| **TOTAL** | **80KB** | - | **~63KB** |

---

## 5. QUICK WINS - Implement Now

### 5.1 Reduce Data Logger JSON Document

**File:** `src/kiln_data_logger.cpp` (lines 123, 214, 290)

Before:
```cpp
DynamicJsonDocument doc(4096);
```

After:
```cpp
DynamicJsonDocument doc(1536);  // 3KB saved
```

**Estimate:** 90% of your data logs won't exceed 1536 bytes

### 5.2 Move HTML to PROGMEM

**File:** `src/webportal.cpp` (handleRoot)

Change all `sendContent()` calls to use F() macro:
```cpp
server.sendContent(F("<style>body{...}</style>"));
```

**Estimate:** 2-3 KB saved during HTML generation

### 5.3 Reduce Scan Result Allocation

**File:** `src/webportal.cpp` (line 772)

Before:
```cpp
StaticJsonDocument<2048> doc;
```

After:
```cpp
StaticJsonDocument<512> doc;
```

**Estimate:** 1.5 KB saved (allocated only during scan)

---

## 6. TARGET CONFIGURATION

With Phase 1 optimizations:
- **JSON Doc 4096 → 1536:** -2.5 KB
- **StaticJSON 2048 → 512:** -1.5 KB
- **Misc reductions:** -1 KB
- **Total savings:** ~5 KB

**New target:** ~58 KB (71% utilization) ✅

---

## 7. ADVANCED - If Still Need More Space

### Compression
- Gzip HTML and serve decompressed on-the-fly
- Use SPI RAM if available on board variant

### Streaming
- Stream JSON parsing instead of full document load
- Process data logger records one at a time

### Caching
- Cache computed results to reduce recalculation
- Implement LRU cache for frequently accessed data

---

## 8. Monitoring Commands

Check actual heap usage:
```cpp
Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
Serial.printf("Heap fragmentation: %u%%\n", ESP.getHeapFragmentation());
```

Add to `setup()` after initialization to monitor baseline usage.

---

## Implementation Priority

**Priority 1 (Do First):**
- [ ] Reduce DynamicJsonDocument sizes in kiln_data_logger.cpp
- [ ] Reduce StaticJsonDocument sizes in webportal.cpp

**Priority 2 (Do Next):**
- [ ] Convert HTML to use F() macro or PROGMEM
- [ ] Replace global String with char[]

**Priority 3 (If Needed):**
- [ ] Implement streaming JSON parsing
- [ ] Move HTML to external file

---

## Expected Outcome

**Before:** 77.4% (63,432 bytes used)  
**After Phase 1:** ~72% (59,000 bytes used)  
**After Phase 2:** ~68% (55,700 bytes used)  
**After Phase 3:** ~65% (53,200 bytes used)

Target: **<70% RAM utilization** ✅
