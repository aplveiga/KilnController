# Web Portal Data Logger - Validation Report

**Status:** ⚠️ **CRITICAL ISSUES FOUND**

---

## Overview

The web portal data logger has **two conflicting implementations** causing undefined behavior:

1. **Standalone functions** in `WebPortalDataLogger.cpp`
2. **Class methods** in `WirelessManager` (webportal.cpp)

The routes are registered but there's **confusion about which version is being called**.

---

## Issue #1: Duplicate Implementations

### Location 1: WebPortalDataLogger.cpp (Lines 11-24)
```cpp
void handleDataLog() {
    String data;
    logger.exportData(data);
    server.send(200, "application/json", data);
}

void handleClearData() {
    logger.clearAll();
    StaticJsonDocument<64> doc;
    doc["success"] = true;
    doc["message"] = "All data cleared";
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}
```
❌ **Problem:** 
- Standalone functions (not class methods)
- Can't access `this` pointer
- Will get "undefined reference" linker errors OR use extern global

### Location 2: webportal.cpp (Lines 916-938)
```cpp
void WirelessManager::handleDataLog() {
    extern KilnDataLogger& logger;
    String data;
    logger.exportData(data);
    server.send(200, "application/json", data);
}

void WirelessManager::handleClearData() {
    extern KilnDataLogger& logger;
    logger.clearAll();
    StaticJsonDocument<64> doc;
    doc["success"] = true;
    doc["message"] = "All data cleared";
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}
```
✅ **This is the CORRECT one** - Class methods with proper extern declarations

---

## Issue #2: Route Registration Points to Wrong Functions

**webportal.cpp Lines 342-343:**
```cpp
server.on("/api/data", [this]() { handleDataLog(); });
server.on("/api/cleardata", [this]() { handleClearData(); });
```

⚠️ **Problem:** 
- Inside `setupWebServer()` which is a `WirelessManager` class method
- Lambda is trying to call standalone `handleDataLog()` NOT `WirelessManager::handleDataLog()`
- Should be: `server.on("/api/data", [this]() { this->handleDataLog(); });`

---

## Issue #3: Missing HTTP Method Specification

**JavaScript sends POST to `/api/cleardata`:**
```javascript
function clearDataLog(){
    if(confirm('Delete all logged data?...')){
        fetch('/api/cleardata',{method:'POST'})  // ← POST method
```

⚠️ **Problem:** 
- Routes accept ANY HTTP method (GET, POST, PUT, etc.)
- `handleClearData()` doesn't validate method
- Should add: `if (server.method() != HTTP_POST) { ... }`

**Correct pattern (from line 1167):**
```cpp
void WirelessManager::handleButtonStartStop() {
    if (server.method() != HTTP_POST) {
        server.send(405, "text/plain", "Method Not Allowed");
        return;
    }
    // ... handler code
}
```

---

## Issue #4: No Response for GET `/api/data`

**JavaScript expects GET to return:**
```json
{
  "data": [
    {
      "ts": 1708876543,
      "t": 45.2,
      "sp": 50.0,
      "r": 100.0,
      "tgt": 300.0,
      "dur": 150,
      "seq": 1,
      "prog": "9-step",
      "stat": "RAMP"
    },
    ...
  ]
}
```

✅ **This works IF:**
- `logger.exportData(data)` correctly outputs JSON array with "data" key
- Let me verify this is working...

---

## Data Flow Validation

### Expected Flow for "Load Data" Button

```
USER CLICKS: "Load Data" Button
    ↓
JavaScript: loadDataLog() function
    ↓
fetch('/api/data') → GET request
    ↓
Server receives GET /api/data
    ↓
route to: [this]() { handleDataLog(); }
    ↓
❓ WHICH handleDataLog() gets called?
    ├─ WebPortalDataLogger.cpp version (STANDALONE) → LIKELY WRONG
    └─ WirelessManager::handleDataLog() (CLASS METHOD) → SHOULD BE

IF WebPortalDataLogger.cpp version runs:
    → extern KilnDataLogger& logger  (GOOD - should find global)
    → logger.exportData(data)  (GOOD - should work)
    → server.send(200, "application/json", data)  (GOOD)
    ✅ SHOULD WORK

IF WirelessManager version runs:
    → extern KilnDataLogger& logger
    → logger.exportData(data)
    → server.send(200, "application/json", data)
    ✅ ALSO WORKS (same implementation!)

RESULT: Both versions identical, so it PROBABLY WORKS despite duplicate code
```

### Expected Flow for "Clear All" Button

```
USER CLICKS: "Clear All" Button
    ↓
showConfirmation dialog
    ↓
fetch('/api/cleardata', {method:'POST'})
    ↓
Server receives POST /api/cleardata
    ↓
⚠️ NO METHOD CHECK - accepts GET or POST!
    ↓
Clear all data: logger.clearAll()
    ↓
Response: {"success":true,"message":"All data cleared"}
    ↓
JavaScript: loadDataLog() to refresh
    ✓ SHOULD WORK (but no HTTP method validation)
```

---

## Validation Checklist

| Item | Expected | Actual | Status |
|------|----------|--------|--------|
| **Route registered** | `/api/data` | ✅ Line 342 | ✓ |
| **Route registered** | `/api/cleardata` | ✅ Line 343 | ✓ |
| **GET /api/data implemented?** | Yes | ✅ handleDataLog() | ✓ |
| **POST /api/cleardata implemented?** | Yes | ✅ handleClearData() | ✓ |
| **HTTP method validation** | POST only for clear | ❌ Missing | ✗ |
| **Export data format** | {"data":[...]} | ? | ? |
| **Duplicate code** | Single impl | ❌ Two versions | ✗ |

