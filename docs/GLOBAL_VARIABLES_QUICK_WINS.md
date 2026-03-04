# Global Variables Quick Wins & Architecture Comparison

**Purpose:** Quick-win improvements you can implement immediately, plus visual comparison of before/after architecture.

---

## Quick Wins (1-2 Hours Each)

### Quick Win #1: Group Timing Variables

**Effort:** 15 minutes | **Risk:** Minimal | **Impact:** Reduces 7 globals → 1

**Current State:**
```cpp
unsigned long lastTempMillis = 0;
unsigned long buttonPressStart = 0;
unsigned long lastStateSaveMillis = 0;
unsigned long segmentStartMillis = 0;
unsigned long holdStartMillis = 0;
unsigned long pauseMillis = 0;
unsigned long windowStartMillis = 0;
unsigned long lastSSRChangeTime = 0;
```

**Refactored:**
```cpp
struct TimingState {
    unsigned long lastTempMs;
    unsigned long buttonPressStartMs;
    unsigned long lastStateSaveMs;
    unsigned long segmentStartMs;
    unsigned long holdStartMs;
    unsigned long pauseMs;
    unsigned long windowStartMs;
    unsigned long lastSSRChangeMs;
};

TimingState timing;  // One global instead of 8
```

**Changes in code:**
```cpp
// Before:
if (now - lastTempMillis >= TEMP_INTERVAL_MS) {
    lastTempMillis = now;
    // ...
}

// After:
if (now - timing.lastTempMs >= TEMP_INTERVAL_MS) {
    timing.lastTempMs = now;
    // ...
}
```

---

### Quick Win #2: Group Program Execution State

**Effort:** 20 minutes | **Risk:** Low | **Impact:** Reduces 9 globals → 1

**Current State:**
```cpp
Program currentProgram;
bool programRunning = false;
uint8_t currentSegmentIndex = 0;
float segmentStartTemp = 25.0;
unsigned long segmentStartMillis = 0;
unsigned long holdStartMillis = 0;
unsigned long pauseMillis = 0;
bool inHold = false;
bool inPause = false;
```

**Refactored:**
```cpp
struct ProgramExecutionState {
    Program current;
    bool running;
    bool paused;
    
    struct {
        uint8_t index;
        unsigned long startMs;
        float startTemp;
    } segment;
    
    struct {
        bool active;
        unsigned long startMs;
    } hold;
    
    unsigned long pauseStartMs;
    
    // Convenience methods
    bool isRunning() const { return running && !paused; }
    bool isHolding() const { return hold.active; }
    uint8_t segmentIndex() const { return segment.index; }
};

ProgramExecutionState program;  // One global instead of 9
```

---

### Quick Win #3: Group Safety State

**Effort:** 10 minutes | **Risk:** Minimal | **Impact:** Reduces 2 globals → 1

**Current State:**
```cpp
bool sensorFault = false;
// Future: Add other safety flags
```

**Refactored:**
```cpp
struct SafetyState {
    bool sensorFault;
    bool temperatureExcessive;
    bool rateLimitExceeded;
    
    bool isSystemSafe() const {
        return !sensorFault && !temperatureExcessive;
    }
};

SafetyState safety;
```

---

### Quick Win #4: Group Persistence State

**Effort:** 10 minutes | **Risk:** Minimal | **Impact:** Reduces 3 globals → 1

**Current State:**
```cpp
bool stateDirty = false;
unsigned long lastStateSaveMillis = 0;
float lastSavedSetpoint = 0.0;
```

**Refactored:**
```cpp
struct PersistenceState {
    bool dirty;
    unsigned long lastSaveMs;
    float lastSavedSetpoint;
    
    bool needsSave() const { return dirty; }
    bool significantChange(float newSetpoint) const {
        return fabs(newSetpoint - lastSavedSetpoint) > 0.5f;
    }
};

PersistenceState persistence;
```

---

### Quick Win #5: Group SSR Control State

**Effort:** 15 minutes | **Risk:** Low | **Impact:** Reduces 4 globals → 1

**Current State:**
```cpp
bool SSR_Status = false;
unsigned long windowStartMillis = 0;
unsigned long lastSSRChangeTime = 0;
unsigned long SSR_CHANGE_RATE_LIMIT = 1000UL;
```

**Refactored:**
```cpp
struct SSRControlState {
    bool status;                    // Current relay state
    unsigned long windowStartMs;    // PWM cycle start
    unsigned long lastChangeMs;     // For rate limiting
    unsigned long rateLimitMs;      // Min time between changes
    
    bool isOn() const { return status; }
    void turnOn() { status = true; }
    void turnOff() { status = false; }
};

SSRControlState ssr;
```

