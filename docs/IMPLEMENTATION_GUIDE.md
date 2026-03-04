# Modern Modular Architecture - Implementation Guide

**Status:** Ready for Implementation  
**Phase:** 1-6 Implementation Steps  
**Estimated Duration:** 20-30 hours of development

---

## Table of Contents
1. [Quick Start](#quick-start)
2. [Phase-by-Phase Implementation](#phase-by-phase-implementation)
3. [Module Dependency Map](#module-dependency-map)
4. [Testing Strategy](#testing-strategy)
5. [Migration Checklist](#migration-checklist)

---

## Quick Start

### Directory Structure Created
```
src/
├── config/
│   ├── config.h          ✅ Configuration constants (DONE)
│   └── types.h           ✅ Shared types & enums (DONE)
├── hal/                  (Hardware Abstraction Layer)
│   ├── temperature_sensor.h   ✅ (Interface created)
│   ├── display_manager.h      ✅ (Interface created)
│   ├── ssr_controller.h       ✅ (Interface created)
│   └── button_input.h         ✅ (Interface created)
├── control/              (Business Logic)
│   ├── pid_controller.h       ✅ (Interface created)
│   ├── program_executor.h     ✅ (Interface created)
│   └── kiln_state_machine.h   ✅ (Interface created)
├── services/             (Application Services)
│   ├── data_logger.h          ✅ (Interface created)
│   ├── program_manager.h      ✅ (Interface created)
│   ├── persistence_manager.h  ✅ (Interface created)
│   └── wireless_manager.h     ✅ (Interface created)
├── api/                  (REST API)
│   └── rest_api.h             ✅ (Interface created)
└── utils/                (Utilities)
    └── logger.h               ✅ (Interface created)
```

All interface files have been created. Now it's time to implement!

---

## Phase-by-Phase Implementation

### Phase 1: Hardware Abstraction Layer (HAL) - ~6 hours

**Goal:** Create clean abstractions around hardware drivers

#### 1.1 TemperatureSensor Implementation
```cpp
// src/hal/temperature_sensor.cpp
#include "temperature_sensor.h"
#include <max6675.h>
#include "../config/config.h"

static MAX6675 thermocouple(Pins::THERMOCOUPLE_SCK, 
                            Pins::THERMOCOUPLE_CS, 
                            Pins::THERMOCOUPLE_OUT);

TemperatureSensor::TemperatureSensor() 
    : initialized_(false), fault_state_(false), 
      last_temperature_(Safety::DEFAULT_SETPOINT), 
      last_read_time_(0) {}

void TemperatureSensor::init() {
    // Initialize MAX6675
    // No special initialization needed for this library
    initialized_ = true;
    LOG_INFO("TempSensor", "Initialized");
}

float TemperatureSensor::readTemperatureC() {
    double raw = thermocouple.readCelsius();
    last_read_time_ = millis();
    
    if (!isFinite(raw)) {
        fault_state_ = true;
        return NAN;
    }
    
    last_temperature_ = sanitizeReading(raw);
    fault_state_ = !isValidReading(last_temperature_);
    
    return last_temperature_;
}

bool TemperatureSensor::isValidReading(float temp) const {
    return (temp >= Safety::SENSOR_MIN_VALID && 
            temp <= Safety::SENSOR_MAX_VALID);
}

bool TemperatureSensor::isFaulted() {
    return fault_state_;
}

float TemperatureSensor::sanitizeReading(float raw_value) {
    // Clamp to valid range
    if (raw_value < Safety::SENSOR_MIN_VALID) 
        return Safety::SENSOR_MIN_VALID;
    if (raw_value > Safety::SENSOR_MAX_VALID) 
        return Safety::SENSOR_MAX_VALID;
    return raw_value;
}
```

**Key points:**
- Wraps MAX6675 library usage
- Handles error cases gracefully
- Returns NAN on fault
- Validates all readings

#### 1.2 DisplayManager Implementation
Extract display code from main.cpp into this class.

#### 1.3 SSRController Implementation
```cpp
// src/hal/ssr_controller.cpp
void SSRController::setOn() {
    digitalWrite(Pins::SSR_RELAY, HIGH);
    current_state_ = true;
    last_state_change_ms_ = millis();
}

void SSRController::setOff() {
    digitalWrite(Pins::SSR_RELAY, LOW);
    current_state_ = false;
    last_state_change_ms_ = millis();
}

void SSRController::setPulseWidth(float duty_percent) {
    desired_pulse_width_ = constrain(duty_percent, 0.0f, 100.0f);
}

void SSRController::updatePWM(unsigned long current_ms) {
    if (!initialized_) return;
    
    // Manage PWM window
    if (current_ms - last_pwm_window_start_ms_ > Timing::PID_COMPUTE_WINDOW_MS) {
        last_pwm_window_start_ms_ += Timing::PID_COMPUTE_WINDOW_MS;
    }
    
    // Calculate desired state based on duty cycle
    unsigned long on_time = (desired_pulse_width_ * Timing::PID_COMPUTE_WINDOW_MS) / 100.0f;
    bool desired_state = (current_ms - last_pwm_window_start_ms_ < on_time);
    
    // Apply rate limiting
    if (desired_state != current_state_ && canChangeState(current_ms)) {
        if (desired_state) setOn();
        else setOff();
    }
}
```

#### 1.4 ButtonInput Implementation
Debouncing and long-press detection.

**Checklist:**
- [ ] TemperatureSensor fully implemented & tested
- [ ] DisplayManager fully implemented & tested
- [ ] SSRController fully implemented & tested
- [ ] ButtonInput fully implemented & tested
- [ ] All HAL headers in platformio.ini include paths
- [ ] Verify no compilation errors

---

### Phase 2: Control Layer - ~8 hours

**Goal:** Implement business logic components

#### 2.1 PIDController
Wraps Arduino-PID-Library with additional methods.

```cpp
// src/control/pid_controller.cpp
#include "pid_controller.h"
#include <PID_v1.h>

// Internal PID object
static PID* internal_pid_ = nullptr;
static double pid_input_;
static double pid_output_;
static double pid_setpoint_;

void PIDController::init(const PIDParams& tuning) {
    // Create internal PID object
    internal_pid_ = new PID(&pid_input_, &pid_output_, &pid_setpoint_,
                            tuning.kp, tuning.ki, tuning.kd, DIRECT);
    
    internal_pid_->SetMode(AUTOMATIC);
    internal_pid_->SetOutputLimits(output_min_, output_max_);
    internal_pid_->SetSampleTime(sample_time_ms_);
    
    initialized_ = true;
    enabled_ = true;
    LOG_INFO("PID", "Initialized with Kp=%.2f Ki=%.2f Kd=%.2f", 
             tuning.kp, tuning.ki, tuning.kd);
}

void PIDController::compute(float input, float setpoint) {
    if (!initialized_ || !enabled_) return;
    
    pid_input_ = (double)input;
    pid_setpoint_ = (double)setpoint;
    
    internal_pid_->Compute();
}

float PIDController::getOutput() const {
    return (float)pid_output_;
}
```

#### 2.2 ProgramExecutor
Segment-by-segment program execution logic.

#### 2.3 KilnStateMachine
The main orchestrator - most complex component.

```cpp
// src/control/kiln_state_machine.cpp
#include "kiln_state_machine.h"
#include "../hal/temperature_sensor.h"
#include "../hal/ssr_controller.h"
#include "../hal/display_manager.h"

void KilnStateMachine::update(unsigned long current_ms) {
    // Call in order - each depends on previous
    updateTemperatureReading();
    updateProgramExecution();
    updatePIDControl();
    updateSSROutput();
    updateSafetyMonitoring();
    updateDisplay();
}

void KilnStateMachine::updateTemperatureReading() {
    unsigned long now = millis();
    if (now - last_temp_read_ms_ < Timing::TEMP_READ_INTERVAL_MS) 
        return;
    
    last_temp_read_ms_ = now;
    current_temperature_ = temperature_sensor_->readTemperatureC();
    
    if (temperature_sensor_->isFaulted()) {
        handleFault(SafetyAlert::SENSOR_FAULT);
    }
}

void KilnStateMachine::startProgram(const Program& program) {
    executor_.loadProgram(program);
    executor_.startProgram();
    mode_ = KilnMode::RAMPING;
    LOG_INFO("Kiln", "Program started: %s", program.name);
}
```

**Checklist:**
- [ ] PIDController fully implemented
- [ ] ProgramExecutor fully implemented
- [ ] KilnStateMachine fully implemented
- [ ] All modules compile without errors
- [ ] Dependency injection working correctly

---

### Phase 3: Service Layer - ~6 hours

**Goal:** Implement data persistence and configuration management

#### 3.1 DataLogger (Refactored)
Extract and clean up from kiln_data_logger.cpp

#### 3.2 ProgramManager (Refactored)
Clean up from program_manager.cpp

#### 3.3 PersistenceManager
New: Create unified save/restore for all state

#### 3.4 WirelessManager (Refactored)
Extract from webportal.cpp

**Checklist:**
- [ ] All service modules implemented
- [ ] State persistence working
- [ ] Program loading/saving verified
- [ ] Data logging functioning

---

### Phase 4: API Layer - ~5 hours

**Goal:** Implement REST API endpoints

Create RestAPI class that routes HTTP requests to appropriate handlers.

```cpp
// src/api/rest_api.cpp
#include "rest_api.h"
#include "../control/kiln_state_machine.h"
#include "../services/data_logger.h"

void RestAPI::handleRequest(const HttpRequest& req, HttpResponse& res) {
    // Parse route
    String resource, action, id;
    if (!parseRoute(req.path, resource, action, id)) {
        respondError(res, 404, "Not found");
        return;
    }
    
    // Route to appropriate handler
    if (resource == "kiln") {
        if (action == "status") getKilnStatus(res);
        else if (action == "start") startProgram({...}, res);
        // ... etc
    } else if (resource == "programs") {
        if (action == "list") listPrograms(res);
        // ... etc
    }
}
```

**Checklist:**
- [ ] All endpoints implemented
- [ ] JSON response formatting correct
- [ ] Error handling complete
- [ ] Web interface testing

---

### Phase 5: Application Layer & Integration - ~4 hours

**Goal:** Create minimal main.cpp and integrate all modules

```cpp
// src/main.cpp (REFACTORED - ~80 lines)
#include <Arduino.h>
#include "config/config.h"
#include "config/types.h"
#include "hal/temperature_sensor.h"
#include "hal/display_manager.h"
#include "hal/ssr_controller.h"
#include "hal/button_input.h"
#include "control/kiln_state_machine.h"
#include "services/wireless_manager.h"
#include "services/data_logger.h"
#include "services/program_manager.h"
#include "api/rest_api.h"
#include "utils/logger.h"

// Global instances
TemperatureSensor sensor;
DisplayManager display;
SSRController ssr;
ButtonInput button;
KilnStateMachine kiln;
WirelessManager wireless;
DataLogger logger;
ProgramManager programs;
RestAPI api;

void setup() {
    Serial.begin(115200);
    
    // Initialize HAL
    sensor.init();
    display.init();
    ssr.init();
    button.init();
    
    // Initialize control
    kiln.init(sensor, ssr, display);
    
    // Initialize services
    wireless.init();
    logger.init();
    programs.init();
    
    // Initialize API
    api.init(kiln, logger, programs, wireless);
    
    LOG_INFO("APP", "Kiln Controller started");
}

void loop() {
    unsigned long now = millis();
    
    // Update all systems
    kiln.update(now);
    wireless.handleLoop();
    button.update();
    
    delay(10);
}
```

**Benefits:**
- `setup()` now 20 lines (was scattered)
- `loop()` now 10 lines (was 100+)
- All concerns clearly isolated
- Easy to understand order of operations

**Checklist:**
- [ ] main.cpp compiles
- [ ] All modules integrate cleanly
- [ ] No global variable spaghetti
- [ ] Dependency injection working

---

### Phase 6: Testing & Refinement - ~4 hours

**Goal:** Comprehensive testing and optimization

#### 6.1 Unit Tests
```cpp
// test/test_pid_controller.cpp
#include <unity.h>
#include "control/pid_controller.h"

void test_pid_initialization() {
    PIDController pid;
    PIDParams params(10.0, 5.0, 2.0);
    pid.init(params);
    
    auto tuning = pid.getTunings();
    TEST_ASSERT_EQUAL_FLOAT(10.0, tuning.kp);
}

void test_pid_compute() {
    PIDController pid;
    PIDParams params(1.0, 0.1, 0.01);
    pid.init(params);
    pid.setOutputLimits(0, 100);
    
    pid.compute(20.0, 30.0);  // input=20, setpoint=30
    float output = pid.getOutput();
    
    TEST_ASSERT_GREATER_THAN(0, output);  // Should increase to reach setpoint
}
```

#### 6.2 Integration Tests
- Program execution with mock setpoints
- State machine transitions
- Persistence save/restore
- Web API endpoint verification

#### 6.3 Hardware Validation
- Temperature reading accuracy
- SSR switching timing
- Display updates
- Button debouncing

**Checklist:**
- [ ] Unit tests for each module
- [ ] Integration tests passing
- [ ] Manual device testing complete
- [ ] Performance benchmarking done
- [ ] Memory usage checked

---

## Module Dependency Map

```
┌─────────────────────────────────┐
│        main.cpp                  │ (Application)
└────────────────┬────────────────┘
                 │
     ┌───────────┼───────────┐
     │           │           │
     ▼           ▼           ▼
┌──────────┐  ┌────────┐  ┌─────────┐
│Kiln State│  │Wireless│  │DataLogger│
│Machine   │  │Manager │  │          │
└────┬─────┘  └────────┘  └─────────┘
     │
     ├─────────────┬──────────────┬──────────────┐
     ▼             ▼              ▼              ▼
 ┌────────┐   ┌──────────┐   ┌──────────┐   ┌────────┐
 │Program │   │PID       │   │Safety    │   │Logger  │
 │Executor│   │Controller│   │Monitor   │   │        │
 └────────┘   └──────────┘   └──────────┘   └────────┘
     │
     ├─────────────┬──────────────┬──────────────┐
     ▼             ▼              ▼              ▼
 ┌──────────┐ ┌────────────┐ ┌────────────┐ ┌────────────┐
 │Temp      │ │Display     │ │SSR         │ │Button      │
 │Sensor    │ │Manager     │ │Controller  │ │Input       │
 └──────────┘ └────────────┘ └────────────┘ └────────────┘
     │             │              │              │
     └─────────────┴──────────────┴──────────────┘
                   │
            ┌──────▼─────────┐
            │ Hardware/      │
            │ Libraries      │
            │ MAX6675, ...   │
            └────────────────┘
```

**Key Rules:**
1. Upper layers depend on lower layers only
2. No dependencies skip layers (e.g., main shouldn't use HAL directly)
3. Services layer doesn't depend on API or control
4. Each module owns its data and state

---

## Testing Strategy

### Unit Testing Levels

| Level | Module | Test Type | Mocks Needed |
|-------|--------|-----------|--------------|
| **HAL** | Temperature Sensor | I/O validation | MAX6675 values |
| **HAL** | SSR Controller | Timing precision | GPIO |
| **Control** | PID | Algorithm correctness | None |
| **Control** | Program Executor | Segment logic | None |
| **Control** | State Machine | Transitions | All HAL |
| **Services** | DataLogger | JSON, file ops | LittleFS |
| **Services** | Program Manager | File operations | LittleFS |

### Integration Testing

1. **Boot Sequence**
   - All modules initialize
   - No circular dependencies
   - No memory leaks

2. **Program Execution**
   - Program loads correctly
   - Segments execute in order
   - Setpoints calculated properly
   - Hold times maintained

3. **Web Interface**
   - API endpoints respond
   - JSON formatting correct
   - Data persistence works

4. **Safety**
   - Overheat protection works
   - Sensor faults detected
   - SSR failsafe enabled

---

## Migration Checklist

### Before Starting
- [ ] Backup current working code
- [ ] Create feature branch
- [ ] Review architecture document
- [ ] Plan implementation order

### Phase 1 (HAL)
- [ ] Create all HAL header files
- [ ] Implement each HAL module
- [ ] Test each HAL module independently
- [ ] Verify no compiler errors
- [ ] Point platformio.ini to new include paths

### Phase 2 (Control)
- [ ] Create control layer headers
- [ ] Implement PIDController
- [ ] Implement ProgramExecutor
- [ ] Implement KilnStateMachine
- [ ] Integration test: Does kiln control work?

### Phase 3 (Services)
- [ ] Refactor DataLogger
- [ ] Refactor ProgramManager
- [ ] Create PersistenceManager
- [ ] Refactor WirelessManager
- [ ] Integration test: Do state saves work?

### Phase 4 (API)
- [ ] Implement RestAPI class
- [ ] Create all endpoints
- [ ] Test each endpoint
- [ ] Integration test: Does web work?

### Phase 5 (Application)
- [ ] Rewrite main.cpp
- [ ] Remove old code carefully
- [ ] Verify all functionality
- [ ] Perform full system test

### Phase 6 (Cleanup)
- [ ] Remove old files
- [ ] Update documentation
- [ ] Final code review
- [ ] Merge to main branch

---

## Performance Targets

| Metric | Current | Target | Notes |
|--------|---------|--------|-------|
| **main.cpp lines** | 819 | <100 | Orchestration only |
| **Largest module** | 819 | 300 | Kiln state machine |
| **Memory footprint** | ~45KB | ~45KB | No overhead |
| **Update frequency** | 10ms | 10ms | Same timing |
| **Flash usage** | ~400KB | ~420KB | +2% for structure |
| **Compilation time** | ~15s | ~20s | More files |

---

## Documentation Updates Needed

After implementation, update:
- [ ] [ARCHITECTURE.md](ARCHITECTURE.md) - Link to new design
- [ ] Add per-module README files
- [ ] API documentation
- [ ] Testing guide
- [ ] Deployment guide

---

## Success Criteria

### Functional Requirements
- ✅ All existing features work
- ✅ Web interface fully functional
- ✅ Data logging continues
- ✅ Program execution unchanged
- ✅ WiFi/OTA works

### Code Quality
- ✅ No cyclomatic complexity > 10
- ✅ All modules < 300 lines
- ✅ 100% dependency injection
- ✅ Zero global state (except instances)
- ✅ Clear namespace isolation

### Maintainability
- ✅ 80%+ code testable
- ✅ Clear module responsibilities
- ✅ Easy to add new features
- ✅ Self-documenting code
- ✅ No code duplication

---

## Next Steps

1. **Review** - Confirm architecture aligns with project goals
2. **Plan** - Assign implementation phases
3. **Execute** - Start with Phase 1 (HAL)
4. **Validate** - Test each phase before proceeding
5. **Integrate** - Bring modules together
6. **Deploy** - Complete system testing
7. **Document** - Update all documentation

---

**Document Version:** 1.0  
**Status:** Ready for Implementation  
**Last Updated:** February 25, 2026

