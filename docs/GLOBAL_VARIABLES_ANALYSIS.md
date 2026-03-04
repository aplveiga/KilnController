# Global Variables Analysis & Minimization Strategy

**Date:** February 26, 2026  
**Purpose:** Identify all global variables across the codebase and provide refactoring strategies to minimize their usage.

---

## Executive Summary

Your kiln controller firmware has **35+ global variables** dispersed across multiple compilation units. While some are necessary at the embedded systems level, many could be encapsulated into classes or structures to improve:

- **Code organization & maintainability**
- **Testability** (easier to inject dependencies)
- **Reusability** (reduced tight coupling)
- **Memory safety** (reduced risk of unintended state modifications)
- **Concurrency safety** (when multi-threading is introduced)

---

## Current Global Variable Inventory

### Category 1: Hardware Abstraction Globals (In `main.cpp`)

| Variable | Type | Purpose | Scope Risk |
|----------|------|---------|-----------|
| `display` | `Adafruit_SSD1306` | OLED screen object | Used in `drawUI()`, accessible everywhere |
| `thermocouple` | `MAX6675` | Temperature sensor | Used in `readTemperatureC()` |
| `kilnPID` | `PID` | PID controller object | Used in `pidCompute()`, modified in setup() |

**Issue:** Hardware drivers requiring initialization should be wrapped in a cohesive control class.

---

### Category 2: PID Control Globals (In `main.cpp`)

| Variable | Type | Purpose | Current Issue |
|----------|------|---------|---|
| `Kp`, `Ki`, `Kd` | `float` | PID tuning gains | Modified from web API, loaded from persistent storage |
| `pidSetpoint` | `double` | Target temperature | Core state variable, modified during program execution |
| `pidInput` | `double` | Current measured temperature | Updated every 1s, used in safety checks |
| `pidOutput` | `double` | PID controller output | Updated by `kilnPID.Compute()`, limited to 0-100 |

**Issue:** PID state tightly coupled to main loop. All other modules access these directly.

---

### Category 3: Program Execution State (In `main.cpp`)

| Variable | Type | Purpose | Issue |
|----------|------|---------|-------|
| `currentProgram` | `Program` | Active program structure | Loaded from persistent storage, used in drawUI(), loop() |
| `programRunning` | `bool` | Execution flag | Modified by start/stop/pause functions |
| `currentSegmentIndex` | `uint8_t` | Current segment number | Advanced by `advanceToNextSegment()` |
| `segmentStartTemp` | `float` | Temperature at segment start | Used for rate calculation |
| `segmentStartMillis` | `unsigned long` | When segment started | Used for elapsed time |
| `holdStartMillis` | `unsigned long` | When hold phase began | Used for hold countdown |
| `pauseMillis` | `unsigned long` | When pause initiated | Used for pause duration |
| `inHold` | `bool` | Program in hold phase | Used in program progression logic |
| `inPause` | `bool` | Program paused | Used in program progression logic |

**Issue:** Program state distributed across 9 variables. No single "program execution context" object. Changes to one variable without updating others causes bugs.

---

### Category 4: SSR/Output Control (In `main.cpp`)

| Variable | Type | Purpose | Issue |
|----------|------|---------|-------|
| `SSR_Status` | `bool` | Current relay state | Queried in UI, set in `updateSSR()` |
| `windowStartMillis` | `unsigned long` | PWM cycle start time | Reset in `startProgram()`, `stopProgram()` |
| `lastSSRChangeTime` | `unsigned long` | Last relay state change | Used for rate limiting |
| `SSR_CHANGE_RATE_LIMIT` | `unsigned long` | Rate limit interval | Modified by `setSSRRateLimit()` |

**Issue:** SSR control distributed. Hard to implement alternative output devices (PWM, contactor, etc.).

---

### Category 5: Persistence & Timing (In `main.cpp`)

| Variable | Type | Purpose | Issue |
|----------|------|---------|-------|
| `lastTempMillis` | `unsigned long` | Last temperature read time | Used to enforce 1s interval |
| `buttonPressStart` | `unsigned long` | Button press timestamp | Used for press duration detection |
| `buttonPrev` | `bool` | Previous button state | Used for edge detection |
| `lastStateSaveMillis` | `unsigned long` | Last state save time | Throttles flash writes |
| `lastSavedSetpoint` | `float` | Last saved setpoint | Used to detect dirty state |
| `stateDirty` | `bool` | State changed flag | Triggers saveState() |
| `sensorFault` | `bool` | Sensor error flag | Used in safety checks and UI |

