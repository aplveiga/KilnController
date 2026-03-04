# Practical Global Variables Refactoring Guide

**Purpose:** Step-by-step code examples showing how to refactor your kiln controller from 35+ globals to a well-organized architecture.

---

## Example 1: Creating the Central KilnContext

### Step 1a: Define the Context Header

```cpp
// src/config/kiln_context.h
#pragma once

#include <Arduino.h>
#include "types.h"
#include <Adafruit_SSD1306.h>
#include <max6675.h>
#include <PID_v1.h>

// ============================================================================
// Central Context for All Kiln State
// ============================================================================
// Instead of 35+ global variables scattered throughout, consolidate everything
// into a single, well-organized context structure.

struct KilnContext {
    // ========== Hardware Drivers ==========
    // Note: Stored as pointers since these require dynamic initialization
    Adafruit_SSD1306* display;
    MAX6675* thermocouple;
    PID* pid_controller;
    
    // ========== PID State ==========
    double pid_setpoint;      // Target temperature (°C)
    double pid_input;         // Current measured temperature (°C)
    double pid_output;        // PID output (0-100%)
    
    struct {
        float kp;             // Proportional gain
        float ki;             // Integral gain
        float kd;             // Derivative gain
    } pid_tuning;
    
    // ========== Program Execution State ==========
    Program current_program;
    bool program_running;
    bool program_paused;
    
    struct {
        uint8_t segment_index;
        unsigned long start_ms;
        float start_temp;
        bool in_hold;
        unsigned long hold_start_ms;
        unsigned long pause_start_ms;
    } program_state;
    
    // ========== SSR / Output Control ==========
    struct {
        bool status;                         // Current relay state
        unsigned long window_start_ms;       // PWM cycle start
        unsigned long last_change_time_ms;   // For rate limiting
        unsigned long rate_limit_ms;         // Minimum time between changes
    } ssr;
    
    // ========== Safety & Diagnostic State ==========
    struct {
        bool sensor_fault;
        bool excessive_temperature;
        // Future: Add other safety alerts
    } safety;
    
    // ========== Persistence State ==========
    struct {
        bool dirty;                    // Needs saving to flash
        unsigned long last_save_ms;    // When state was last persisted
        float last_saved_setpoint;     // For dirty detection
    } persistence;
    
    // ========== Timing & Synchronization ==========
    struct {
        unsigned long last_temp_update_ms;
        unsigned long last_display_update_ms;
        unsigned long last_state_save_ms;
        unsigned long last_button_press_ms;
    } timing;
    
    bool last_button_state;
    
    // ========== Utility Methods ==========
    
    // Query state without risk of accidental modification
    bool isProgramRunning() const { 
        return program_running && !program_paused; 
    }
    
    bool isProgramInHold() const { 
        return program_state.in_hold; 
    }
    
    bool isProgramPaused() const { 
        return program_paused; 
    }
    
    float getTemperature() const { 
        return isnan(pid_input) ? 0.0f : (float)pid_input; 
    }
    
    float getSetpoint() const { 
        return (float)pid_setpoint; 
    }
    
    double getPIDOutput() const { 
        return pid_output; 
    }
    
    uint8_t getCurrentSegmentIndex() const { 
        return program_state.segment_index; 
    }
    
    bool hasSSREnabled() const { 
        return ssr.status; 
    }
    
    bool hasSensorFault() const { 
        return safety.sensor_fault || safety.excessive_temperature; 
    }
};

#endif // KILN_CONTEXT_H
```

### Step 1b: Refactor main.cpp to Use Context

