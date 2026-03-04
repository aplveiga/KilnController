# Kiln Controller - Modern Modular Architecture Analysis

**Date:** February 25, 2026  
**Project:** Kiln Controller - ESP8266 + Web Portal  
**Current Architecture:** Monolithic with scattered concerns  
**Target Architecture:** Clean layered modular design

---

## Executive Summary

Your Kiln Controller project is feature-rich but suffers from typical embedded system organization issues:
- **Mixed concerns** in main.cpp (control, UI, networking, persistence)
- **Hard-to-test** monolithic structure
- **Tight coupling** between hardware, business logic, and UI
- **Difficult maintenance** and feature additions

This document proposes a **modern layered modular architecture** following SOLID principles and industry best practices.

---

## Current State Analysis

### Current Structure

```
src/
├── main.cpp                      (819 lines - controls everything)
├── program_manager.cpp           (Program persistence)
├── kiln_data_logger.cpp         (Data logging)
├── webportal.cpp                (Web server - 1263 lines)
├── WebPortalButtons.cpp         (Web button handlers)
├── WebPortalDataLogger.cpp      (Web data endpoints)
├── WebPortalFirmware.cpp        (Web firmware management)
├── WebPortalPID.cpp             (Web PID control)
├── WebPortalPrograms.cpp        (Web program management)
└── WebPortalWiFi.cpp            (Web WiFi config)
```

### Current Issues

| Issue | Impact | Severity |
|-------|--------|----------|
| **Monolithic main.cpp** | 819 lines handling temp reading, PID, SSR control, button input, UI, state persistence | HIGH |
| **Hardware tightly coupled** | Wire protocol, pins, sensor readings baked into control logic | HIGH |
| **UI logic in main** | Display updates mixed with business logic | MEDIUM |
| **Web portal fragmentation** | 7 separate WebPortal*.cpp files doing different things | MEDIUM |
| **Global variables** | Program state scattered as globals (pidSetpoint, programRunning, etc.) | HIGH |
| **No clear abstraction** | Direct calls to library functions everywhere (LittleFS, ArduinoJson, display) | MEDIUM |
| **Hard to test** | No way to unit test PID, program logic, or state management | HIGH |
| **State management unclear** | Save/restore logic mixed into main control flow | MEDIUM |

---

## Proposed Modern Architecture

### Design Principles

1. **Separation of Concerns** - Each module has ONE responsibility
2. **Dependency Injection** - Components receive dependencies, not create them
3. **Layered Architecture** - Clear data flow from hardware → logic → UI/network
4. **Abstraction** - Hide hardware details behind interfaces
5. **Testability** - All business logic independently testable

### Layered Architecture

```
┌─────────────────────────────────────────┐
│     APPLICATION LAYER                    │
│  (main.cpp - Orchestration & Loop)       │
└────────────────┬────────────────────────┘
                 │
┌────────────────┴────────────────────┐
│     API / COMMUNICATION LAYER        │
│  ┌──────────────────────────────┐   │
│  │ Web Server / REST API        │   │
│  │ UART Debug / Telemetry       │   │
│  └──────────────────────────────┘   │
└────────────────┬────────────────────┘
                 │
┌────────────────┴────────────────────────────┐
│     BUSINESS LOGIC LAYER                     │
│  ┌──────────────┐  ┌──────────────────────┐ │
│  │ Kiln State   │  │ Program Executor     │ │
│  │ Machine      │  │ PID Controller       │ │
│  └──────────────┘  └──────────────────────┘ │
└────────────────┬────────────────────────────┘
                 │
┌────────────────┴────────────────────────────┐
│     SERVICE LAYER                            │
│  ┌──────────────┐  ┌──────────────────────┐ │
│  │ Data Logger  │  │ Persistence Manager  │ │
│  │ Program Mgr  │  │ WiFi Manager         │ │
│  └──────────────┘  └──────────────────────┘ │
└────────────────┬────────────────────────────┘
                 │
┌────────────────┴────────────────────────────┐
│     HARDWARE ABSTRACTION LAYER (HAL)        │
│  ┌──────────────┐  ┌──────────────────────┐ │
│  │ Temperature  │  │ Display Manager      │ │
│  │ Sensor       │  │ SSR Controller       │ │
│  │ Button Input │  │ (GPIO abstraction)   │ │
│  └──────────────┘  └──────────────────────┘ │
└────────────────┬────────────────────────────┘
                 │
┌────────────────┴────────────────────────────┐
│     DRIVER / LIBRARY LAYER                   │
│  (MAX6675, SSD1306, ESP8266, LittleFS)      │
└────────────────────────────────────────────┘
```

