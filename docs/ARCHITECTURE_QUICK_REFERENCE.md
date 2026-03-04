# Modern Modular Architecture - Quick Reference Guide

**For Quick Overview of New Structure**

---

## File Organization Overview

```
src/
├── main.cpp                          ← Application entry point (~80 lines instead of 819)
│
├── config/
│   ├── config.h                      ← All constants (pins, timing, limits)
│   └── types.h                       ← Shared enums and structs
│
├── hal/                              ← HARDWARE ABSTRACTION LAYER
│   ├── temperature_sensor.h/.cpp     ← MAX6675 wrapper
│   ├── display_manager.h/.cpp        ← SSD1306 wrapper
│   ├── ssr_controller.h/.cpp         ← SSR/GPIO wrapper
│   └── button_input.h/.cpp           ← Button ADC reader
│
├── control/                          ← BUSINESS LOGIC LAYER
│   ├── pid_controller.h/.cpp         ← PID algorithm wrapper
│   ├── program_executor.h/.cpp       ← Program segment logic
│   └── kiln_state_machine.h/.cpp     ← Main orchestrator
│
├── services/                         ← APPLICATION SERVICES LAYER
│   ├── data_logger.h/.cpp            ← Data logging service
│   ├── program_manager.h/.cpp        ← Program management service
│   ├── persistence_manager.h/.cpp    ← State save/restore service
│   └── wireless_manager.h/.cpp       ← WiFi/OTA service
│
├── api/                              ← API & WEB LAYER
│   ├── rest_api.h/.cpp               ← REST endpoint router
│   └── web_handlers/                 ← Optional: per-resource handlers
│       ├── kiln_handler.h/.cpp
│       ├── program_handler.h/.cpp
│       └── ... (others)
│
└── utils/                            ← UTILITIES
    ├── logger.h/.cpp                 ← Unified logging
    └── time_utils.h/.cpp             ← Time helpers (optional)
```

---

## Before vs After - Code Comparison

### BEFORE (Current)
```cpp
// main.cpp - 819 LINES
void loop() {
  unsigned long now = millis();
  
  // WiFi handling
  wirelessManager.handleWiFi();
  
  // Button debouncing
  bool btn = getButtonStateDebounced();
  if (btn != buttonPrev) { ... }
  
  // Temperature reading
  if (now - lastTempMillis >= TEMP_INTERVAL_MS) {
    float t = readTemperatureC();
    if (!isnan(t)) pidInput = t;
    
    // Program progression
    handleProgramProgress(now);
    
    // PID compute
    pidCompute(now);
    
    // SSR update
    updateSSR(now);
    
    // Display update
    drawUI();
    
    // State persistence
    if (stateDirty && (now - lastStateSaveMillis >= STATE_SAVE_INTERVAL_MS)) {
      saveState();
    }
  }
  delay(10);
}
```
❌ **Issues:**
- Mixed concerns in one function
- Global variables scattered everywhere
- Hard to test
- Hard to maintain
- Hard to extend

### AFTER (Refactored)
```cpp
// main.cpp - ~80 LINES
void setup() {
    sensor.init();
    display.init();
    ssr.init();
    button.init();
    kiln.init(sensor, ssr, display);
    wireless.init();
    logger.init();
    programs.init();
    api.init(kiln, logger, programs, wireless);
}

void loop() {
    unsigned long now = millis();
    
    kiln.update(now);              // All kiln logic
    wireless.handleLoop();         // WiFi/web handling
    button.update();               // Button debouncing
    
    delay(10);
}
```
✅ **Benefits:**
- Clear separation of concerns
- Dependency injection
- Testable components
- Easy to maintain
- Easy to extend

---

## Data Flow Diagrams

### Temperature Reading Flow
```
TemperatureSensor::read()
        ↓
    MAX6675 library
        ↓
    Validate reading
        ↓
    Return temperature
        ↓
KilnStateMachine::updateTemperatureReading()
        ↓
    pid_input = temperature
        ↓
PIDController::compute(input, setpoint)
        ↓
    Calculate PID output
        ↓
SSRController::updatePWM(pid_output)
        ↓
    Time-proportional control
        ↓
    digitalWrite(SSR_PIN, HIGH/LOW)
```

### Program Execution Flow
```
RestAPI::startProgram(program_data)
        ↓
KilnStateMachine::startProgram(program)
        ↓
ProgramExecutor::loadProgram(program)
        ↓
ProgramExecutor::startProgram()
        ↓
   [Every update cycle]
        ↓
ProgramExecutor::update()
        ↓
Calculate setpoint for current segment
        ↓
Return updated setpoint to state machine
        ↓
State machine uses setpoint for PID
```

### Save/Restore Flow
```
KilnStateMachine (running state)
        ↓
PersistenceManager::saveKilnState(status)
        ↓
Serialize to JSON
        ↓
Write to /state.json (LittleFS)
        ↓
    [After restart]
        ↓
PersistenceManager::restoreKilnState()
        ↓
Read from /state.json
        ↓
Deserialize from JSON
        ↓
Restore to KilnStateMachine::setRestoredState()
```