**BEFORE (Global Variables):**
```cpp
// Old approach: 35+ scattered globals
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
MAX6675 thermocouple(PIN_MAX_SCK, PIN_MAX_CS, PIN_MAX_SO);

float Kp = 15.5, Ki = 13.1, Kd = 1.20;
double pidSetpoint = 25.0;
double pidInput = 25.0;
double pidOutput = 0.0;
PID kilnPID(&pidInput, &pidOutput, &pidSetpoint, Kp, Ki, Kd, DIRECT);

Program currentProgram;
bool programRunning = false;
uint8_t currentSegmentIndex = 0;
float segmentStartTemp = 25.0;
unsigned long segmentStartMillis = 0;
bool inHold = false;
bool inPause = false;

bool SSR_Status = false;
unsigned long windowStartMillis = 0;
unsigned long lastSSRChangeTime = 0;
unsigned long SSR_CHANGE_RATE_LIMIT = 1000UL;

bool sensorFault = false;
unsigned long lastTempMillis = 0;
unsigned long lastStateSaveMillis = 0;
bool stateDirty = false;

// ... and many more
```

**AFTER (Single Context):**
```cpp
// New approach: Single context object
#include "config/kiln_context.h"

KilnContext kiln;  // ONE GLOBAL instead of 35+

void initializeKilnContext() {
    // Initialize hardware into context
    kiln.display = new Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
    kiln.thermocouple = new MAX6675(PIN_MAX_SCK, PIN_MAX_CS, PIN_MAX_SO);
    
    // Initialize PID parameters
    kiln.pid_tuning.kp = 15.5;
    kiln.pid_tuning.ki = 13.1;
    kiln.pid_tuning.kd = 1.20;
    
    // Initialize PID controller
    kiln.pid_controller = new PID(
        &kiln.pid_input, 
        &kiln.pid_output, 
        &kiln.pid_setpoint,
        kiln.pid_tuning.kp,
        kiln.pid_tuning.ki,
        kiln.pid_tuning.kd,
        DIRECT
    );
    
    // Initialize control state
    kiln.pid_setpoint = 25.0;
    kiln.pid_input = 25.0;
    kiln.pid_output = 0.0;
    
    kiln.program_running = false;
    kiln.program_paused = false;
    kiln.program_state.segment_index = 0;
    kiln.program_state.start_temp = 25.0;
    kiln.program_state.in_hold = false;
    
    kiln.ssr.status = false;
    kiln.ssr.rate_limit_ms = 1000;
    kiln.ssr.window_start_ms = millis();
    kiln.ssr.last_change_time_ms = millis();
    
    kiln.safety.sensor_fault = false;
    kiln.safety.excessive_temperature = false;
    
    kiln.persistence.dirty = false;
    kiln.persistence.last_save_ms = millis();
    kiln.persistence.last_saved_setpoint = 25.0;
    
    kiln.timing.last_temp_update_ms = 0;
    kiln.timing.last_display_update_ms = 0;
    kiln.timing.last_state_save_ms = 0;
    kiln.timing.last_button_press_ms = 0;
    kiln.last_button_state = false;
}

void setup() {
    Serial.begin(115200);
    pinMode(PIN_SSR, OUTPUT);
    digitalWrite(PIN_SSR, LOW);
    
    initializeKilnContext();
    
    kiln.display->begin(SSD1306_SWITCHCAPVCC, 0x3C);
    kiln.display->clearDisplay();
    
    // Read initial temperature
    kiln.pid_input = readTemperatureC();
    if (isnan(kiln.pid_input)) kiln.pid_input = 25.0;
    kiln.pid_setpoint = kiln.pid_input;
    
    // Load persisted state
    loadState(kiln);
    loadPIDValues(kiln);
    
    // Setup PID
    kiln.pid_controller->SetMode(AUTOMATIC);
    kiln.pid_controller->SetOutputLimits(0, 100);
    kiln.pid_controller->SetSampleTime(1000);
    kiln.pid_controller->SetTunings(
        kiln.pid_tuning.kp, 
        kiln.pid_tuning.ki, 
        kiln.pid_tuning.kd
    );
    
    initializePrograms(kiln);
    wirelessManager.begin();
}
```

---

## Example 2: Refactoring Major Functions to Use Context

### Function: `readTemperatureC()`

