// Kiln controller for ESP8266 + SSD1306(I2C on D5/D6) + MAX6675(SO D2, CS D3, SCK D4) + SSR D0 + Button A0
// Minimal, self-contained implementation based on REQUIREMENTS.md
// Requires libraries: Adafruit_SSD1306, Adafruit_GFX, MAX6675 (install via Library Manager)

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <max6675.h>
#include <LittleFS.h>
#include <wireless.h>
#include <kiln_data_logger.h>

//
// Pin mapping (NodeMCU style Dn defines available in ESP8266 core)
const int PIN_SDA = D5;      // SDA -> D5 (GPIO14)
const int PIN_SCL = D6;      // SCL -> D6 (GPIO12)

const int PIN_MAX_SCK = D4;  // SCK -> D4 (GPIO2)
const int PIN_MAX_CS  = D3;  // CS  -> D3 (GPIO0) - note: D3 is boot-associated
const int PIN_MAX_SO  = D2;  // SO  -> D2 (GPIO4)

const int PIN_SSR = D0;      // SSR -> D0 (GPIO16)
const int PIN_BUTTON_ADC = A0; // Button on analog port A0

// SSD1306 display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// MAX6675 object
MAX6675 thermocouple(PIN_MAX_SCK, PIN_MAX_CS, PIN_MAX_SO);

// PID control parameters (defaults - tune for your kiln)
float Kp = 15.5;    // Proportional gain (lower for high thermal mass to minimize overshoot)
float Ki = 13.1;   // Integral gain (moderate for steady-state accuracy)
float Kd = 1.20;  // Derivative gain (higher to dampen oscillations and thermal lag)

// Control window for SSR (ms)
const unsigned long CONTROL_WINDOW_MS = 2000UL; // 2s window

// Sampling / update intervals
const unsigned long TEMP_INTERVAL_MS = 1000UL;  // temperature read, PID compute

// Safety limits
const float ABSOLUTE_MAX_TEMP = 1200.0;   // emergency cutoff (°C)
const float SENSOR_MIN_VALID = -50.0;
const float SENSOR_MAX_VALID = 1100.0;

// Button thresholds (ADC 0..1023)
const int ADC_BUTTON_THRESHOLD = 200;   // pressed if ADC < threshold (adjust hardware)
const unsigned long LONG_PRESS_MS = 2000UL;
unsigned long lastTempMillis = 0;
unsigned long buttonPressStart = 0;
bool buttonPrev = false;
const unsigned long BUTTON_DEBOUNCE_MS = 50;

// Program structures
struct Segment {
  float rate_c_per_hour; // °C/h
  float target_c;        // target temperature for this segment
  uint32_t hold_seconds; // hold duration in seconds
};

struct Program {
  const char* name;
  Segment segments[9]; // max 9 segments for simplicity
  uint8_t seqCount;
};

// Default program "9-step" based on typical kiln firing curve (ramp rates and holds); adjust as needed
Program defaultProgram = {
  "9-step",
  {
    {50.0, 100.0, 30 * 60},   // seq 1
    {200.0, 300.0, 0},  // seq 2
    {75.0, 550.0, 30 * 60},   // seq 3
    {25.0, 573.0, 30 * 60},  // seq 4
    {50.0, 600.0, 0},   // seq 5
    {100.0, 900.0, 0},  // seq 6
    {50.0, 1000.0, 0},   // seq 7
    {25.0, 1050.0, 0},  // seq 8
    {25.0, 1100.0, 15 * 60}   // seq 9
  },
  9
};

// Secondary program "3-step" for faster testing; adjust as needed
Program SecondaryProgram = {
  "4-step",
  {
    {50.0, 100.0, 15 * 60},   // seq 1
    {100.0, 573.0, 30 * 60},  // seq 2
    {100.0, 900.0, 0},  // seq 3
    {50.0, 1100.0, 30 * 60}   // seq 4  
  },
  4
};

// Runtime program state
Program* currentProgram = &defaultProgram;
bool programRunning = false;
uint8_t currentSegmentIndex = 0;
float segmentStartTemp = 25.0;     // captured at segment start
unsigned long segmentStartMillis = 0;
unsigned long holdStartMillis = 0;
unsigned long pauseMillis = 0;
bool inHold = false;
bool inPause = false;
bool SSR_Status = false;