### Proposed Directory Structure

```
src/
├── main.cpp                          (Application entry point - ~100 lines)
├── config/
│   ├── config.h                      (All configuration constants)
│   └── types.h                       (Shared data types & enums)
├── hal/                              (Hardware Abstraction Layer)
│   ├── temperature_sensor.h/.cpp     (MAX6675 abstraction)
│   ├── display_manager.h/.cpp        (SSD1306 abstraction) 
│   ├── ssr_controller.h/.cpp         (SSR GPIO control)
│   ├── button_input.h/.cpp           (Button ADC input)
│   └── gpio_abstraction.h            (Pin definitions & GPIO utilities)
├── control/                          (Business Logic)
│   ├── pid_controller.h/.cpp         (PID algorithm wrapper)
│   ├── kiln_state_machine.h/.cpp     (Main kiln state machine)
│   ├── program_executor.h/.cpp       (Program segment execution logic)
│   └── safety_monitor.h/.cpp         (Temperature limits & alerts)
├── services/                         (Service Layer)
│   ├── data_logger.h/.cpp            (Data logging - refactored)
│   ├── program_manager.h/.cpp        (Program management - refactored)
│   ├── persistence_manager.h/.cpp    (State save/restore)
│   └── wireless_manager.h/.cpp       (WiFi/OTA - refactored)
├── api/                              (API Layer)
│   ├── web_server.h/.cpp             (Web routes aggregator)
│   ├── rest_api.h/.cpp               (REST endpoints)
│   ├── web_handlers/
│   │   ├── kiln_handler.h/.cpp       (Kiln status/control endpoints)
│   │   ├── program_handler.h/.cpp    (Program endpoints)
│   │   ├── data_handler.h/.cpp       (Data logging endpoints)
│   │   ├── firmware_handler.h/.cpp   (OTA/firmware endpoints)
│   │   └── wifi_handler.h/.cpp       (WiFi config endpoints)
│   └── response_builder.h            (JSON response utilities)
├── utils/
│   ├── logger.h                      (Unified logging)
│   └── time_utils.h                  (Time/date utilities)
└── platform/
    ├── esp32_platform.h              (ESP32-specific abstractions)
    └── esp8266_platform.h            (ESP8266-specific abstractions)

include/
├── kiln_data_logger.h                (External interface - keep for compatibility)
├── program_manager.h                 (External interface - keep for compatibility)
└── wireless.h                        (External interface - keep for compatibility)
```

---

## Module Responsibilities

### Hardware Abstraction Layer (HAL)

**Purpose:** Hide platform-specific hardware details

#### `temperature_sensor.h/cpp`
```cpp
class TemperatureSensor {
    // Hardware abstraction for MAX6675
    float readTemperatureC();
    bool isFalted();
    bool isValidReading(float temp);
};
```
- **Responsibilities:** Read sensor, validate readings, detect faults
- **Dependencies:** I2C/SPI driver
- **Used by:** Control layer

#### `display_manager.h/cpp`
```cpp
class DisplayManager {
    // Hardware abstraction for SSD1306
    void init();
    void clear();
    void renderKilnStatus(const KilnStatus& status);
    void renderError(const String& message);
};
```

#### `ssr_controller.h/cpp`
```cpp
class SSRController {
    // Abstract SSR on/off control
    void init();
    void setPulseWidth(float duty_percent);  // 0-100%
    bool isOn();
};
```

#### `button_input.h/cpp`
```cpp
class ButtonInput {
    // Button press detection with debouncing
    bool isPressed();
    bool isLongPress();
    void update();  // Call regularly
};
```

---