**Issue:** No coherent "system state" container. State scattered across multiple variables.

---

### Category 6: Web & Wireless Globals (In `webportal.cpp`)

| Variable | Type | Purpose | Accessible From |
|----------|------|---------|---|
| `wirelessManager` | `WirelessManager` | WiFi/Web server manager | Extern in WebPortal*.cpp files |
| `server` | `ESP8266WebServer&` | Reference to web server | Extern in 5+ files (WebPortalPID, WebPortalPrograms, etc.) |
| `programManager` | `ProgramManager` | Program storage manager | Extern in WebPortalPrograms.cpp |

**Issue:** Web server handlers accessed via extern; creates circular dependency between main and WebPortal*.cpp.

---

### Category 7: Data Logger Globals (In `kiln_data_logger.cpp`)

| Variable | Type | Purpose | Pattern |
|----------|------|---------|---------|
| `KilnDataLogger::instance` | `KilnDataLogger` | Singleton storage | Static member |
| `logger` | `KilnDataLogger&` | Global reference | Extern declaration |

**Issue:** Singleton pattern forces global reference. All code accessing logger must include header.

---

## Root Causes

### 1. **No Central State Container**
   - PID, program, SSR, persistence variables live independently
   - No single object representing "kiln system state"

### 2. **Hardware Drivers as Globals**
   - `display`, `thermocouple`, `kilnPID` initialized globally
   - Cannot swap implementations without major refactoring

### 3. **Timing/Synchronization Variables Scattered**
   - Multiple `lastXxxMillis` variables for different update intervals
   - No unified timing manager

### 4. **Weak Dependency Injection**
   - WebPortal files depend on externs for `server`, `wirelessManager`, `programManager`
   - Cannot easily test or run multiple instances

### 5. **Singleton Logger**
   - Data logger uses singleton pattern
   - All code must know about "the" global logger instance

---

## Recommended Refactoring Strategies

### Strategy 1: Create a Central `KilnContext` Structure

**Goal:** Consolidate all kiln state into a single object.

```cpp
// kiln_context.h
#pragma once
#include "types.h"

struct KilnContext {
    // Hardware references
    Adafruit_SSD1306* display;
    MAX6675* thermocouple;
    PID* pidController;
    
    // PID state
    double pidSetpoint;
    double pidInput;
    double pidOutput;
    PIDParams pidTuning;
    
    // Program execution state
    Program currentProgram;
    bool programRunning;
    uint8_t currentSegmentIndex;
    unsigned long segmentStartMillis;
    float segmentStartTemp;
    bool inHold;
    bool inPause;
    unsigned long holdStartMillis;
    unsigned long pauseMillis;
    
    // SSR/Output control
    bool ssrStatus;
    unsigned long windowStartMillis;
    unsigned long lastSSRChangeTime;
    unsigned long ssrRateLimit;
    
    // Safety & status
    bool sensorFault;
    bool stateDirty;
    
    // Timing
    unsigned long lastTempUpdateMs;
    unsigned long lastStateSaveMs;
    float lastSavedSetpoint;
    unsigned long lastButtonPressMs;
    bool lastButtonState;
    
    // Utility methods
    bool isProgramRunning() const { return programRunning; }
    bool isInHoldPhase() const { return inHold; }
    bool isPaused() const { return inPause; }
    float getTemperature() const { return pidInput; }
    float getSetpoint() const { return pidSetpoint; }
};
```

**Refactoring in main.cpp:**