// PID runtime
float pidSetpoint = 25.0;
float pidInput = 25.0;
float pidOutput = 0.0;
float integralTerm = 0.0;
float lastError = 0.0;
unsigned long lastPidMillis = 0;

// Persistence (LittleFS)
const char *STATE_FILE = "/kiln_state.txt";
bool stateDirty = false;
unsigned long lastStateSaveMillis = 0;
const unsigned long STATE_SAVE_INTERVAL_MS = 10000UL; // throttle writes
float lastSavedSetpoint = 0.0;

// SSR time-proportional control
unsigned long windowStartMillis = 0;

// Thermocouple error flag
bool sensorFault = false;

// ---------- Persistence helpers ----------
void saveState() {
  if (!LittleFS.begin()) return;
  File f = LittleFS.open(STATE_FILE, "w");
  if (!f) return;
  // pidSetpoint, programName, currentSegmentIndex, programRunning
  unsigned long now = millis();
  unsigned long segmentElapsed = 0;
  unsigned long holdElapsed = 0;
  if (segmentStartMillis != 0) segmentElapsed = (now - segmentStartMillis) / 1000UL;
  if (inHold && holdStartMillis != 0) holdElapsed = (now - holdStartMillis) / 1000UL;
  f.printf("pidSetpoint=%.3f\n", pidSetpoint);
  f.printf("programName=%s\n", currentProgram->name);
  f.printf("currentSegmentIndex=%u\n", currentSegmentIndex);
  f.printf("programRunning=%d\n", programRunning ? 1 : 0);
  f.printf("segmentElapsed=%lu\n", segmentElapsed);
  f.printf("inHold=%d\n", inHold ? 1 : 0);
  f.printf("holdElapsed=%lu\n", holdElapsed);
  f.close();
  lastStateSaveMillis = now;
  lastSavedSetpoint = pidSetpoint;
  stateDirty = false;
}

void loadState() {
  if (!LittleFS.begin()) return;
  if (!LittleFS.exists(STATE_FILE)) return;
  File f = LittleFS.open(STATE_FILE, "r");
  if (!f) return;
  String line;
  String programName = "";
  unsigned long segmentElapsed = 0;
  unsigned long holdElapsed = 0;
  while (f.available()) {
    line = f.readStringUntil('\n');
    line.trim();
    if (line.startsWith("pidSetpoint=")) {
      pidSetpoint = line.substring(strlen("pidSetpoint=")).toFloat();
    } else if (line.startsWith("programName=")) {
      programName = line.substring(strlen("programName="));
    } else if (line.startsWith("currentSegmentIndex=")) {
      currentSegmentIndex = (uint8_t)line.substring(strlen("currentSegmentIndex=")).toInt();
    } else if (line.startsWith("programRunning=")) {
      programRunning = line.substring(strlen("programRunning=")).toInt() ? true : false;
    } else if (line.startsWith("segmentElapsed=")) {
      segmentElapsed = (unsigned long)line.substring(strlen("segmentElapsed=")).toInt();
    } else if (line.startsWith("inHold=")) {
      inHold = line.substring(strlen("inHold=")).toInt() ? true : false;
    } else if (line.startsWith("holdElapsed=")) {
      holdElapsed = (unsigned long)line.substring(strlen("holdElapsed=")).toInt();
    }
  }
  f.close();

  // map program name to known programs (default and secondary)
  if (programName.length() > 0) {
    if (programName == String(defaultProgram.name)) currentProgram = &defaultProgram;
    else if (programName == String(SecondaryProgram.name)) currentProgram = &SecondaryProgram;
    else currentProgram = &defaultProgram; // fallback
  }

  // restore timing
  unsigned long now = millis();
  segmentStartMillis = (segmentElapsed > 0) ? (now - segmentElapsed * 1000UL) : now;
  if (inHold) holdStartMillis = (holdElapsed > 0) ? (now - holdElapsed * 1000UL) : now;

  lastSavedSetpoint = pidSetpoint;
  stateDirty = false;
}

//
// Utility functions
//
bool isButtonPressed() {
  int v = analogRead(PIN_BUTTON_ADC);
  return v < ADC_BUTTON_THRESHOLD;
}

