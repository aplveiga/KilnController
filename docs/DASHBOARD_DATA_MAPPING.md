# Dashboard Data Mapping - Field Reference

## Overview
This document maps each dashboard field to its source in the KilnController code.

---

## Dashboard Fields and Data Sources

### 1. Temperature
- **Display Label**: "Temperature"
- **Display Value**: `${pidInput}.toFixed(1) °C`
- **Source Variable**: `float pidInput`
- **Location**: `KilnController.cpp` (global)
- **Updated By**: `readTemperatureC()` function
- **Source Device**: MAX6675 thermocouple
- **Update Frequency**: Every 1 second
- **Example Values**: `850.5 °C`, `0.0 °C` (sensor fault)

```cpp
// In KilnController.cpp
float pidInput = 25.0;  // Current temperature

// Reading code:
float t = readTemperatureC();
if (!isnan(t)) pidInput = t;
```

**JSON Field**:
```json
"temperature": 850.5
```

---

### 2. Set Point (Setpoint)
- **Display Label**: "Set Point"
- **Display Value**: `${pidSetpoint}.toFixed(1) °C`
- **Source Variable**: `float pidSetpoint`
- **Location**: `KilnController.cpp` (global)
- **Updated By**: `computeSetpointForCurrentSegment()`
- **Meaning**: Target temperature the PID is trying to reach
- **Update Frequency**: Every 1 second (during program run)
- **Example Values**: `900.0 °C`, `25.0 °C` (idle)

```cpp
// In KilnController.cpp
float pidSetpoint = 25.0;  // Target temperature

// Set by program segment calculation:
pidSetpoint = computeSetpointForCurrentSegment(nowMs);
```

**JSON Field**:
```json
"setpoint": 900.0
```

---

### 3. Program
- **Display Label**: "Program"
- **Display Value**: `${program}`
- **Source Variable**: `const char* name` (in currentProgram struct)
- **Location**: `KilnController.cpp` (global)
- **Examples**: `"9-step"`, `"4-step"`
- **Update Frequency**: Only when program changes
- **Type**: C string

```cpp
// In KilnController.cpp
Program* currentProgram = &defaultProgram;

struct Program {
  const char* name;    // "9-step" or "4-step"
  Segment segments[9];
  uint8_t seqCount;
};
```

**JSON Field**:
```json
"program": "9-step"
```

---

### 4. Status
- **Display Label**: "Status"
- **Display Value**: Status string (calculated from program state)
- **Possible Values**: `"IDLE"`, `"RAMP"`, `"HOLD"`, `"PAUSE"`, `"FAULT"`
- **Location**: `KilnController.cpp` (calculated from flags)
- **Source Flags**:
  - `bool programRunning` - Program active
  - `bool inPause` - User paused
  - `bool inHold` - At target, holding
  - `bool sensorFault` - Sensor error

```cpp
// Status determination logic (in getKilnStatusJSON):
if (sensorFault) {
  status = "FAULT";      // Sensor error
} else if (!programRunning) {
  status = "IDLE";       // Not running
} else if (inPause) {
  status = "PAUSE";      // Paused by user
} else if (inHold) {
  status = "HOLD";       // Holding at target
} else {
  status = "RAMP";       // Actively changing temperature
}
```

**Status Meaning**:
- **IDLE**: Kiln is off, no program running
- **RAMP**: Temperature actively changing to setpoint
- **HOLD**: Temperature stable at target, holding for specified time
- **PAUSE**: Program started but user pressed button to pause
- **FAULT**: Thermocouple/MAX6675 sensor error detected

**JSON Field**:
```json
"status": "RAMP"
```

---

### 5. Rate
- **Display Label**: "Rate"
- **Display Value**: `${rate}.toFixed(1) °C/h`
- **Source Variable**: `float rate_c_per_hour` (in current segment)
- **Location**: `KilnController.cpp` segment definition
- **Meaning**: How fast temperature should change (degrees per hour)
- **Update Frequency**: When segment changes
- **Example Values**: `50.0`, `100.0`, `200.0` °C/h

```cpp
// In KilnController.cpp - Segment definition
struct Segment {
  float rate_c_per_hour;    // e.g., 50.0, 100.0
  float target_c;
  uint32_t hold_seconds;
};

// Accessing current segment's rate:
Segment &seg = currentProgram->segments[currentSegmentIndex];
float rate = seg.rate_c_per_hour;
```

**9-Step Program Rates**:
```
Segment 1: 50.0 °C/h
Segment 2: 200.0 °C/h
Segment 3: 75.0 °C/h
Segment 4: 25.0 °C/h
Segment 5: 50.0 °C/h
Segment 6: 100.0 °C/h
Segment 7: 50.0 °C/h
Segment 8: 25.0 °C/h
Segment 9: 25.0 °C/h
```

**JSON Field**:
```json
"rate": 100.0
```

---

### 6. Target
- **Display Label**: "Target"
- **Display Value**: `${target}.toFixed(1) °C`
- **Source Variable**: `float target_c` (in current segment)
- **Location**: `KilnController.cpp` segment definition
- **Meaning**: Goal temperature for current segment
- **Update Frequency**: When segment changes
- **Example Values**: `100.0 °C`, `900.0 °C`, `1100.0 °C`

```cpp
// In KilnController.cpp - Segment definition
struct Segment {
  float rate_c_per_hour;
  float target_c;           // e.g., 100.0, 900.0
  uint32_t hold_seconds;
};

// Accessing current segment's target:
Segment &seg = currentProgram->segments[currentSegmentIndex];
float target = seg.target_c;
```