---

## Module Responsibilities

### Hardware Abstraction Layer (HAL)
| Module | Responsibility | Hides |
|--------|-----------------|-------|
| `TemperatureSensor` | Read temperature, detect faults | MAX6675 library, I2C details |
| `DisplayManager` | Render UI screens | SSD1306 library, I2C details |
| `SSRController` | PWM control SSR | GPIO, timing windows |
| `ButtonInput` | Debounced button reading | ADC raw values, debouncing |

**Key Feature:** No other code accesses libraries directly!

### Control Layer
| Module | Responsibility | Provides |
|--------|-----------------|----------|
| `PIDController` | PID algorithm | Wrapped PID computation |
| `ProgramExecutor` | Program segment tracking | Segment progression logic |
| `KilnStateMachine` | State orchestration | Complete kiln control |

**Key Feature:** Pure business logic, no hardware or networking!

### Service Layer
| Module | Responsibility | Provides |
|--------|-----------------|----------|
| `DataLogger` | Persistent data storage | Data logging API |
| `ProgramManager` | Program file management | Program CRUD operations |
| `PersistenceManager` | State persistence | Save/restore all state |
| `WirelessManager` | Network connectivity | WiFi/OTA management |

**Key Feature:** Cross-cutting services, reusable!

### API Layer
| Component | Responsibility |
|-----------|-----------------|
| `RestAPI` | Route HTTP requests → handlers |
| Web Handlers | Handle specific resource endpoints |
| Responses | Format JSON responses |

**Key Feature:** Clean separation from business logic!

---

## Data Structures Flow

### Program Execution
```
Program (struct in storage)
├── name: "9-step"
├── segment_count: 9
└── segments[] array
    └── Segment 0
        ├── rate_c_per_hour: 50.0
        ├── target_c: 100.0
        └── hold_seconds: 1800
    └── Segment 1
        ├── rate_c_per_hour: 200.0
        ├── target_c: 300.0
        └── hold_seconds: 0
    ... (more segments)
        ↓
ProgramExecutor (active execution)
├── current_program (loaded copy)
├── current_segment_idx: 0
├── segment_start_time_ms: 1234567
├── segment_start_temp: 25.0
└── is_holding: false
        ↓
KilnStateMachine (calculates)
├── current_setpoint: 45.3
├── current_mode: RAMPING
└── segment_elapsed: 123 seconds
```

### Status Information
```
KilnStatus (returned from state machine)
├── temperature: 45.3°C
├── setpoint: 50.0°C
├── pid_output: 65.2%
├── mode: RAMPING
├── program: "9-step"
├── segment_index: 0
├── segment_count: 9
├── ssr_on: true
├── sensor_fault: false
├── alert: NONE
└── ... (timing info)
        ↓
Passed to
├── DisplayManager::renderKilnStatus()
├── RestAPI::getKilnStatus()
└── DataLogger::logReading()
```

---

## Initialization Sequence

```
main()
└─ setup()
   ├─ Serial.begin()
   │
   ├─ HAL Initialization
   │  ├─ sensor.init()          → Initialize MAX6675
   │  ├─ display.init()         → Initialize SSD1306
   │  ├─ ssr.init()             → Initialize GPIO
   │  └─ button.init()          → Initialize ADC
   │
   ├─ Control Initialization
   │  └─ kiln.init(sensor, ssr, display)    → Inject dependencies
   │
   ├─ Service Initialization
   │  ├─ wireless.init()        → Start WiFi
   │  ├─ logger.init()          → Open logging
   │  ├─ programs.init()        → Load programs
   │  └─ persistence.init()     → Load saved state
   │
   ├─ Apply Restored State
   │  └─ kiln.setRestoredState(saved_status)
   │
   └─ API Initialization
      └─ api.init(kiln, logger, programs, wireless)

loop()
└─ Every ~10ms
   ├─ kiln.update(now)                    → Main control
   ├─ wireless.handleLoop()               → WiFi/web
   └─ button.update()                     → Input
```

---

## Key Design Patterns

### 1. Dependency Injection
```cpp
// ❌ OLD (Tightly Coupled)
class KilnStateMachine {
    TemperatureSensor sensor;  // Creates its own
    SSRController ssr;         // Creates its own
};

// ✅ NEW (Loosely Coupled)
class KilnStateMachine {
    void init(TemperatureSensor& sensor,    // Receives dependency
              SSRController& ssr,           // Receives dependency
              DisplayManager& display) {    // Receives dependency
        temperature_sensor_ = &sensor;
        ssr_controller_ = &ssr;
        display_manager_ = &display;
    }
};
```