### Control Layer

**Purpose:** Kiln control logic and state management

#### `kiln_state_machine.h/cpp`
```cpp
enum class KilnMode { IDLE, RAMPING, HOLDING, PAUSED, FAULT, COOLING };

class KilnStateMachine {
    void init();
    void update();                          // Call every loop iteration
    void startProgram(const Program& p);
    void pauseProgram();
    void stopProgram();
    
    KilnStatus getCurrentStatus() const;
    float getSetpoint() const;
    float getTemperature() const;
};
```
**Responsibilities:**
- Manage program execution state
- Calculate setpoints based on program segments
- Handle transitions between segments
- Manage pause/resume/hold logic
- Track timing and durations
- Provide current kiln status

**Dependencies:** 
- `PIDController`, `ProgramManager`, `TemperatureSensor`
- Does NOT directly access files, WiFi, or UI

#### `pid_controller.h/cpp`
```cpp
class PIDController {
    void setTunings(float kp, float ki, float kd);
    void setOutputLimits(float min, float max);
    void compute(float input, float setpoint);
    float getOutput() const;
};
```

#### `program_executor.h/cpp`
```cpp
class ProgramExecutor {
    void executeSegment(const Segment& seg, float current_temp);
    float calculateSetpoint(unsigned long elapsed_ms);
    bool isSegmentComplete();
    const Segment& getCurrentSegment() const;
};
```

#### `safety_monitor.h/cpp`
```cpp
class SafetyMonitor {
    void update(float temperature, const KilnStatus& status);
    bool isEmergencyCondition();
    SafetyAlert getLatestAlert();
};
```

---

### Service Layer

**Purpose:** Cross-cutting concerns (persistence, logging, config)

#### `data_logger.h/cpp`
```cpp
class DataLogger {
    void init();
    void logReading(const KilnReading& reading);
    void cleanup();  // Remove old data
    JsonArray getRecentData(int count);
};
```

#### `program_manager.h/cpp`
```cpp
class ProgramManager {
    bool saveProgram(const Program& p);
    bool loadProgram(const String& name, Program& p);
    StringArray listPrograms();
    bool deleteProgram(const String& name);
};
```

#### `persistence_manager.h/cpp`
```cpp
class PersistenceManager {
    void saveKilnState(const KilnStatus& status);
    bool restoreKilnState(KilnStatus& status);
    void savePIDTunings(const PIDParams& params);
    bool restorePIDTunings(PIDParams& params);
};
```

#### `wireless_manager.h/cpp`
```cpp
class WirelessManager {
    void init();
    void handleLoop();
    bool isConnected();
    String getStatusJSON();
    void setWiFiCredentials(const String& ssid, const String& pwd);
};
```

---

### API Layer

**Purpose:** HTTP endpoints and web interface

#### `rest_api.h/cpp`
```cpp
class RestAPI {
    void init(const KilnStateMachine& kiln);
    void handleRequest(const HttpRequest& req, HttpResponse& res);
    
private:
    // Route handlers
    void handleKilnStatus(HttpResponse& res);
    void handleStartProgram(const JsonObject& body, HttpResponse& res);
    void handleProgramList(HttpResponse& res);
    void handleDataLog(HttpResponse& res);
};
```

#### Web Handler Pattern
```cpp
class ProgramHandler {
    void handleListPrograms(HttpResponse& res);
    void handleCreateProgram(const JsonObject& body, HttpResponse& res);
    void handleDeleteProgram(const String& name, HttpResponse& res);
};
```

---

### Configuration & Types

#### `config.h`
```cpp
// Pin definitions
namespace Pins {
    constexpr int I2C_SDA = D5;
    constexpr int I2C_SCL = D6;
    constexpr int THERMOCOUPLE_SCK = D4;
    // ... etc
};

// Timing parameters
namespace Timing {
    constexpr unsigned long TEMP_READ_INTERVAL_MS = 1000;
    constexpr unsigned long PID_COMPUTE_WINDOW_MS = 2000;
    // ... etc
};

// Safety limits
namespace Safety {
    constexpr float MAX_TEMPERATURE = 1200.0f;
    constexpr float SENSOR_MIN_VALID = -50.0f;
    // ... etc
};
```