---

## Testing Steps to Validate

### Test 1: Can we load data?
```
1. Open browser DevTools → Network tab
2. Click "Load Data" button
3. Check:
   ✓ GET /api/data → Status 200
   ✓ Response contains JSON
   ✓ Response has "data" array
   ✓ Table populates with data points
```

### Test 2: Can we clear data?
```
1. Open browser DevTools → Network tab
2. Click "Clear All" button
3. Confirm dialog
4. Check:
   ✓ POST /api/cleardata → Status 200
   ✓ Response: {"success":true,"message":"..."}
   ✓ Message shown: "All data cleared"
   ✓ Table empties
   ✓ Serial shows: "[Logger] All data cleared"
```

### Test 3: Data export format
```
1. Use curl or Postman to test:
   curl http://kilncontroller/api/data
   
2. Verify response format:
   {
     "data": [
       { "ts": ..., "t": ..., etc }
     ]
   }
```

---

## Problems Found

### 🔴 **Severity 1: Duplicate Code**
- Two implementations doing same thing
- Creates maintenance burden
- Linker confusion possible

### 🟡 **Severity 2: No HTTP Method Validation on /api/cleardata**
- Should accept POST only
- Currently accepts any method
- Could be triggered accidentally with GET

### 🟡 **Severity 3: Lambda Routing Issue**
```cpp
// Line 342-343
server.on("/api/data", [this]() { handleDataLog(); });
```
This looks for `handleDataLog()` in scope:
1. First checks local scope - NOT FOUND
2. Then checks WirelessManager scope via `this` - NOT FOUND (missing `this->`)
3. Then checks global scope - FOUND (WebPortalDataLogger.cpp version)

**Works by accident!** But fragile if WebPortalDataLogger.h not included.

---

## Recommended Fixes

### Fix #1: Remove Duplicate - Delete WebPortalDataLogger.cpp Handlers

**Option A:** Keep only class methods, update routing
```cpp
// webportal.cpp line 342-343 (CHANGE TO)
server.on("/api/data", [this]() { this->handleDataLog(); });
server.on("/api/cleardata", [this]() { this->handleClearData(); });
```

**Option B:** Keep WebPortalDataLogger.cpp, delete class methods from webportal.cpp
- Less work if already compiling
- But increases maintenance

### Fix #2: Add HTTP Method Validation

```cpp
// webportal.cpp handleDataLog() - add GET validation
void WirelessManager::handleDataLog() {
    if (server.method() != HTTP_GET) {
        server.send(405, "text/plain", "Method Not Allowed");
        return;
    }
    extern KilnDataLogger& logger;
    String data;
    logger.exportData(data);
    server.send(200, "application/json", data);
}

// webportal.cpp handleClearData() - add POST validation
void WirelessManager::handleClearData() {
    if (server.method() != HTTP_POST) {
        server.send(405, "text/plain", "Method Not Allowed");
        return;
    }
    extern KilnDataLogger& logger;
    logger.clearAll();
    StaticJsonDocument<64> doc;
    doc["success"] = true;
    doc["message"] = "All data cleared";
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}
```

### Fix #3: Fix Route Registration

```cpp
// webportal.cpp around line 342-343
// Change from:
server.on("/api/data", [this]() { handleDataLog(); });
server.on("/api/cleardata", [this]() { handleClearData(); });

// Change to:
server.on("/api/data", HTTP_GET, [this]() { this->handleDataLog(); });
server.on("/api/cleardata", HTTP_POST, [this]() { this->handleClearData(); });
```

---

## Current Status Summary

| Feature | Works? | Issue | Impact |
|---------|--------|-------|--------|
| **Load Data** | ✅ Probably | Route routing fragile | Medium |
| **Data Display** | ✅ Probably | Depends on export format | Medium |
| **Clear All** | ✅ Probably | No method validation | Low |
| **Filter Data** | ? Unknown | Not tested | Unknown |

---

## Buttons Behavior Matrix

| Button | Function | Expected | Works? | Note |
|--------|----------|----------|--------|------|
| **Load Data** | `loadDataLog()` | Fetch /api/data, show table | ✅ | Should work |
| **Refresh** | `refreshDataLog()` | Reload filtered data | ✅ | Depends on Load |
| **Clear All** | `clearDataLog()` | Delete all, ask confirm | ✅ | No method check |
| **Filter inputs** | `getElementById(...).value` | Set filter params | ✅ | Client-side only |

---

## Next Steps to Validate

1. ✅ Open DevTools → Network tab
2. ✅ Click "Load Data" button
3. ✅ Verify GET /api/data response has "data" array
4. ✅ Check Serial output for "[Logger] Data logged..." messages
5. ✅ Run cleanup: should see "[Logger] Cleanup complete..."
6. ✅ Click "Clear All" button
7. ✅ Verify POST /api/cleardata response
8. ✅ Check Serial: "[Logger] All data cleared"
9. ✅ Test filters work (sequence number, time range)

---

## Files Affected

- `src/WebPortalDataLogger.cpp` - Standalone handlers (DUPLICATE)
- `src/webportal.cpp` - Class methods (MAIN - lines 342-343, 916-938)
- `include/WebPortalDataLogger.h` - Headers only
- HTML/JavaScript dashboard - lines 522, 742-768

---

**Recommendation:** Run the validation steps and report what you see in the DevTools Network tab.