**BEFORE:**
```cpp
// Modifies global sensorFault
float readTemperatureC() {
    double t = thermocouple.readCelsius();
    if (!isfinite(t) || t < SENSOR_MIN_VALID || t > SENSOR_MAX_VALID) {
        sensorFault = true;
        return NAN;
    }
    sensorFault = false;
    return (float)t;
}
```

**AFTER:**
```cpp
// Pure function: takes context, returns result
float readTemperatureC(KilnContext& ctx) {
    double t = ctx.thermocouple->readCelsius();
    const float SENSOR_MIN_VALID = -50.0;
    const float SENSOR_MAX_VALID = 1100.0;
    
    if (!isfinite(t) || t < SENSOR_MIN_VALID || t > SENSOR_MAX_VALID) {
        ctx.safety.sensor_fault = true;
        return NAN;
    }
    ctx.safety.sensor_fault = false;
    return (float)t;
}
```

---

### Function: `drawUI()`

**BEFORE:**
```cpp
void drawUI() {
    unsigned long now = millis();
    display.clearDisplay();
    
    // Large temperature display (left)
    display.setTextSize(2);
    display.setCursor(0, 0);
    if (isnan(pidInput)) display.print("T: --C");
    else display.printf("T:%4.0fC", pidInput);
    
    // SSR indicator
    display.setTextSize(1);
    display.setCursor(100, 0);
    display.print(SSR_Status ? "SSR" : "   ");
    
    // Setpoint
    display.setTextSize(2);
    display.setCursor(0, 18);
    display.printf("SP:%4.0fC", pidSetpoint);
    
    // Program name
    display.setTextSize(1);
    display.setCursor(70, 36);
    display.print(currentProgram.name);
    
    // Segment info
    display.setCursor(0, 48);
    if (sensorFault) {
        display.print("Sensor Fault");
    } else if (programRunning) {
        Segment &seg = currentProgram.segments[currentSegmentIndex];
        display.printf("Rph:%0.0f %u/%u %s", 
            seg.rate_c_per_hour, 
            currentSegmentIndex+1, 
            currentProgram.seqCount, 
            inPause ? "PAUSE" : inHold ? "HOLD" : "RAMP");
    } else {
        display.printf("Idle %s", WiFi.localIP().toString().c_str());
    }
    
    display.display();
}
```

**AFTER:**
```cpp
void drawUI(const KilnContext& ctx) {
    ctx.display->clearDisplay();
    
    // Large temperature display (left)
    ctx.display->setTextSize(2);
    ctx.display->setCursor(0, 0);
    if (isnan(ctx.pid_input)) {
        ctx.display->print("T: --C");
    } else {
        char buf[16];
        snprintf(buf, sizeof(buf), "T:%4.0fC", ctx.pid_input);
        ctx.display->print(buf);
    }
    
    // SSR indicator
    ctx.display->setTextSize(1);
    ctx.display->setCursor(100, 0);
    ctx.display->print(ctx.ssr.status ? "SSR" : "   ");
    
    // Setpoint
    ctx.display->setTextSize(2);
    ctx.display->setCursor(0, 18);
    char sp_buf[16];
    snprintf(sp_buf, sizeof(sp_buf), "SP:%4.0fC", ctx.pid_setpoint);
    ctx.display->print(sp_buf);
    
    // Program name
    ctx.display->setTextSize(1);
    ctx.display->setCursor(70, 36);
    ctx.display->print(ctx.current_program.name);
    
    // Segment info
    ctx.display->setCursor(0, 48);
    if (ctx.hasSensorFault()) {
        ctx.display->print("Sensor Fault");
    } else if (ctx.isProgramRunning()) {
        const Segment& seg = ctx.current_program.segments[ctx.program_state.segment_index];
        char seg_buf[32];
        snprintf(seg_buf, sizeof(seg_buf), "Rph:%0.0f %u/%u %s",
            seg.rate_c_per_hour,
            ctx.program_state.segment_index + 1,
            ctx.current_program.seqCount,
            ctx.isProgramPaused() ? "PAUSE" : ctx.isProgramInHold() ? "HOLD" : "RAMP"
        );
        ctx.display->print(seg_buf);
    } else {
        char idle_buf[32];
        snprintf(idle_buf, sizeof(idle_buf), "Idle %s", 
            WiFi.localIP().toString().c_str());
        ctx.display->print(idle_buf);
    }
    
    ctx.display->display();
}

// In main loop:
void loop() {
    // ...
    drawUI(kiln);
}
```