bool getButtonStateDebounced() {
  static bool debouncedState = false;
  static unsigned long lastChangeTime = 0;
  
  bool currentState = isButtonPressed();
  
  if (currentState != debouncedState) {
    if (millis() - lastChangeTime >= BUTTON_DEBOUNCE_MS) {
      debouncedState = currentState;
      lastChangeTime = millis();
    }
  } else {
    lastChangeTime = millis();
  }
  
  return debouncedState;
}

float readTemperatureC() {
  // MAX6675 returns NaN on error in some libraries; here we guard values
  double t = thermocouple.readCelsius();
  if (!isfinite(t) || t < SENSOR_MIN_VALID || t > SENSOR_MAX_VALID) {
    sensorFault = true;
    return NAN;
  }
  sensorFault = false;
  return (float)t;
}

void ssdInit() {
  Wire.begin(PIN_SDA, PIN_SCL); // initialize I2C with custom pins
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    // Display init failed; continue but UI won't work
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
}

void drawUI() {
  unsigned long now = millis();
  display.clearDisplay();

  // Large temperature display (left)
  display.setTextSize(2);
  display.setCursor(0, 0);
  if (isnan(pidInput)) display.print("T: --C");
  else display.printf("T:%4.0fC", pidInput);

  // Small SSR/Status indicators (top-right)
  display.setTextSize(1);
  display.setCursor(100, 0);
  display.print(SSR_Status ? "SSR" : "   ");
  display.setCursor(100, 9);
  display.print(getButtonStateDebounced() ? "BTN" : "   ");

  // Setpoint and program name
  display.setTextSize(2);
  display.setCursor(0, 18);
  display.printf("SP:%4.0fC", pidSetpoint);
  display.setTextSize(1);
  display.setCursor(70, 36);
  display.print(currentProgram->name);

  // Segment info and mode
  display.setCursor(0, 48);
  if (sensorFault) {
    display.print("Sensor Fault");
  } else if (programRunning) {
    Segment &seg = currentProgram->segments[currentSegmentIndex];
    display.printf("Rph:%0.0f %u/%u %s", currentProgram->segments[currentSegmentIndex].rate_c_per_hour, currentSegmentIndex+1, currentProgram->seqCount, inPause ? "PAUSE" : inHold ? "HOLD" : "RAMP");
    // if in hold, show remaining time
    if (inHold) {
      unsigned long holdElapsed = (now - holdStartMillis) / 1000UL;
      unsigned long remain = (holdElapsed >= seg.hold_seconds) ? 0 : (seg.hold_seconds - holdElapsed);
      unsigned int mm = remain / 60;
      unsigned int ss = remain % 60;
      display.setCursor(70, 48);
      display.printf("t:%02u:%02u", mm, ss);
    } else {
      // show target temp and rate
      display.setCursor(0, 36);
      display.printf("Tgt:%0.0f", seg.target_c);
    }
  } else {
    display.printf("Idle %s", WiFi.localIP().toString().c_str());
  }

  display.display();
}

//
// PID compute (simple discrete PID with anti-windup)
void pidCompute(unsigned long nowMs) {
  float dt = (nowMs - lastPidMillis) / 1000.0;
  if (dt <= 0) return;
  float error = pidSetpoint - pidInput;
  integralTerm += Ki * error * dt;
  // anti-windup clamp integral
  if (integralTerm > 100.0) integralTerm = 100.0;
  if (integralTerm < 0.0) integralTerm = 0.0;
  float derivative = (error - lastError) / dt;
  pidOutput = Kp * error + integralTerm + Kd * derivative;
  // clamp output to 0..100%
  if (pidOutput > 100.0) pidOutput = 100.0;
  if (pidOutput < 0.0) pidOutput = 0.0;
  lastError = error;
  lastPidMillis = nowMs;
}

//
// Program control functions
//
void startProgram(Program* p) {
  currentProgram = p;
  programRunning = true;
  currentSegmentIndex = 0;
  segmentStartTemp = isnan(pidInput) ? pidSetpoint : pidInput; // begin ramp from measured temp
  segmentStartMillis = millis();
  inHold = false;
  // initialize setpoint to current temp to begin ramp calculation
  inPause = false;
  stateDirty = true;
  
  // Log program start
  logger.onProgramStart();
}