```cpp
// BEFORE (Global variables)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
MAX6675 thermocouple(PIN_MAX_SCK, PIN_MAX_CS, PIN_MAX_SO);
double pidSetpoint = 25.0;
double pidInput = 25.0;
double pidOutput = 0.0;
PID kilnPID(&pidInput, &pidOutput, &pidSetpoint, Kp, Ki, Kd, DIRECT);
Program currentProgram;
bool programRunning = false;
// ... 20+ more globals

// AFTER (Single context object)
KilnContext kiln;

void setup() {
    // Initialize all hardware into context
    kiln.display = new Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
    kiln.thermocouple = new MAX6675(PIN_MAX_SCK, PIN_MAX_CS, PIN_MAX_SO);
    kiln.pidController = new PID(&kiln.pidInput, &kiln.pidOutput, &kiln.pidSetpoint, Kp, Ki, Kd, DIRECT);
    kiln.pidSetpoint = 25.0;
    kiln.pidInput = 25.0;
    kiln.programRunning = false;
    // ... initialization complete
}
```

**Benefits:**
- ✅ All state in one container
- ✅ Easy to pass to functions: `void updateUI(const KilnContext& ctx)`
- ✅ Simple to persist: serialize whole context
- ✅ Testable: inject mock context

---

### Strategy 2: Extract Hardware Drivers into a `HardwareController` Class

**Goal:** Encapsulate display, sensor, relay hardware access.

```cpp
// hardware_controller.h
#pragma once
#include <Adafruit_SSD1306.h>
#include <max6675.h>

class HardwareController {
public:
    HardwareController(int sda, int scl, int mx_sck, int mx_cs, int mx_so, int ssr_pin);
    
    // Display operations
    void displayClear();
    void displayText(int x, int y, const char* text);
    void displayUpdate();
    
    // Sensor operations
    float readTemperature();
    bool hasTemperatureFault() const;
    
    // Output control
    void setSSRState(bool enabled);
    bool getSSRState() const;
    
private:
    Adafruit_SSD1306 display_;
    MAX6675 thermocouple_;
    int ssr_pin_;
    bool ssr_status_;
};
```

**Usage:**
```cpp
// Centralized hardware, swappable implementations
KilnContext kiln;
HardwareController hardware(PIN_SDA, PIN_SCL, PIN_MAX_SCK, PIN_MAX_CS, PIN_MAX_SO, PIN_SSR);

void setup() {
    hardware.displayClear();
}

void loop() {
    float temp = hardware.readTemperature();
    kiln.pidInput = temp;
}
```

**Benefits:**
- ✅ Mock hardware easily for testing
- ✅ Swap display/sensor implementations without touching kiln logic
- ✅ Centralized pin management

---

### Strategy 3: Extract Program Execution into `ProgramExecutor` Class

**Goal:** Isolate program state machine from main loop.

```cpp
// program_executor.h
#pragma once
#include "types.h"

class ProgramExecutor {
public:
    void setProgram(const Program& program);
    void start();
    void stop();
    void pause();
    void update(unsigned long nowMs, float currentTemp);
    
    // Query state
    bool isRunning() const { return running_; }
    bool isHolding() const { return in_hold_; }
    bool isPaused() const { return paused_; }
    float getSetpoint() const { return setpoint_; }
    uint8_t getSegmentIndex() const { return segment_index_; }
    
private:
    Program program_;
    bool running_;
    bool in_hold_;
    bool paused_;
    uint8_t segment_index_;
    float setpoint_;
    unsigned long segment_start_ms_;
    float segment_start_temp_;
    unsigned long hold_start_ms_;
    unsigned long pause_start_ms_;
    
    void advanceSegment(unsigned long nowMs);
    float computeSetpoint(unsigned long nowMs, float current_temp);
};
```

**Usage:**
```cpp
ProgramExecutor executor;

void loop() {
    executor.update(millis(), temperature);
    kiln.setpoint = executor.getSetpoint();
}
```

**Benefits:**
- ✅ Program state machine self-contained
- ✅ Easier to test segment transitions, hold logic
- ✅ Reduced reliance on global `inHold`, `inPause`, etc.

---

### Strategy 4: Create a Unified `TimingManager`

**Goal:** Consolidate all timing logic into a single manager.