**Benefits:**
- ✅ Function signature shows it reads display state
- ✅ Easier to test: pass a mock context
- ✅ Testable without globals initialized

---

### Function: `startProgram()`

**BEFORE:**
```cpp
void startProgram(Program* p) {
    if (p == nullptr) {
        Serial.println("[Kiln] Error: Program pointer is null");
        return;
    }
    
    if (p->seqCount == 0) {
        Serial.printf("[Kiln] Error: Program '%s' has no segments\n", p->name);
        return;
    }
    
    currentProgram = *p;
    programRunning = true;
    currentSegmentIndex = 0;
    segmentStartTemp = isnan(pidInput) ? pidSetpoint : pidInput;
    segmentStartMillis = millis();
    windowStartMillis = millis();
    inHold = false;
    inPause = false;
    stateDirty = true;
    
    Serial.printf("[Kiln] Program STARTED: %s (%d segments), setpoint=%.1f\n", 
        currentProgram.name, currentProgram.seqCount, pidSetpoint);
}
```

**AFTER:**
```cpp
bool startProgram(KilnContext& ctx, const Program& program, unsigned long nowMs) {
    // Validation
    if (program.seqCount == 0) {
        Serial.printf("[Kiln] Error: Program '%s' has no segments\n", program.name);
        return false;
    }
    
    // Update context state
    ctx.current_program = program;
    ctx.program_running = true;
    ctx.program_paused = false;
    
    ctx.program_state.segment_index = 0;
    ctx.program_state.start_temp = isnan(ctx.pid_input) ? ctx.pid_setpoint : ctx.pid_input;
    ctx.program_state.start_ms = nowMs;
    ctx.program_state.in_hold = false;
    
    ctx.ssr.window_start_ms = nowMs;
    ctx.persistence.dirty = true;
    
    Serial.printf("[Kiln] Program STARTED: %s (%d segments), setpoint=%.1f\n",
        ctx.current_program.name,
        ctx.current_program.seqCount,
        ctx.pid_setpoint);
    
    return true;
}

// Usage in loop:
void loop() {
    unsigned long now = millis();
    
    if (button_pressed) {
        if (kiln.isProgramRunning()) {
            pauseProgram(kiln, now);
        } else {
            startProgram(kiln, kiln.current_program, now);
        }
    }
}
```

---

## Example 3: TimingManager Class (Quick Win)

### Consolidate All Timing Variables