**9-Step Program Targets**:
```
Segment 1: Target 100 °C (hold 30 min)
Segment 2: Target 300 °C
Segment 3: Target 550 °C (hold 30 min)
Segment 4: Target 573 °C (hold 30 min)
Segment 5: Target 600 °C
Segment 6: Target 900 °C
Segment 7: Target 1000 °C
Segment 8: Target 1050 °C
Segment 9: Target 1100 °C (hold 15 min) - FINAL
```

**JSON Field**:
```json
"target": 900.0
```

---

## Complete JSON Response Structure

```cpp
String getKilnStatusJSON() {
  StaticJsonDocument<256> doc;
  
  doc["temperature"] = isnan(pidInput) ? 0.0 : pidInput;
  doc["setpoint"] = pidSetpoint;
  
  doc["program"] = currentProgram->name;
  doc["segment"] = currentSegmentIndex + 1;
  doc["segmentCount"] = currentProgram->seqCount;
  
  Segment &seg = currentProgram->segments[currentSegmentIndex];
  doc["rate"] = seg.rate_c_per_hour;
  doc["target"] = seg.target_c;
  
  if (sensorFault) {
    doc["status"] = "FAULT";
  } else if (!programRunning) {
    doc["status"] = "IDLE";
  } else if (inPause) {
    doc["status"] = "PAUSE";
  } else if (inHold) {
    doc["status"] = "HOLD";
  } else {
    doc["status"] = "RAMP";
  }
  
  doc["running"] = programRunning;
  doc["holding"] = inHold;
  doc["paused"] = inPause;
  doc["ssrStatus"] = SSR_Status;
  doc["pidOutput"] = pidOutput;
  
  String response;
  serializeJson(doc, response);
  return response;
}
```

---

## Example API Responses

### Example 1: Idle State
```json
{
  "temperature": 25.3,
  "setpoint": 25.0,
  "program": "9-step",
  "segment": 1,
  "segmentCount": 9,
  "rate": 50.0,
  "target": 100.0,
  "status": "IDLE",
  "running": false,
  "holding": false,
  "paused": false,
  "ssrStatus": false,
  "pidOutput": 0.0
}
```

**Dashboard Display**:
```
Temperature: 25.3 °C    │  Setpoint: 25.0 °C
Program: 9-step         │  Status: IDLE
Rate: 50.0 °C/h         │  Target: 100.0 °C
```

---

### Example 2: Ramping (Heating)
```json
{
  "temperature": 450.7,
  "setpoint": 500.0,
  "program": "9-step",
  "segment": 2,
  "segmentCount": 9,
  "rate": 200.0,
  "target": 300.0,
  "status": "RAMP",
  "running": true,
  "holding": false,
  "paused": false,
  "ssrStatus": true,
  "pidOutput": 85.3
}
```

**Dashboard Display**:
```
Temperature: 450.7 °C   │  Setpoint: 500.0 °C
Program: 9-step         │  Status: RAMP
Rate: 200.0 °C/h        │  Target: 300.0 °C
```

---

### Example 3: Holding at Target
```json
{
  "temperature": 100.1,
  "setpoint": 100.0,
  "program": "9-step",
  "segment": 1,
  "segmentCount": 9,
  "rate": 50.0,
  "target": 100.0,
  "status": "HOLD",
  "running": true,
  "holding": true,
  "paused": false,
  "ssrStatus": false,
  "pidOutput": 5.2
}
```

**Dashboard Display**:
```
Temperature: 100.1 °C   │  Setpoint: 100.0 °C
Program: 9-step         │  Status: HOLD
Rate: 50.0 °C/h         │  Target: 100.0 °C
```

---

### Example 4: Sensor Fault
```json
{
  "temperature": 0.0,
  "setpoint": 0.0,
  "program": "9-step",
  "segment": 1,
  "segmentCount": 9,
  "rate": 50.0,
  "target": 100.0,
  "status": "FAULT",
  "running": false,
  "holding": false,
  "paused": false,
  "ssrStatus": false,
  "pidOutput": 0.0
}
```

**Dashboard Display**:
```
Temperature: 0.0 °C     │  Setpoint: 0.0 °C
Program: 9-step         │  Status: FAULT
Rate: 50.0 °C/h         │  Target: 100.0 °C
```

---

## Data Update Cycle

```
KilnController Loop (every 1 second):
  1. Read temperature from MAX6675 → pidInput
  2. Calculate setpoint from segment → pidSetpoint
  3. Check program state → status flags
  4. Get current segment → rate_c_per_hour, target_c
  
Web Interface (every 1 second):
  1. Call fetch('/api/kiln')
  2. wireless.cpp calls handleKilnStatus()
  3. handleKilnStatus() calls getKilnStatusJSON()
  4. Returns JSON with all above values
  5. JavaScript updates 6 dashboard elements
```

---

## Summary Table

| Dashboard Field | Source Code Variable | Type | Update Rate | Example |
|-----------------|----------------------|------|------------|---------|
| Temperature | `float pidInput` | Measured | 1 sec | 850.5 |
| Set Point | `float pidSetpoint` | Calculated | 1 sec | 900.0 |
| Program | `currentProgram->name` | String | On change | "9-step" |
| Status | Calculated from flags | String | 1 sec | "RAMP" |
| Rate | `seg.rate_c_per_hour` | Float | On segment change | 100.0 |
| Target | `seg.target_c` | Float | On segment change | 900.0 |

All values are read directly from the running KilnController main loop, ensuring real-time accuracy.