#### `types.h`
```cpp
enum class KilnMode { IDLE, RAMPING, HOLDING, PAUSED, FAULT };

struct PIDParams {
    float kp, ki, kd;
};

struct KilnReading {
    uint32_t timestamp;
    float temperature;
    float setpoint;
    const Segment* current_segment;
    KilnMode mode;
};

struct KilnStatus {
    float temperature;
    float setpoint;
    KilnMode mode;
    const Program* current_program;
    uint8_t segment_index;
    bool ssr_on;
    bool sensor_fault;
};
```

---

## Main Application (`main.cpp`)

With modular architecture, main.cpp becomes very clean:

```cpp
#include <Arduino.h>
#include "control/kiln_state_machine.h"
#include "services/wireless_manager.h"
#include "api/rest_api.h"
#include "utils/logger.h"

// Singleton managers
KilnStateMachine kilnController;
WirelessManager wireless;
RestAPI api;

void setup() {
    Serial.begin(115200);
    
    kilnController.init();
    wireless.init();
    api.init(kilnController);
}

void loop() {
    kilnController.update();
    wireless.handleLoop();
    
    delay(10);
}
```

That's it! Everything else is modularized.

---

## Benefits of This Architecture

| Benefit | How Achieved |
|---------|-------------|
| **Testability** | Each module independently testable with mock dependencies |
| **Maintainability** | Clear responsibility boundaries, easy to locate bugs |
| **Reusability** | HAL components can be reused in other projects |
| **Scalability** | New features added without modifying existing code |
| **Debugging** | Isolated modules easier to debug |
| **Performance** | Cleaner separation enables optimization |
| **Documentation** | Module purposes self-evident from structure |
| **Team collaboration** | Different developers can work on different layers |

---

## Implementation Strategy

### Phase 1: Create Infrastructure
1. Create directory structure
2. Define `config.h` and `types.h`
3. Create HAL interfaces (headers only, no implementation changes yet)

### Phase 2: Extract Hardware Abstraction
1. Create `TemperatureSensor` wrapper around MAX6675
2. Create `DisplayManager` wrapper around SSD1306
3. Create `SSRController` wrapper around GPIO
4. Create `ButtonInput` wrapper around ADC

### Phase 3: Extract Business Logic
1. Extract program execution logic into `ProgramExecutor`
2. Extract state machine into `KilnStateMachine`
3. Wrap PID algorithm in `PIDController` class

### Phase 4: Extract Services
1. Consolidate data logging logic
2. Consolidate program management
3. Create `PersistenceManager` for save/restore

### Phase 5: Refactor API Layer
1. Consolidate web handlers
2. Create `RestAPI` class
3. Remove global WirelessManager dependencies

### Phase 6: Clean Up main.cpp
1. Replace with minimal orchestration
2. Remove all business logic
3. Test integration

---

## Migration Path (Backward Compatibility)

Keep existing `include/` headers as facades:

```cpp
// include/wireless.h - Wrapper around new WirelessManager
#include "../src/services/wireless_manager.h"

extern WirelessManager wirelessManager;
// Existing code continues to work...
```

This allows **gradual migration** without breaking existing functionality.

---

## Code Quality Metrics (After Refactoring)

| Metric | Before | After |
|--------|--------|-------|
| **Avg file size** | 819 lines | 150-200 lines |
| **Cyclomatic complexity** | High (deeply nested) | Low (straight-flow) |
| **Cohesion** | Low (mixed concerns) | High (single responsibility) |
| **Coupling** | High (global variables) | Low (dependency injection) |
| **Testability** | ~5% of code (business logic) | ~80% of code (all business logic) |

---

## Next Steps

1. ✅ Review this architecture (DONE)
2. ⏳ Create directory structure  
3. ⏳ Define interfaces (headers)
4. ⏳ Start extracting modules (phase by phase)
5. ⏳ Unit tests for each module
6. ⏳ Integration testing
7. ⏳ Documentation per module

---

**Document Version:** 1.0  
**Status:** Ready for Implementation Planning