---

### Quick Win #6: Add Method to Check if State Update Needed

**Effort:** 5 minutes | **Risk:** None | **Impact:** Cleaner logic

```cpp
// Instead of checking multiple conditions scattered in code:
if (fabs(pidSetpoint - lastSavedSetpoint) > 0.5f) {
    stateDirty = true;
}

if (stateDirty && (now - lastStateSaveMillis >= STATE_SAVE_INTERVAL_MS)) {
    saveState();
}

// Consolidate:
bool shouldSaveState(unsigned long nowMs) {
    return persistence.needsSave() && 
           (nowMs - persistence.lastSaveMs >= 10000);
}

if (shouldSaveState(now)) {
    saveState(kiln);
    persistence.dirty = false;
    persistence.lastSaveMs = now;
}
```

---

### Quick Win #7: Make PID Parameters a Struct

**Effort:** 10 minutes | **Risk:** Low | **Impact:** Reduces 3 globals → 1, improves organization

**Current State:**
```cpp
float Kp = 15.5;
float Ki = 13.1;
float Kd = 1.20;

// Modified from web:
void setPIDValues(float p, float i, float d) {
    Kp = p;
    Ki = i;
    Kd = d;
    kilnPID.SetTunings(Kp, Ki, Kd);
};
```

**Refactored:**
```cpp
struct PIDTuning {
    float kp;
    float ki;
    float kd;
    
    PIDTuning(float p = 15.5f, float i = 13.1f, float d = 1.20f)
        : kp(p), ki(i), kd(d) {}
    
    void apply(PID& pid_controller) {
        pid_controller.SetTunings(kp, ki, kd);
    }
};

PIDTuning pidTuning;  // One struct instead of 3

// Usage:
pidTuning.apply(*kiln.pid_controller);
```

---

## Cumulative Impact of Quick Wins

```
Before quick wins:
├── display (global)
├── thermocouple (global)
├── kilnPID (global)
├── pidSetpoint, pidInput, pidOutput (3 globals)
├── Kp, Ki, Kd (3 globals)
├── currentProgram, programRunning, currentSegmentIndex, ... (9 globals)
├── SSR_Status, windowStartMillis, lastSSRChangeTime, SSR_CHANGE_RATE_LIMIT (4 globals)
├── sensorFault (global)
├── stateDirty, lastStateSaveMillis, lastSavedSetpoint (3 globals)
├── lastTempMillis, buttonPressStart, ... (7+ globals)
└── Total: 35+ separate global variables

After quick wins:
├── kiln.display
├── kiln.thermocouple
├── kiln.pid_controller
├── kiln.pid_setpoint, kiln.pid_input, kiln.pid_output
├── kiln.pid_tuning (Kp, Ki, Kd grouped)
├── program (timing, safety, persistence, ssr all grouped)
└── Total: ~8-10 organized globals in cohesive structures
```

---

## Architecture Comparison

### Current Architecture (Problematic)

```
┌─────────────────────────────────────────────────────────┐
│                     GLOBAL SCOPE                         │
│                                                           │
│  display □    pidSetpoint □    stateDirty □             │
│  thermocouple □    pidInput □    lastSaveMs □           │
│  kilnPID □    pidOutput □    sensorFault □              │
│  Kp □    Ki □    Kd □                                   │
│  currentProgram □    programRunning □                    │
│  currentSegmentIndex □    segmentStartTemp □             │
│  segmentStartMillis □    holdStartMillis □               │
│  pauseMillis □    inHold □    inPause □                 │
│  SSR_Status □    windowStartMillis □                     │
│  lastSSRChangeTime □    SSR_CHANGE_RATE_LIMIT □         │
│  lastTempMillis □    buttonPressStart □                  │
│  lastStateSaveMillis □    lastSavedSetpoint □           │
│  buttonPrev □                                            │
│                                                           │
│  ┌──────────────┐   ┌──────────┐   ┌─────────────┐     │
│  │              │   │          │   │             │      │
│  │  main()      │   │readTemp()│   │drawUI()     │      │
│  │pidCompute()  │   │          │   │updateSSR()  │      │
│  │startProgram()│───│modify all│──→│handleProg() │      │
│  │...           │   │...       │   │...          │      │
│  └──────────────┘   └──────────┘   └─────────────┘      │
│                                                           │
│      ↑ All functions directly access and modify ↑        │
└─────────────────────────────────────────────────────────┘

PROBLEMS:
❌ No encapsulation
❌ 35+ variables to track mentally
❌ Anyone can modify anything
❌ Hard to test individual components
❌ Data dependencies not obvious
❌ Difficult to add features safely
```

---