```cpp
// src/services/timing_manager.h
#pragma once

#include <Arduino.h>

class TimingManager {
public:
    // Check if intervals have elapsed
    bool shouldUpdateTemperature(unsigned long nowMs) {
        return (nowMs - last_temp_update_ms_) >= TEMP_INTERVAL_MS;
    }
    
    bool shouldSaveState(unsigned long nowMs) {
        return (nowMs - last_state_save_ms_) >= STATE_SAVE_INTERVAL_MS;
    }
    
    bool shouldUpdateDisplay(unsigned long nowMs) {
        return (nowMs - last_display_update_ms_) >= DISPLAY_INTERVAL_MS;
    }
    
    bool shouldScanNetworks(unsigned long nowMs) {
        return (nowMs - last_network_scan_ms_) >= NETWORK_SCAN_INTERVAL_MS;
    }
    
    // Reset timers after action
    void resetTempUpdate(unsigned long nowMs) {
        last_temp_update_ms_ = nowMs;
    }
    
    void resetStateSave(unsigned long nowMs) {
        last_state_save_ms_ = nowMs;
    }
    
    void resetDisplayUpdate(unsigned long nowMs) {
        last_display_update_ms_ = nowMs;
    }
    
    void resetNetworkScan(unsigned long nowMs) {
        last_network_scan_ms_ = nowMs;
    }
    
    // Reconfigurable intervals
    void setTempInterval(unsigned long ms) { TEMP_INTERVAL_MS = ms; }
    void setStateSaveInterval(unsigned long ms) { STATE_SAVE_INTERVAL_MS = ms; }

private:
    unsigned long last_temp_update_ms_ = 0;
    unsigned long last_state_save_ms_ = 0;
    unsigned long last_display_update_ms_ = 0;
    unsigned long last_network_scan_ms_ = 0;
    
    unsigned long TEMP_INTERVAL_MS = 1000;           // 1 second
    unsigned long STATE_SAVE_INTERVAL_MS = 10000;    // 10 seconds
    unsigned long DISPLAY_INTERVAL_MS = 100;         // 100ms (for smooth updates)
    unsigned long NETWORK_SCAN_INTERVAL_MS = 30000;  // 30 seconds
};
```

**Usage in main loop:**
```cpp
TimingManager timing;
KilnContext kiln;

void loop() {
    unsigned long now = millis();
    
    // Temperature update
    if (timing.shouldUpdateTemperature(now)) {
        float temp = readTemperatureC(kiln);
        if (!isnan(temp)) kiln.pid_input = temp;
        
        kiln.pid_controller->Compute();
        updateSSR(kiln, now);
        handleProgramProgress(kiln, now);
        
        timing.resetTempUpdate(now);
    }
    
    // State persistence
    if (timing.shouldSaveState(now)) {
        if (kiln.persistence.dirty) {
            saveState(kiln);
            timing.resetStateSave(now);
        }
    }
    
    // UI update (can be faster than temperature updates)
    if (timing.shouldUpdateDisplay(now)) {
        drawUI(kiln);
        timing.resetDisplayUpdate(now);
    }
    
    // Network operations
    wirelessManager.handleWiFi();
    
    delay(10);  // Prevent CPU busy-loop
}
```

**Benefits:**
- ✅ All timing logic in one place
- ✅ Easy to adjust intervals globally
- ✅ No scattered `lastXxxMillis` variables
- ✅ Consolidates 7+ variables into 1 object

---

## Example 4: SafetyMonitor Class

```cpp
// src/services/safety_monitor.h
#pragma once

#include <Arduino.h>
#include "config/types.h"

class SafetyMonitor {
public:
    SafetyMonitor() : sensor_fault_(false), excessive_temp_(false), 
                      current_alert_(SafetyAlert::NONE) {}
    
    // Update safety status based on current temperature
    void update(float temperature) {
        // Check for sensor fault (invalid reading)
        if (!isfinite(temperature) || 
            temperature < SENSOR_MIN_VALID || 
            temperature > SENSOR_MAX_VALID) {
            sensor_fault_ = true;
            current_alert_ = SafetyAlert::SENSOR_FAULT;
        } else {
            sensor_fault_ = false;
        }
        
        // Check for excessive temperature
        if (!sensor_fault_ && temperature > ABSOLUTE_MAX_TEMP) {
            excessive_temp_ = true;
            current_alert_ = SafetyAlert::OVERHEAT;
        } else if (!sensor_fault_) {
            excessive_temp_ = false;
            if (current_alert_ == SafetyAlert::OVERHEAT) {
                current_alert_ = SafetyAlert::NONE;
            }
        }
    }
    
    // Query safety state
    bool hasSensorFault() const { return sensor_fault_; }
    bool hasExcessiveTemperature() const { return excessive_temp_; }
    SafetyAlert getCurrentAlert() const { return current_alert_; }
    bool isSystemSafe() const { return !sensor_fault_ && !excessive_temp_; }
    
    // Clear alerts manually (e.g., after user acknowledges)
    void clearAlert() {
        if (current_alert_ != SafetyAlert::SENSOR_FAULT) {
            current_alert_ = SafetyAlert::NONE;
        }
    }
    
    const char* getAlertDescription() const {
        switch (current_alert_) {
            case SafetyAlert::SENSOR_FAULT:     return "Sensor Fault";
            case SafetyAlert::OVERHEAT:         return "Overheat";
            case SafetyAlert::RATE_EXCEEDED:    return "Rate Exceeded";
            case SafetyAlert::PROGRAM_ERROR:    return "Program Error";
            case SafetyAlert::NONE:
            default:                            return "No Alert";
        }
    }

private:
    bool sensor_fault_;
    bool excessive_temp_;
    SafetyAlert current_alert_;
    
    static constexpr float ABSOLUTE_MAX_TEMP = 1200.0;
    static constexpr float SENSOR_MIN_VALID = -50.0;
    static constexpr float SENSOR_MAX_VALID = 1100.0;
};
```