```cpp
// timing_manager.h
#pragma once

class TimingManager {
public:
    TimingManager();
    
    // Check if intervals have elapsed
    bool shouldUpdateTemperature(unsigned long nowMs);
    bool shouldSaveState(unsigned long nowMs);
    bool shouldUpdateDisplay(unsigned long nowMs);
    bool shouldScanNetworks(unsigned long nowMs);
    
    // Reset timers (call after action)
    void resetTempTimer(unsigned long nowMs);
    void resetStateTimer(unsigned long nowMs);
    void resetDisplayTimer(unsigned long nowMs);
    
    // Configuration
    void setTempInterval(unsigned long ms);
    void setStateInterval(unsigned long ms);
    
private:
    unsigned long last_temp_update_ms_;
    unsigned long last_state_save_ms_;
    unsigned long last_display_update_ms_;
    unsigned long last_network_scan_ms_;
    
    unsigned long temp_interval_ms_;
    unsigned long state_interval_ms_;
    unsigned long display_interval_ms_;
    unsigned long network_scan_interval_ms_;
};
```

**Usage:**
```cpp
TimingManager timing;

void loop() {
    unsigned long now = millis();
    
    if (timing.shouldUpdateTemperature(now)) {
        readAndUpdateTemperature();
        timing.resetTempTimer(now);
    }
    
    if (timing.shouldSaveState(now)) {
        saveState();
        timing.resetStateTimer(now);
    }
}
```

**Benefits:**
- ✅ All timing logic in one place
- ✅ Easy to adjust intervals globally
- ✅ Reduced number of standalone `lastXxxMillis` variables

---

### Strategy 5: Dependency Injection for Web Handlers

**Goal:** WebPortal handlers receive dependencies instead of using externs.

**Current (Poor Practice):**
```cpp
// WebPortalPID.cpp
extern ESP8266WebServer server;
extern ProgramManager programManager;

void handlePIDSet() {
    server.send(200, "text/json", "{...}");
    programManager.saveProgram(...);
}
```

**Refactored (Better Practice):**
```cpp
// web_handlers.h
class WebHandlers {
public:
    WebHandlers(ESP8266WebServer& srv, ProgramManager& pm, KilnContext& ctx);
    
    void registerRoutes();
    void handlePIDSet();
    void handleProgramList();
    
private:
    ESP8266WebServer& server_;
    ProgramManager& program_manager_;
    KilnContext& context_;
};

// Register in setup:
WebHandlers handlers(wirelessManager.server, programManager, kiln);
handlers.registerRoutes();
```

**Benefits:**
- ✅ No extern declarations
- ✅ Easy to pass mock objects for testing
- ✅ Clearer ownership and dependencies

---

### Strategy 6: Extract Sensor Fault Logic

**Goal:** Encapsulate safety checks in a dedicated class.

```cpp
// safety_monitor.h
#pragma once

class SafetyMonitor {
public:
    void update(float current_temp);
    
    bool hasSensorFault() const { return sensor_fault_; }
    bool isTemperatureExcessive() const { return excessive_temp_; }
    SafetyAlert getAlert() const { return alert_; }
    void clearAlert();
    
private:
    bool sensor_fault_;
    bool excessive_temp_;
    SafetyAlert alert_;
    
    static constexpr float ABSOLUTE_MAX_TEMP = 1200.0;
    static constexpr float SENSOR_MIN_VALID = -50.0;
    static constexpr float SENSOR_MAX_VALID = 1100.0;
};
```

**Benefits:**
- ✅ Consolidates all safety checks in one place
- ✅ Reduces scattered `if (sensorFault)` checks
- ✅ Easier to add/modify safety rules

---

## Migration Path (Phased Approach)

### Phase 1: No-Risk Changes (Keep existing globals, layer abstractions on top)
1. Create `KilnContext` struct empty, start populating gradually
2. Create `TimingManager`, migrate `lastTempMillis`, `lastStateSaveMillis`
3. Create `SafetyMonitor`, migrate `sensorFault` logic

**Effort:** 2-4 hours  
**Risk:** Very low (additive changes)  
**Benefit:** Foundation for Phase 2

---

### Phase 2: Extract Hardware (Medium Risk)
1. Create `HardwareController` wrapping display, thermocouple, SSR
2. Update `readTemperatureC()`, `drawUI()`, `updateSSR()` to use `HardwareController`
3. Keep `kiln` context in globals for now

**Effort:** 4-6 hours  
**Risk:** Medium (refactor UI and sensor reading)  
**Benefit:** Hardware swappable, testable

---

### Phase 3: Extract Program Execution (Medium Risk)
1. Create `ProgramExecutor` class
2. Migrate `currentProgram`, `programRunning`, `inHold`, `inPause`, etc.
3. Update `handleProgramProgress()`, `startProgram()`, `stopProgram()`

