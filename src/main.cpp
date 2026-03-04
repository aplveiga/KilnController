// Kiln controller for ESP8266 + SSD1306(I2C on D5/D6) + MAX6675(SO D2, CS D3, SCK D4) + SSR D0 + Button A0
// Minimal, self-contained implementation based on REQUIREMENTS.md
// Requires libraries: Adafruit_SSD1306, Adafruit_GFX, MAX6675 (install via Library Manager)

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <max6675.h>
#include <LittleFS.h>
#include <PID_v1.h>
#include <wireless.h>
#include <program_manager.h>
#include <data_logger.h>

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

// PID objects and variables
double pidSetpoint = 25.0;
double pidInput = 25.0;
double pidOutput = 0.0;
PID kilnPID(&pidInput, &pidOutput, &pidSetpoint, Kp, Ki, Kd, DIRECT);

// Control window for SSR (ms)
const unsigned long CONTROL_WINDOW_MS = 10000UL; // 10s window

// Sampling / update intervals
const unsigned long TEMP_INTERVAL_MS = 1000UL;  // temperature read, PID compute
const unsigned long DATA_LOG_INTERVAL_MS = 60000UL; // data logging every 60 seconds

// Safety limits
const float ABSOLUTE_MAX_TEMP = 1200.0;   // emergency cutoff (°C)
const float SENSOR_MIN_VALID = -50.0;
const float SENSOR_MAX_VALID = 1100.0;

// Button thresholds (ADC 0..1023)
const int ADC_BUTTON_THRESHOLD = 200;   // pressed if ADC < threshold (adjust hardware)
const unsigned long LONG_PRESS_MS = 1000UL;
unsigned long lastTempMillis = 0;
unsigned long lastDataLogMillis = 0;  // Track last data log time
unsigned long buttonPressStart = 0;
bool buttonPrev = false;
const unsigned long BUTTON_DEBOUNCE_MS = 50;

// Runtime program state - loaded from flash
Program currentProgram;
bool programRunning = false;
uint8_t currentSegmentIndex = 0;
float segmentStartTemp = 25.0;     // captured at segment start
unsigned long segmentStartMillis = 0;
unsigned long holdStartMillis = 0;
unsigned long pauseMillis = 0;
bool inHold = false;
bool inPause = false;
bool SSR_Status = false;

// Persistence (LittleFS)
const char *STATE_FILE = "/kiln_state.txt";
bool stateDirty = false;
unsigned long lastStateSaveMillis = 0;
const unsigned long STATE_SAVE_INTERVAL_MS = 10000UL; // throttle writes
float lastSavedSetpoint = 0.0;

// SSR time-proportional control
unsigned long windowStartMillis = 0;
unsigned long lastSSRChangeTime = 0;
unsigned long SSR_CHANGE_RATE_LIMIT = 100UL; // Minimum 100ms between state changes

// Function to set SSR rate limiting from web interface
void setSSRRateLimit(unsigned long rateMs) {
  if (rateMs >= 100 && rateMs <= 10000) {  // 100ms to 10 seconds
    SSR_CHANGE_RATE_LIMIT = rateMs;
    Serial.printf("[Kiln] SSR rate limit updated to %lu ms\n", SSR_CHANGE_RATE_LIMIT);
  }
}

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
  f.printf("programName=%s\n", currentProgram.name);
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

  // map program name to known programs - load from flash
  if (programName.length() > 0) {
    yield();
    if (!programManager.loadProgram(programName.c_str(), currentProgram)) {
      // Load failed, try 9-step fallback
      Serial.println("[Kiln] Failed to load program, using 9-step");
      yield();
      if (!programManager.loadProgram("9-step", currentProgram)) {
        strcpy(currentProgram.name, "UNKNOWN");
        currentProgram.seqCount = 0;
      }
    }
  }

  // restore timing
  unsigned long now = millis();
  segmentStartMillis = (segmentElapsed > 0) ? (now - segmentElapsed * 1000UL) : now;
  if (inHold) holdStartMillis = (holdElapsed > 0) ? (now - holdElapsed * 1000UL) : now;

  lastSavedSetpoint = pidSetpoint;
  stateDirty = false;
}