**Usage:**
```cpp
SafetyMonitor safety;
KilnContext kiln;

void loop() {
    // ...
    
    // After temperature read
    kiln.pid_input = readTemperatureC(kiln);
    safety.update(kiln.pid_input);
    
    // Check safety globally
    if (!safety.isSystemSafe()) {
        // Emergency stop
        updateSSR(kiln, now, false);  // Force SSR off
        Serial.printf("[SAFETY] Alert: %s\n", safety.getAlertDescription());
    }
}
```

**Benefits:**
- ✅ All safety logic in one class
- ✅ Reduces scattered safety checks
- ✅ Easy to add/modify safety rules
- ✅ Better error messaging

---

## Example 5: Main Loop Refactored

**BEFORE (With 35+ globals):**
```cpp
void loop() {
    unsigned long now = millis();
    
    // Handle WiFi and OTA
    wirelessManager.handleWiFi();
    
    // Button handling with global state
    bool btn = getButtonStateDebounced();
    if (btn != buttonPrev) {
        if (btn) {
            buttonPressStart = now;
            Serial.println("[Kiln] Button PRESSED");
        } else {
            unsigned long held = now - buttonPressStart;
            if (held > LONG_PRESS_MS) {
                buttonActionCycleProgram();
            } else {
                buttonActionStartStop();
            }
        }
        buttonPrev = btn;
    }
    
    // Temperature read + PID compute at interval
    if (now - lastTempMillis >= TEMP_INTERVAL_MS) {
        lastTempMillis = now;
        float t = readTemperatureC();
        if (!isnan(t)) pidInput = t;
        else pidInput = NAN;
        
        handleProgramProgress(now);
        pidCompute(now);
        
        if (pidSetpoint > ABSOLUTE_MAX_TEMP) 
            pidSetpoint = ABSOLUTE_MAX_TEMP;
        
        updateSSR(now);
        drawUI();
        
        Serial.printf("T=%.2f SP=%.2f Out=%.1f Prog=%s Seg=%u Fault=%d\n",
            isnan(pidInput)?0.0f:pidInput, pidSetpoint, pidOutput, 
            currentProgram.name, currentSegmentIndex+1, sensorFault?1:0);
        
        if (fabs(pidSetpoint - lastSavedSetpoint) > 0.5f) 
            stateDirty = true;
        
        if (stateDirty && (now - lastStateSaveMillis >= STATE_SAVE_INTERVAL_MS)) {
            saveState();
        }
    }
    
    delay(10);
}
```