### Improved Architecture (After Refactoring)

```
┌──────────────────────────────────────────────────────┐
│              ORGANIZED GLOBAL SCOPE                  │
│                                                      │
│  ┌────────────────────────────────────────────────┐ │
│  │ KilnContext kiln                               │ │
│  ├────────────────────────────────────────────────┤ │
│  │ • display, thermocouple, pid_controller       │ │
│  │ • pid_setpoint, pid_input, pid_output         │ │
│  │ • pid_tuning (Kp, Ki, Kd grouped)             │ │
│  │ • program_state (current, running, paused)    │ │
│  │ • ssr (status, window, rate_limit)            │ │
│  │ • safety (sensor_fault, excessive_temp)       │ │
│  │ • persistence (dirty, last_save_ms)           │ │
│  └────────────────────────────────────────────────┘ │
│                                                      │
│  ┌────────────────────────────────────────────────┐ │
│  │ TimingManager timing                           │ │
│  ├────────────────────────────────────────────────┤ │
│  │ • shouldUpdateTemperature(now)                │ │
│  │ • shouldSaveState(now)                        │ │
│  │ • shouldUpdateDisplay(now)                    │ │
│  └────────────────────────────────────────────────┘ │
│                                                      │
│  ┌────────────────────────────────────────────────┐ │
│  │ SafetyMonitor safety                           │ │
│  ├────────────────────────────────────────────────┤ │
│  │ • update(temperature)                         │ │
│  │ • isSystemSafe()                              │ │
│  │ • getCurrentAlert()                           │ │
│  └────────────────────────────────────────────────┘ │
│                                                      │
│  ┌──────────────────────────────────────────────────────┐
│  │                   Functions                         │
│  ├──────────────────────────────────────────────────────┤
│  │                                                      │
│  │  readTemperatureC(kiln,...)  ┐                     │
│  │  drawUI(kiln,...)            │ Receive context+   │
│  │  updateSSR(kiln,...)         │ managers as params │
│  │  handleProgramProgress(...) ┘                      │
│  │                                                     │
│  │  └── Signatures show dependencies clearly!        │
│  └──────────────────────────────────────────────────────┘
└──────────────────────────────────────────────────────┘

BENEFITS:
✅ Clear data grouping
✅ Reduced global count (35+ → ~3)
✅ Encapsulation (use methods instead of direct access)
✅ Easier to test (pass objects to functions)
✅ Dependencies visible in signatures
✅ Safe to add new features
✅ Better memory layout (related data together)
```

---

## Visual Function Signature Improvement

### Before (Current)
```cpp
// Signature hides dependencies and side effects
void drawUI();
void updateSSR(unsigned long nowMs);
void pidCompute(unsigned long nowMs);
void handleProgramProgress(unsigned long nowMs);
void startProgram(Program* p);

// Must trace into function to see what globals they access
// Hard to understand which globals affect which functions
```

### After (Improved)
```cpp
// Signature shows all dependencies
void drawUI(const KilnContext& ctx);
void updateSSR(KilnContext& ctx, unsigned long nowMs);
void pidCompute(KilnContext& ctx);
void handleProgramProgress(KilnContext& ctx, unsigned long nowMs);
bool startProgram(KilnContext& ctx, const Program& program, unsigned long nowMs);

// Clear what data is read (const&) vs. modified (&)
// Easy to see that SSR and PID both need the context
// Simple to unit test: just create a test context
```

---

## Before/After Code Metrics

### Memory Organization

**Before:**
```
Global Memory Layout (fragmented):
0x00: display (100 bytes)
0x64: padding/gap
0x80: thermocouple (20 bytes)
0x94: padding/gap
0x100: pidSetpoint (8 bytes)
0x108: pidInput (8 bytes)
0x110: pidOutput (8 bytes)
0x118: padding/gap
0x140: currentProgram (300 bytes)
0x26c: padding/gap
... scattered throughout memory
Total global size: ~600 bytes + fragmentation
```

**After:**
```
Global Memory Layout (organized):
0x00: KilnContext {
    0x00: display* (4 bytes)
    0x04: thermocouple* (4 bytes)
    0x08: pid_controller* (4 bytes)
    0x0c: pid_setpoint (8 bytes)
    0x14: pid_input (8 bytes)
    0x1c: pid_output (8 bytes)
    0x24: pid_tuning {kp, ki, kd} (12 bytes)
    0x30: current_program (300 bytes)
    0x15c: program_state { ... } (40 bytes)
    0x184: ssr { ... } (24 bytes)
    0x19c: safety { ... } (4 bytes)
    0x1a0: persistence { ... } (12 bytes)
    0x1ac: timing { ... } (32 bytes)
    0x1cc: last_button_state (1 byte)
} = ~460 bytes (contiguous, better cache locality)

0x1cc: TimingManager (~40 bytes)
0x1f4: SafetyMonitor (~16 bytes)
Total global size: ~520 bytes (more compact, better organization)
```