void stopProgram() {
  programRunning = false;
  inHold = false;
  stateDirty = true;
  pidSetpoint = 25.0; // reset setpoint to ambient (or could hold last temp)
  
  // Log program stop
  logger.onProgramStop();
}

void pauseProgram() {
  programRunning = programRunning; // keep running state
  inHold = inHold; // keep hold state
  inPause = !inPause; // toggle pause state
  if (inPause) {
    pauseMillis = millis();
  } else {
    // resuming: adjust timers to account for pause duration
    unsigned long pauseDuration = millis() - pauseMillis;
    segmentStartMillis += pauseDuration;
    if (inHold) holdStartMillis += pauseDuration;
  }
  stateDirty = true;
}

void advanceToNextSegment() {
  if (currentSegmentIndex + 1 < currentProgram->seqCount) {
    currentSegmentIndex++;
    segmentStartMillis = millis();
    segmentStartTemp = pidSetpoint; // start from last setpoint
    inHold = false;
    stateDirty = true;
  } else {
    // program finished
    stopProgram();
  }
}

float computeSetpointForCurrentSegment(unsigned long nowMs) {
  Segment &seg = currentProgram->segments[currentSegmentIndex];
  unsigned long elapsed = (nowMs - segmentStartMillis) / 1000UL; // seconds
  float rate_per_s = seg.rate_c_per_hour / 3600.0; // °C per second
  float sp = segmentStartTemp + rate_per_s * (float)elapsed;
  if (seg.rate_c_per_hour >= 0) {
    if (sp >= seg.target_c) {
      sp = seg.target_c;
      // enter hold if not already
      if (!inHold) {
        inHold = true;
        holdStartMillis = nowMs;
      }
    }
  } else {
    if (sp <= seg.target_c) {
      sp = seg.target_c;
      if (!inHold) {
        inHold = true;
        holdStartMillis = nowMs;
      }
    }
  }
  return sp;
}

void handleProgramProgress(unsigned long nowMs) {
  if (!programRunning || inPause) return;
  Segment &seg = currentProgram->segments[currentSegmentIndex];
  if (!inHold) {
    pidSetpoint = computeSetpointForCurrentSegment(nowMs);
  } else {
    // in hold: count down
    unsigned long holdElapsed = (nowMs - holdStartMillis) / 1000UL;
    if (holdElapsed >= seg.hold_seconds) {
      // move to next segment or finish
      advanceToNextSegment();
      if (programRunning) {
        // initialize setpoint for new segment (immediately compute)
        pidSetpoint = computeSetpointForCurrentSegment(nowMs);
      }
    } else {
      // keep setpoint at target during hold
      pidSetpoint = seg.target_c;
    }
  }
}

//
// SSR time-proportional control
//
void updateSSR(unsigned long nowMs) {
  // safety: enforce emergency cutoff and sensor faults
  if (sensorFault || (!isnan(pidInput) && pidInput > ABSOLUTE_MAX_TEMP) ) {
    digitalWrite(PIN_SSR, LOW);
    return;
  }
  if (!programRunning) {
    // if not running, ensure SSR is off
    digitalWrite(PIN_SSR, LOW);
    SSR_Status = false;
    return;
  }
  // Manage window
  if (nowMs - windowStartMillis > CONTROL_WINDOW_MS) {
    windowStartMillis += CONTROL_WINDOW_MS;
  }
  unsigned long onTime = (unsigned long)( (pidOutput / 100.0) * (float)CONTROL_WINDOW_MS );
  if (nowMs - windowStartMillis < onTime) {
    digitalWrite(PIN_SSR, HIGH);
    SSR_Status = true;
  } else {
    digitalWrite(PIN_SSR, LOW);
    SSR_Status = false;
  }
}

//
// Setup & Loop
//
void setup() {
  Serial.begin(115200);
  delay(100);
  pinMode(PIN_SSR, OUTPUT);
  digitalWrite(PIN_SSR, LOW);

  ssdInit();

  // read initial temperature
  delay(200);
  pidInput = readTemperatureC();
  if (isnan(pidInput)) pidInput = 25.0;
  pidSetpoint = pidInput;

  // load persisted state (if exists)
  LittleFS.begin();
  loadState();

  // Initialize data logger
  logger.begin();

  // Initialize WiFi and OTA
  wirelessManager.begin();

  lastPidMillis = millis();
  windowStartMillis = millis();

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0,0);
  display.println("Kiln\nController");
  display.setCursor(0,34);
  display.setTextSize(1);
  display.println("Kp: " + String(Kp) + "\nKi: " + String(Ki) + "\nKd: " + String(Kd));
  display.display();
  delay(5000);
}