### 2. Separation of Concerns
```cpp
// ❌ OLD
class Everything {
    void loop() {
        readTemp();
        updatePID();
        renderDisplay();
        handleWiFi();
        logData();
        saveState();
    }
};

// ✅ NEW
class TemperatureSensor { void read(); };
class PIDController { void compute(); };
class DisplayManager { void render(); };
class WirelessManager { void handleLoop(); };
class DataLogger { void log(); };
class PersistenceManager { void save(); };
```

### 3. Open/Closed Principle
```cpp
// ✅ Adding new feature doesn't modify existing code
// Just add: DisplayManager::renderDebugScreen()
// Existing flow unchanged
```

### 4. Single Responsibility
```cpp
// Each class does ONE thing:
TemperatureSensor    → Read temperature
DisplayManager       → Show on display
SSRController        → Control SSR relay
DataLogger           → Log data
WirelessManager      → Manage WiFi
PersistenceManager   → Save/restore state
```

---

## Testing Strategy

### Unit Test Example
```cpp
// test/test_pid_controller.cpp
void test_pid_increases_output_when_input_below_setpoint() {
    // Arrange
    PIDController pid;
    pid.init({10.0, 5.0, 2.0});
    pid.setOutputLimits(0, 100);
    
    // Act
    pid.compute(20.0, 30.0);  // input=20, setpoint=30 (below)
    
    // Assert
    TEST_ASSERT_GREATER_THAN(0, pid.getOutput());  // Output increased
}
```

### Integration Test Example
```cpp
void test_kiln_ramps_to_setpoint() {
    // Arrange
    MockTemperatureSensor sensor;
    MockSSRController ssr;
    MockDisplayManager display;
    
    KilnStateMachine kiln;
    kiln.init(sensor, ssr, display);
    
    // Act
    Program prog;
    prog.segments[0] = {100.0, 300.0, 0};
    kiln.startProgram(prog);
    
    // Simulate heating
    for (int i = 0; i < 30; i++) {  // 30 seconds
        sensor.setTemperature(25.0 + i * 3.0);
        kiln.update(i * 1000);
    }
    
    // Assert
    assert(kiln.isProgramRunning() == true);
}
```

---

## Common Tasks & Where to Do Them

| Task | Module | Location |
|------|--------|----------|
| Add new sensor | HAL | `src/hal/new_sensor.h/.cpp` |
| Change control algorithm | Control | `src/control/kiln_state_machine.cpp` |
| Add new program file format | Services | `src/services/program_manager.cpp` |
| Add REST endpoint | API | `src/api/rest_api.cpp` |
| Change PID gains | Config | `src/config/config.h` |
| Add startup sequence | App | `src/main.cpp` - setup() |
| Update display layout | HAL | `src/hal/display_manager.cpp` |

---

## Compilation & Linking

### platformio.ini
```ini
[env:esp8266]
platform = espressif8266
board = nodemcuv2
framework = arduino

# Include paths for all modules
build_flags = 
    -Isrc/config
    -Isrc/hal
    -Isrc/control
    -Isrc/services
    -Isrc/api
    -Isrc/utils

# Source files
src_dir = src
lib_dir = lib
```

### Compilation Order
1. `config.h` and `types.h` (dependencies for all)
2. `hal/*.h/.cpp` (hardware abstractions)
3. `control/*.h/.cpp` (depends on HAL)
4. `services/*.h/.cpp` (depends on types)
5. `api/*.h/.cpp` (depends on services & control)
6. `main.cpp` (ties everything together)

---

## Performance Characteristics

| Aspect | Before | After | Notes |
|--------|--------|-------|-------|
| **Boot Time** | ~2s | ~2s | Same |
| **Temperature Check** | 1ms | 1ms | Same |
| **PID Compute** | 5µs | 5µs | Same |
| **Loop Cycle** | 10ms | 10ms | Same |
| **Memory (RAM)** | ~38KB | ~40KB | +2%: Object overhead |
| **Memory (Flash)** | ~410KB | ~425KB | +4%: More structure |
| **Response Time** | <100ms | <100ms | Same |

---

## Troubleshooting Guide

| Problem | Probable Cause | Solution |
|---------|-----------------|----------|
| Compilation errors | Missing include | Check include paths in platformio.ini |
| Dependency not found | Forward declaration missing | Add // Forward declarations at top |
| Undefined reference | Missing .cpp implementation | Implement .cpp file for interface |
| Memory leak | Object not destroyed | Add shutdown() methods |
| Slow performance | Too many function calls | Use inline for HAL wrapper getters |

---

## Quick Wins After Refactoring

1. **Testing** - Write unit tests for each module independently
2. **Optimization** - Profile each module separately
3. **Reusability** - Transplant HAL layer to other projects
4. **Documentation** - Auto-generate from clear code structure
5. **Debugging** - Enable/disable modules independently
6. **Scaling** - Add new features without main.cpp changes

---

**Reference Version:** 1.0  
**For use with:** MODERN_ARCHITECTURE_ANALYSIS.md & IMPLEMENTATION_GUIDE.md