**Effort:** 4-6 hours  
**Risk:** Medium (core logic affected)  
**Benefit:** Program state machine testable in isolation

---

### Phase 4: Consolidate Context (Low Risk)
1. Migrate PID state into `KilnContext`
2. Pass `KilnContext&` to all major functions
3. Update main loop

**Effort:** 2-3 hours  
**Risk:** Low (mostly parameter passing)  
**Benefit:** Global count reduced by 50%+

---

### Phase 5: Dependency Injection for Web (Medium Risk)
1. Create `WebHandlers` class accepting injected dependencies
2. Update all WebPortal*.cpp files to use handler methods
3. Remove extern declarations

**Effort:** 3-4 hours  
**Risk:** Medium (web handlers refactored)  
**Benefit:** No more externs; easier to unit test handlers

---

## Quick Wins (1-2 Hour Improvements)

### 1. Group All Timing Variables
```cpp
struct TimingState {
    unsigned long lastTempMs;
    unsigned long lastSaveMs;
    unsigned long lastButtonPressMs;
    unsigned long lastSSRChangeMs;
};

TimingState timing;  // Single global instead of 4+
```

### 2. Group All Program State
```cpp
struct ProgramState {
    Program currentProgram;
    bool running;
    uint8_t segmentIndex;
    unsigned long segmentStartMs;
    float segmentStartTemp;
    bool inHold;
    bool inPause;
    unsigned long holdStartMs;
    unsigned long pauseMs;
};

ProgramState program;  // Single global instead of 9+
```

### 3. Const Correctness on All Getters
```cpp
// BEFORE
float readTemperatureC();  // Hides side effects

// AFTER
float readTemperatureC() const;  // Clearly reads only
void updateTemperatureSensorFaultFlag(float reading);  // Clearly writes
```

### 4. Use std::atomic for Safety-Critical Values
```cpp
#include <atomic>

// Replace:
bool sensorFault = false;

// With:
std::atomic<bool> sensorFault{false};  // Thread-safe reads/writes
```

---

## Current State Summary

| Metric | Count | Severity |
|--------|-------|----------|
| **Total Global Variables** | 35+ | ⚠️ High |
| **Extern Declarations** | 6+ | ⚠️ Medium |
| **Singleton Patterns** | 1 (KilnDataLogger) | ⚠️ Medium |
| **Hardware Drivers Global** | 3 | ⚠️ Medium |
| **Timing Variables Global** | 7+ | ⚠️ Medium |
| **Program State Variables** | 9 | ⚠️ High |

---

## Best Practices Applied

✅ **Encapsulation** — Group related globals into structs/classes  
✅ **Single Responsibility** — Each class manages one concern  
✅ **Dependency Injection** — Pass dependencies, don't use externs  
✅ **Const Correctness** — `const` on read-only operations  
✅ **RAII** — Resource initialization tied to object construction  
✅ **Composition** — Build complex objects from simpler ones  

---

## Testing Benefits

With minimal globals:

```cpp
// BEFORE: Hard to test—must mock 20+ globals
TEST(ProgramExecutorTest, SegmentTransition) {
    // How to test without access to globals?
}

// AFTER: Easy to test—pass objects
TEST(ProgramExecutorTest, SegmentTransition) {
    Program test_prog = createTestProgram();
    ProgramExecutor executor;
    executor.setProgram(test_prog);
    executor.start();
    
    executor.update(1000, 50.0);  // 1 second elapsed, temp=50
    ASSERT_FALSE(executor.isHolding());
    
    executor.update(60000, 100.0);  // 60 seconds, reached target
    ASSERT_TRUE(executor.isHolding());
}
```

---

## Conclusion

Your firmware is functional but has **significant room for architectural improvement**. The recommended path is a **phased 3-4 week migration**:

1. Start with Phase 1 (timing, safety)—very low risk
2. Proceed to Phase 2-3 (hardware, program execution)
3. Finish with Phase 4-5 (context consolidation, web layer)

**Target:** Reduce global variable count from 35+ to **< 5** (core context, managers, logger).

This will make your code **more testable, maintainable, and suitable for future features** like multi-program execution or alternative hardware configurations.