void loop() {
  unsigned long now = millis();

  // Handle WiFi and OTA
  wirelessManager.handleWiFi();

  // Button handling (Long press = cycle program; short press = start/stop)
  bool btn = getButtonStateDebounced();
  if (btn != buttonPrev) {
    if (btn) {
      // pressed now
      buttonPressStart = now;
    } else {
      // released now
      unsigned long held = now - buttonPressStart;
      if (held >= LONG_PRESS_MS) {
        // long press -> cycle program selection
        bool wasRunning = programRunning;
        if (wasRunning) stopProgram();

        if (currentProgram == &defaultProgram) {
          currentProgram = &SecondaryProgram;
        } else {
          currentProgram = &defaultProgram;
        }
      } else {
        // short press -> start/stop program
        if (programRunning) {
          pauseProgram();
        } else {
          startProgram(currentProgram);
        }
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

    // program progression update
    handleProgramProgress(now);

    // compute PID
    pidCompute(now);

    // safety: don't allow setpoint exceed absolute max
    if (pidSetpoint > ABSOLUTE_MAX_TEMP) pidSetpoint = ABSOLUTE_MAX_TEMP;

    // Update SSR using time-proportional control
    updateSSR(now);

    // UI update
    drawUI();

    // Serial logging
    Serial.printf("T=%.2f SP=%.2f Out=%.1f Prog=%s Seg=%u Fault=%d\n",
                  isnan(pidInput)?0.0f:pidInput, pidSetpoint, pidOutput, currentProgram->name, currentSegmentIndex+1, sensorFault?1:0);

    // Data logging (per-second when program is running or when there's activity)
    if (programRunning || !sensorFault) {
      const char* status;
      if (sensorFault) {
        status = "FAULT";
      } else if (!programRunning) {
        status = "IDLE";
      } else if (inPause) {
        status = "PAUSE";
      } else if (inHold) {
        status = "HOLD";
      } else {
        status = "RAMP";
      }
      
      // Calculate total program duration
      uint32_t totalDuration = 0;
      if (programRunning && segmentStartMillis > 0) {
        totalDuration = (now - segmentStartMillis) / 1000UL;
      }
      
      Segment &seg = currentProgram->segments[currentSegmentIndex];
      logger.logData(
        isnan(pidInput) ? 0.0 : pidInput,  // temperature
        pidSetpoint,                        // setpoint
        seg.rate_c_per_hour,               // rate
        seg.target_c,                      // target
        totalDuration,                     // duration
        currentProgram->name,              // program
        status                             // status
      );
    }

    // mark dirty if setpoint changed significantly since last save
    if (fabs(pidSetpoint - lastSavedSetpoint) > 0.5f) stateDirty = true;

    // flush state to flash periodically if dirty
    if (stateDirty && (now - lastStateSaveMillis >= STATE_SAVE_INTERVAL_MS)) {
      saveState();
    }
  }
  delay(10); // small delay to avoid busy loop; adjust as needed
}

// Function to get kiln status as JSON string (for web API)
String getKilnStatusJSON() {
  StaticJsonDocument<256> doc;
  
  // Temperature and setpoint
  doc["temperature"] = isnan(pidInput) ? 0.0 : pidInput;
  doc["setpoint"] = pidSetpoint;
  
  // Program info
  doc["program"] = currentProgram->name;
  doc["segment"] = currentSegmentIndex + 1;
  doc["segmentCount"] = currentProgram->seqCount;
  
  // Current segment details
  Segment &seg = currentProgram->segments[currentSegmentIndex];
  doc["rate"] = seg.rate_c_per_hour;
  doc["target"] = seg.target_c;
  
  // Status
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
  
  // Additional info
  doc["running"] = programRunning;
  doc["holding"] = inHold;
  doc["paused"] = inPause;
  doc["ssrStatus"] = SSR_Status;
  doc["pidOutput"] = pidOutput;
  
  String response;
  serializeJson(doc, response);
  return response;
}