---

## Feature Addition Comparison

### Adding a New Feature: Rate Monitoring

**Before (Current - Complex)**
```cpp
// New globals needed to track feature
unsigned long rateMonitorStartMs = 0;
float rateMonitorStartTemp = 0.0;
float lastRecordedRate = 0.0;
bool rateLimitExceeded = false;

// Scattered checks throughout code
void loop() {
    // ... in temperature read
    if (now - rateMonitorStartMs >= 3000) {
        float elapsedSec = (now - rateMonitorStartMs) / 1000.0f;
        float tempDelta = kiln.pidInput - rateMonitorStartTemp;
        float rate = (tempDelta / elapsedSec) * 3600.0f;  // °C/h
        lastRecordedRate = rate;
        
        if (rate > MAX_RATE) {
            rateLimitExceeded = true;
        } else {
            rateLimitExceeded = false;
        }
        
        rateMonitorStartTemp = kiln.pidInput;
        rateMonitorStartMs = now;
    }
    
    // ... later in safety checks  
    if (rateLimitExceeded) {
        // handle error
    }
}
```

**After (Improved - Clean)**
```cpp
// Create a new manager class
class RateMonitor {
public:
    void update(float temperature, unsigned long nowMs) {
        if (nowMs - last_record_ms_ >= RECORD_INTERVAL_MS) {
            float delta = temperature - last_recorded_temp_;
            float rate = (delta / (RECORD_INTERVAL_MS / 1000.0f)) * 3600.0f;
            
            rate_exceeded_ = (fabs(rate) > MAX_RATE);
            last_rate_ = rate;
            last_recorded_temp_ = temperature;
            last_record_ms_ = nowMs;
        }
    }
    
    bool isRateExceeded() const { return rate_exceeded_; }
    float getLastRate() const { return last_rate_; }

private:
    static constexpr unsigned long RECORD_INTERVAL_MS = 3000;
    static constexpr float MAX_RATE = 300.0f;  // °C/h max
    
    unsigned long last_record_ms_ = 0;
    float last_recorded_temp_ = 0.0;
    float last_rate_ = 0.0;
    bool rate_exceeded_ = false;
};

// Usage in main loop
RateMonitor rate_monitor;

void loop() {
    // ... temperature read
    rate_monitor.update(kiln.pid_input, now);
    
    if (rate_monitor.isRateExceeded()) {
        // handle safely
    }
}
```

**Impact:**
- Before: 3 new globals + scattered logic
- After: 1 new manager class, encapsulated logic
- Easier to test, modify, or disable
- No impact on global namespace

---

## Implementation Checklist

### Phase 1: Grouping (1 Week)
- [ ] Create struct for timing variables
- [ ] Create struct for program state
- [ ] Create struct for SSR state
- [ ] Create struct for persistence state
- [ ] Create struct for safety state
- [ ] Create struct for PID tuning
- [ ] Verify compilation ✓
- [ ] Verify functionality unchanged ✓
- [ ] Commit to version control

### Phase 2: Context Creation (1 Week)
- [ ] Create `kinl_context.h`
- [ ] Create central `KilnContext` struct
- [ ] Move all grouped data into context
- [ ] Verify compilation ✓
- [ ] Verify functionality unchanged ✓
- [ ] Commit to version control

### Phase 3: Manager Classes (2 Weeks)
- [ ] Create `TimingManager` class
- [ ] Create `SafetyMonitor` class
- [ ] Create `RateMonitor` class
- [ ] Create `HardwareController` class
- [ ] Create `ProgramExecutor` class
- [ ] Integration testing

### Phase 4: Function Refactoring (2 Weeks)
- [ ] Update all function signatures
- [ ] Update temperature reading
- [ ] Update UI rendering
- [ ] Update SSR control
- [ ] Update program handling
- [ ] Update web handlers
- [ ] Full system testing

### Phase 5: Testing & Documentation (1 Week)
- [ ] Unit test key managers
- [ ] Integration testing
- [ ] Performance validation
- [ ] Update documentation
- [ ] Code review
- [ ] Release

---

## Summary

**Starting Point:** 35+ scattered global variables  
**Quick Wins:** 1-2 hours, reduces to ~20 variables in structs  
**Full Refactoring:** 4-5 weeks, reduces to ~3 organized objects  

**Key Achievement:** From "Wild West" globals to **well-organized, testable architecture**.