**AFTER (Clean with context + managers):**
```cpp
Timing timing;
SafetyMonitor safety;
KilnContext kiln;

void loop() {
    unsigned long now = millis();
    
    // Network operations
    wirelessManager.handleWiFi();
    
    // Button handling
    handleButtonInput(kiln, now, timing);
    
    // Core kiln updates at 1Hz
    if (timing.shouldUpdateTemperature(now)) {
        // Read temperature
        float temp = readTemperatureC(kiln);
        if (!isnan(temp)) kiln.pid_input = temp;
        
        // Safety monitoring
        safety.update(kiln.pid_input);
        if (!safety.isSystemSafe()) {
            updateSSR(kiln, now, false);  // Emergency stop
        } else {
            // Program progression
            handleProgramProgress(kiln, now);
            
            // PID compute
            kiln.pid_controller->Compute();
            
            // Safety limit
            if (kiln.pid_setpoint > SafetyMonitor::MAX_TEMP) {
                kiln.pid_setpoint = SafetyMonitor::MAX_TEMP;
            }
            
            // Output control
            updateSSR(kiln, now);
        }
        
        // Diagnostics
        logKilnState(kiln);
        
        // Persistence
        if (fabs(kiln.pid_setpoint - kiln.persistence.last_saved_setpoint) > 0.5f) {
            kiln.persistence.dirty = true;
        }
        
        timing.resetTempUpdate(now);
    }
    
    // UI update (can be independent of control loop)
    if (timing.shouldUpdateDisplay(now)) {
        drawUI(kiln);
        timing.resetDisplayUpdate(now);
    }
    
    // State persistence (independent timing)
    if (timing.shouldSaveState(now)) {
        if (kiln.persistence.dirty) {
            saveState(kiln);
            timing.resetStateSave(now);
        }
    }
    
    delay(10);
}

void logKilnState(const KilnContext& ctx) {
    Serial.printf("[Kiln] T=%.2fC SP=%.2fC Out=%.1f%% Prog=%s Seg=%u/%u Status=%s\n",
        ctx.getTemperature(),
        ctx.getSetpoint(),
        ctx.getPIDOutput(),
        ctx.current_program.name,
        ctx.getCurrentSegmentIndex() + 1,
        ctx.current_program.seqCount,
        ctx.hasSensorFault() ? "FAULT" : 
        (ctx.isProgramRunning() ? (ctx.isProgramInHold() ? "HOLD" : "RAMP") : "IDLE")
    );
}
```

**Benefits:**
- ✅ Clear separation of concerns
- ✅ Easy to understand control flow
- ✅ Modular update timings
- ✅ Independent safety monitoring

---

## Step-by-Step Migration Plan

### Week 1: Preparation
- [ ] Create `kiln_context.h` header
- [ ] Create `timing_manager.h` and `safety_monitor.h`
- [ ] Compile (don't change main.cpp yet)
- [ ] Commit to git

### Week 2: Gradual Refactoring
- [ ] Refactor temperature reading: `readTemperatureC(kiln)`
- [ ] Refactor UI: `drawUI(kiln)`
- [ ] Refactor SSR control: `updateSSR(kiln, now)`
- [ ] Add logging: `logKilnState(kiln)`
- [ ] Test each change

### Week 3: Core Logic
- [ ] Refactor program control: `startProgram(kiln, ...)`, `stopProgram(kiln, ...)`
- [ ] Refactor program progression: `handleProgramProgress(kiln, now)`
- [ ] Refactor button handling: `handleButtonInput(kiln, now, ...)`
- [ ] Refactor PID tuning API

### Week 4: Web & Integration
- [ ] Dependency injection for web handlers
- [ ] Update web API endpoints to use context
- [ ] Final testing
- [ ] Documentation

---

## Summary: Globals Before vs. After

| Category | Before | After |
|----------|--------|-------|
| **Core control globals** | 35+ | 1 (kiln context) |
| **Timing variables** | 7+ | 1 (TimingManager) |
| **Safety checks** | Scattered | 1 (SafetyMonitor) |
| **Extern declarations** | 6+ | 0 |
| **Functions with context param** | 0 | 15+ |
| **Testable components** | 2 | 8+ |

**Result:** From 35+ fragile globals to ~3 well-organized context objects and managers.