void loadPIDValues() {
  if (!LittleFS.exists("/pid.json")) {
    Serial.println("[Kiln] No saved PID values, using defaults");
    return;
  }
  
  yield();
  File file = LittleFS.open("/pid.json", "r");
  if (!file) {
    Serial.println("[Kiln] Failed to open PID file");
    return;
  }
  
  DynamicJsonDocument doc(256);
  yield();
  if (deserializeJson(doc, file) != DeserializationError::Ok) {
    Serial.println("[Kiln] Failed to parse PID file");
    file.close();
    yield();
    return;
  }
  
  if (doc.containsKey("kp")) Kp = doc["kp"] | 15.5f;
  if (doc.containsKey("ki")) Ki = doc["ki"] | 13.1f;
  if (doc.containsKey("kd")) Kd = doc["kd"] | 1.20f;
  
  file.close();
  yield();
  Serial.printf("[Kiln] Loaded PID: Kp=%.2f, Ki=%.2f, Kd=%.2f\n", Kp, Ki, Kd);
  
  // Update PID controller with loaded tuning parameters
  // (This will be called again in setup() after PID object creation, so it's safe)
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
  static unsigned long changeDetectedTime = 0;
  static bool inDebounceWait = false;
  
  bool currentState = isButtonPressed();
  
  if (currentState != debouncedState) {
    // State has changed - start debounce timer
    if (!inDebounceWait) {
      inDebounceWait = true;
      changeDetectedTime = millis();
    }
    
    // Wait for debounce period
    if (millis() - changeDetectedTime >= BUTTON_DEBOUNCE_MS) {
      debouncedState = currentState;
      inDebounceWait = false;
    }
  } else {
    // State is stable - reset debounce wait
    inDebounceWait = false;
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
  bool btn_pressed = getButtonStateDebounced();
  display.print(btn_pressed ? "BTN" : "   ");
  // Draw 2px underline for long press
  if (btn_pressed) {
    unsigned long pressDuration = now - buttonPressStart;
    if (pressDuration > LONG_PRESS_MS) {
      display.fillRect(98, 18, 30, 2, SSD1306_WHITE);
    }
  }

  // Setpoint and program name
  display.setTextSize(2);
  display.setCursor(0, 18);
  display.printf("SP:%4.0fC", pidSetpoint);
  display.setTextSize(1);
  display.setCursor(70, 36);
  display.print(currentProgram.name);

  // Segment info and mode
  display.setCursor(0, 48);
  if (sensorFault) {
    display.print("Sensor Fault");
  } else if (programRunning) {
    Segment &seg = currentProgram.segments[currentSegmentIndex];
    display.printf("R:%0.0f %u/%u %s", currentProgram.segments[currentSegmentIndex].rate_c_per_hour, currentSegmentIndex+1, currentProgram.seqCount, inPause ? "P" : inHold ? "H" : "R");
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
// PID compute using Arduino-PID-Library (robust, derivative kick filtered, anti-windup)
void pidCompute(unsigned long nowMs) {
  // Update PID at 1000ms intervals (1Hz sample rate)
  kilnPID.Compute();
}

// Forward declarations for program control functions
void startProgram(Program* p);
void pauseProgram();
void stopProgram();

//
// Button Action Functions (called from physical button or dashboard)
//
void buttonActionStartStop() {
  if (programRunning) {
    pauseProgram();
    Serial.println("[Kiln] Button: PAUSE");
  } else {
    Serial.printf("[Kiln] Button: START (calling startProgram with '%s')\n", currentProgram.name);
    startProgram(&currentProgram);
  }
  // Update display immediately
  drawUI();
}

void buttonActionCycleProgram() {
  bool wasRunning = programRunning;
  if (wasRunning) stopProgram();

  // Get list of all available programs
  Dir dir = LittleFS.openDir("/programs");
  String programs[20];  // Support up to 20 programs
  int programCount = 0;
  int currentIndex = 0;  // Default to first program
  
  Serial.println("[Kiln] === Cycling Programs ===");
  Serial.printf("[Kiln] Current program: '%s'\n", currentProgram.name);
  
  // Enumerate all programs
  while (dir.next() && programCount < 20) {
    String filename = dir.fileName();
    Serial.printf("[Kiln] Found file: '%s'\n", filename.c_str());
    
    if (filename.endsWith(".json")) {
      // Remove leading slash if present
      int startPos = (filename[0] == '/') ? 1 : 0;
      // Remove .json extension
      String programName = filename.substring(startPos, filename.length() - 5);
      programs[programCount] = programName;
      
      Serial.printf("[Kiln]   -> Program: '%s' (idx=%d)\n", programName.c_str(), programCount);
      
      // Find current program index (using strcmp for reliable comparison)
      if (strcmp(programName.c_str(), currentProgram.name) == 0) {
        currentIndex = programCount;
        Serial.printf("[Kiln]   -> MATCHED current program at index %d\n", currentIndex);
      }
      programCount++;
    }
    yield();
  }
  
  Serial.printf("[Kiln] Total programs found: %d, current index: %d\n", programCount, currentIndex);
  
  // If we have programs, cycle to the next one
  if (programCount > 0) {
    int nextIndex = (currentIndex + 1) % programCount;  // Wrap around at end
    
    Serial.printf("[Kiln] Cycling: index %d -> %d\n", currentIndex, nextIndex);
    Serial.printf("[Kiln] Next program name: '%s'\n", programs[nextIndex].c_str());
    
    yield();
    if (programManager.loadProgram(programs[nextIndex].c_str(), currentProgram)) {
      programManager.setLastSelectedProgram(programs[nextIndex].c_str());
      Serial.printf("[Kiln] ✓ Cycled to: '%s' (loaded and selected)\n", programs[nextIndex].c_str());
      // Update display immediately
      drawUI();
    } else {
      Serial.printf("[Kiln] ✗ ERROR: Failed to load program '%s'\n", programs[nextIndex].c_str());
    }
  } else {
    Serial.println("[Kiln] ✗ No programs found in /programs directory");
  }
  
  Serial.println("[Kiln] === Cycle Complete ===");
  yield();
}

//
// Program control functions
//
void startProgram(Program* p) {
  // Copy program data from pointer
  if (p == nullptr) {
    Serial.println("[Kiln] Error: Program pointer is null");
    return;
  }
  
  // Verify program has segments
  if (p->seqCount == 0) {
    Serial.printf("[Kiln] Error: Program '%s' has no segments\n", p->name);
    return;
  }
  
  currentProgram = *p;  // Copy struct data
  programRunning = true;
  currentSegmentIndex = 0;
  segmentStartTemp = isnan(pidInput) ? pidSetpoint : pidInput;
  segmentStartMillis = millis();
  windowStartMillis = millis();  // Reset window for fresh SSR control
  lastSSRChangeTime = millis();  // Reset rate limiting timer - allow immediate SSR change on start
  inHold = false;
  inPause = false;
  stateDirty = true;
  
  // Debug output
  Serial.printf("[Kiln] Program STARTED: %s (%d segments), setpoint=%.1f\n", 
    currentProgram.name, currentProgram.seqCount, pidSetpoint);
}

void stopProgram() {
  programRunning = false;
  inHold = false;
  inPause = false;
  stateDirty = true;
  pidSetpoint = 25.0; // reset setpoint to ambient (or could hold last temp)
  windowStartMillis = millis();  // Reset window for next program
  lastSSRChangeTime = millis();  // Reset rate limiting timer
  
  Serial.println("[Kiln] Program STOPPED");
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
  if (currentSegmentIndex + 1 < currentProgram.seqCount) {
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
  Segment &seg = currentProgram.segments[currentSegmentIndex];
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
  Segment &seg = currentProgram.segments[currentSegmentIndex];
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
// SSR time-proportional control with rate limiting
//
void updateSSR(unsigned long nowMs) {
  static unsigned long lastDebugLog = 0;
  bool shouldLog = (nowMs - lastDebugLog > 5000);  // Log every 5 seconds
  
  // safety: enforce emergency cutoff and sensor faults
  if (sensorFault || (!isnan(pidInput) && pidInput > ABSOLUTE_MAX_TEMP) ) {
    if (SSR_Status) {  // Only change if currently ON
      if (nowMs - lastSSRChangeTime >= SSR_CHANGE_RATE_LIMIT) {
        digitalWrite(PIN_SSR, LOW);
        SSR_Status = false;
        lastSSRChangeTime = nowMs;
        Serial.println("[SSR] OFF - Safety cutoff");
      }
    }
    return;
  }
  if (!programRunning) {
    // if not running, ensure SSR is off
    if (SSR_Status) {  // Only change if currently ON
      if (nowMs - lastSSRChangeTime >= SSR_CHANGE_RATE_LIMIT) {
        digitalWrite(PIN_SSR, LOW);
        SSR_Status = false;
        lastSSRChangeTime = nowMs;
        Serial.println("[SSR] OFF - Program not running");
      }
    }
    if (shouldLog) {
      Serial.println("[SSR] Program NOT running - SSR disabled");
      lastDebugLog = nowMs;
    }
    return;
  }
  
  // Manage window - ensure windowStartMillis is properly initialized  
  if (windowStartMillis == 0) {
    windowStartMillis = nowMs;
  }
  
  // Only advance window if time has passed
  unsigned long timeSinceWindowStart = nowMs - windowStartMillis;
  if (timeSinceWindowStart >= CONTROL_WINDOW_MS) {
    // Advance window by complete cycles only
    unsigned long cyclesToAdvance = (timeSinceWindowStart / CONTROL_WINDOW_MS);
    windowStartMillis += (cyclesToAdvance * CONTROL_WINDOW_MS);
    timeSinceWindowStart = nowMs - windowStartMillis;
  }
  
  // Calculate desired SSR state based on PID output (using integer math for efficiency)
  unsigned long onTime = (pidOutput * CONTROL_WINDOW_MS) / 100UL;
  bool desiredState = (timeSinceWindowStart < onTime);
  
  if (shouldLog) {
    Serial.printf("[SSR] Program Running: PID=%.1f%% OnTime=%lums WindowPos=%lums Desired=%s Actual=%s\n", 
                  pidOutput, onTime, timeSinceWindowStart, desiredState?"ON":"OFF", SSR_Status?"ON":"OFF");
    lastDebugLog = nowMs;
  }
  
  // Apply rate limiting: only change state if rate limit has elapsed since last change
  if (desiredState != SSR_Status) {
    if (nowMs - lastSSRChangeTime >= SSR_CHANGE_RATE_LIMIT) {
      // Enough time has passed, allow state change
      if (desiredState) {
        digitalWrite(PIN_SSR, HIGH);
        SSR_Status = true;
        Serial.println("[SSR] ON");
      } else {
        digitalWrite(PIN_SSR, LOW);
        SSR_Status = false;
        Serial.println("[SSR] OFF");
      }
      lastSSRChangeTime = nowMs;
    } else if (shouldLog) {
      unsigned long remaining = SSR_CHANGE_RATE_LIMIT - (nowMs - lastSSRChangeTime);
      Serial.printf("[SSR] Rate limited - waiting %lums\n", remaining);
    }
  }
}

//
// Program Management
//
void initializePrograms() {
  // Create default programs if they don't exist
  Program defaultProg;
  
  // 9-step program
  strcpy(defaultProg.name, "9-step");
  defaultProg.seqCount = 9;
  defaultProg.segments[0] = {50.0, 100.0, 30 * 60};
  defaultProg.segments[1] = {200.0, 300.0, 0};
  defaultProg.segments[2] = {75.0, 550.0, 30 * 60};
  defaultProg.segments[3] = {25.0, 573.0, 30 * 60};
  defaultProg.segments[4] = {50.0, 600.0, 0};
  defaultProg.segments[5] = {100.0, 900.0, 0};
  defaultProg.segments[6] = {50.0, 1000.0, 0};
  defaultProg.segments[7] = {25.0, 1050.0, 0};
  defaultProg.segments[8] = {25.0, 1100.0, 15 * 60};
  
  // Save 9-step if it doesn't exist
  yield();
  if (!programManager.loadProgram("9-step", defaultProg)) {
    yield();
    programManager.saveProgram(defaultProg);
    yield();
    Serial.println("[Kiln] Created default 9-step program");
  }
  
  yield();
  // 4-step program
  strcpy(defaultProg.name, "4-step");
  defaultProg.seqCount = 4;
  defaultProg.segments[0] = {50.0, 100.0, 15 * 60};
  defaultProg.segments[1] = {100.0, 573.0, 30 * 60};
  defaultProg.segments[2] = {100.0, 900.0, 0};
  defaultProg.segments[3] = {50.0, 1100.0, 30 * 60};
  
  // Save 4-step if it doesn't exist
  yield();
  if (!programManager.loadProgram("4-step", defaultProg)) {
    yield();
    programManager.saveProgram(defaultProg);
    yield();
    Serial.println("[Kiln] Created default 4-step program");
  }
  
  yield();
  // Load last selected program
  yield();
  if (!programManager.loadLastSelectedProgram(currentProgram)) {
    // If no last selected program is saved, load 9-step
    yield();
    if (programManager.loadProgram("9-step", currentProgram)) {
      programManager.setLastSelectedProgram("9-step");
      Serial.println("[Kiln] Loaded 9-step as last selected program");
    } else {
      Serial.println("[Kiln] ERROR: Failed to load any program!");
      strcpy(currentProgram.name, "EMPTY");
      currentProgram.seqCount = 0;
    }
  }
  
  Serial.printf("[Kiln] Loaded program: %s (%d segments)\n", currentProgram.name, currentProgram.seqCount);
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
  yield();
  loadState();
  yield();
  loadPIDValues();  // Load saved PID parameters
  yield();
  
  // Initialize PID controller
  kilnPID.SetMode(AUTOMATIC);           // Enable PID
  kilnPID.SetOutputLimits(0, 100);      // Output: 0-100% PWM duty cycle
  kilnPID.SetSampleTime(1000);          // 1 second sampling interval
  kilnPID.SetTunings(Kp, Ki, Kd);       // Apply loaded/default tuning parameters
  
  // Initialize program manager and load default program
  initializePrograms();
  yield();

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0,0);
  display.println("Kiln\nController");
  display.setCursor(0,34);
  display.setTextSize(1);
  display.println("Kp: " + String(Kp) + "\nKi: " + String(Ki) + "\nKd: " + String(Kd));
  display.display();

  // Initialize WiFi and OTA
  wirelessManager.begin();

  windowStartMillis = millis();
  delay(5000);
}


void loop() {
  unsigned long now = millis();

  // Handle WiFi and OTA
  wirelessManager.handleWiFi();

  // Button handling (Long press > 2 seconds = cycle program; short press = start/stop)
  bool btn = getButtonStateDebounced();
  if (btn != buttonPrev) {
    if (btn) {
      // pressed now
      buttonPressStart = now;
      Serial.println("[Kiln] Button PRESSED");
    } else {
      // released now
      unsigned long held = now - buttonPressStart;
      Serial.printf("[Kiln] Button released after %lu ms\n", held);
      if (held > LONG_PRESS_MS) {
        // long press -> cycle program selection
        Serial.println("[Kiln] LONG PRESS detected -> Cycling program");
        buttonActionCycleProgram();
      } else {
        // short press -> start/stop program
        Serial.println("[Kiln] SHORT PRESS detected -> Start/Stop");
        buttonActionStartStop();
      }
    }
    buttonPrev = btn;
  }

  // Update SSR on every loop iteration for accurate rate limiting
  updateSSR(now);

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

    // UI update
    drawUI();

    // Serial logging
    Serial.printf("T=%.2f SP=%.2f Out=%.1f Prog=%s Seg=%u Fault=%d\n",
                  isnan(pidInput)?0.0f:pidInput, pidSetpoint, pidOutput, currentProgram.name, currentSegmentIndex+1, sensorFault?1:0);

    // mark dirty if setpoint changed significantly since last save
    if (fabs(pidSetpoint - lastSavedSetpoint) > 0.5f) stateDirty = true;

    // flush state to flash periodically if dirty
    if (stateDirty && (now - lastStateSaveMillis >= STATE_SAVE_INTERVAL_MS)) {
      saveState();
    }

    // Data logging every 60 seconds
    if (now - lastDataLogMillis >= DATA_LOG_INTERVAL_MS) {
      lastDataLogMillis = now;
      
      // Get current status string
      const char* statusStr = "IDLE";
      if (sensorFault) {
        statusStr = "FAULT";
      } else if (programRunning) {
        if (inPause) {
          statusStr = "PAUSE";
        } else if (inHold) {
          statusStr = "HOLD";
        } else {
          statusStr = "RAMP";
        }
      }
      
      // Log the entry with current kiln state
      float target = (currentSegmentIndex < currentProgram.seqCount) 
                      ? currentProgram.segments[currentSegmentIndex].target_c 
                      : 0.0;
      
      dataLogger.logEntry(
        time(nullptr),
        isnan(pidInput) ? 0.0 : pidInput,
        pidSetpoint,
        target,
        currentProgram.name,
        statusStr
      );
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
  doc["program"] = currentProgram.name;
  doc["segment"] = currentSegmentIndex + 1;
  doc["segmentCount"] = currentProgram.seqCount;
  
  // Current segment details
  Segment &seg = currentProgram.segments[currentSegmentIndex];
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
  
  // Diagnostic info - calculate window position using same logic as updateSSR
  unsigned long now = millis();
  unsigned long windowPos = 0;
  if (windowStartMillis == 0) {
    windowPos = 0;
  } else {
    unsigned long elapsed = now - windowStartMillis;
    if (elapsed < CONTROL_WINDOW_MS) {
      windowPos = elapsed;
    } else {
      windowPos = elapsed % CONTROL_WINDOW_MS;
    }
  }
  
  doc["windowPos"] = windowPos;
  doc["windowSize"] = CONTROL_WINDOW_MS;
  doc["ssrRateLimit"] = SSR_CHANGE_RATE_LIMIT;
  doc["timeSinceSSRChange"] = (unsigned long)(now - lastSSRChangeTime);
  
  String response;
  serializeJson(doc, response);
  return response;
}
