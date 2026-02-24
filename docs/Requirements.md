# Kiln Controller — Requirements (implemented)

Brief: build a kiln controller using ESP8266 + SSD1306 OLED + MAX6675 thermocouple + SSR. The firmware implements PID-based control, a program runner with persisted program definitions, an OLED UI, a single-button interface on A0 and a small web UI for Wi‑Fi configuration and status. This document describes the implemented behavior (matching `src/KilnController.cpp`).

## Hardware & Wiring
- MCU: ESP8266 (NodeMCU/ESP-12 variant)
- Display: SSD1306 (I2C)
  - SDA -> D5 (GPIO14)
  - SCL -> D6 (GPIO12)
- Thermocouple interface: MAX6675 (SPI-like)
  - SO (MISO) -> D2 (GPIO4)
  - CS  -> D3 (GPIO0)  (note: D3 is boot-associated)
  - SCK -> D4 (GPIO2)
- SSR (heater) output:
  - SSR control -> D0 (GPIO16)
- User button:
  - Button -> A0 (Analog input). The firmware treats A0 as a single button (pressed when ADC reading < ~200). The code provides debouncing, short-press and long-press behavior.
- Power: ensure proper isolated AC wiring and mains safety (earth, fuses, physical separation)

## Functional Requirements (implemented)
- Read temperature from MAX6675 at 1 s interval (implemented).
- Display current temperature, setpoint, program name/status, segment info and small status indicators on the SSD1306.
- Implement PID loop to drive SSR to reach and maintain setpoint using a simple discrete PID with anti-windup.
  - SSR is driven using time-proportional control (implemented control window = 2s by default).
  - Default PID gains are provided in code (Kp=40, Ki=10, Kd=4) and can be tuned in source.
- Provide simple program runner with start/stop/pause and automatic progression through segments.
- Persist runtime state and setpoint using LittleFS (`/kiln_state.txt`).
- Safety and fault handling implemented:
  - Absolute emergency cutoff at 1200 °C (configured in code as ABSOLUTE_MAX_TEMP).
  - Thermocouple sensor error detection: on invalid reading, sensorFault is set and SSR is disabled.
  - No external hardware watchdog code is implemented in firmware (MCU-level watchdog not explicitly used here).
- Note: the explicit OLED message "Temp < 100 °C" required by the original spec is not implemented; the UI shows sensor faults, running/idle state and segment information.

Additional implemented functions:
- Program definitions are stored in JSON on LittleFS (`/programs.json`) and loaded on boot. If the file is missing the firmware creates default example programs and writes `/programs.json`.

**Note on Wi-Fi and Web UI (NOT currently implemented):**
- Wi-Fi connectivity, SoftAP mode, and web server/API are **NOT** currently implemented in the firmware code.
- `/wifi.cfg` file handling is **NOT** implemented.
- Web endpoints (GET /, GET /status, POST /save) are **NOT** implemented.
- Future enhancement: Wi-Fi and web UI can be added in a future release if needed.

## Firing Programs (implemented)
Programs are now user‑definable and persisted in JSON (`/programs.json`). The firmware supports up to `MAX_PROGRAMS` (default 8) and up to 9 segments per program.

Default behavior:
- On first boot (no `/programs.json`) the firmware initializes two example programs (a 9-step and a 4-step) and saves them to `/programs.json`.
- On subsequent boots the firmware loads `/programs.json` into a `programs[]` array. `currentProgram` is a pointer to one entry in that array.

Program segment fields:
- rate_c_per_hour (float) — ramp rate in °C per hour
- target_c (float) — segment target temperature
- hold_seconds (uint32) — hold duration in seconds

Setpoints are computed at runtime from the segment's start temperature and ramp rate (rate_per_s = rate_c_per_hour / 3600). When the computed setpoint reaches the segment target the firmware enters the hold state and counts down the configured hold_seconds.

## User Interface & Controls (implemented)
Implemented UI and controls (matches code):

- OLED content (as implemented):
  - Large temperature readout (top-left)
  - Small SSR/BTN indicators (top-right)
  - Setpoint (large) and program name
  - Bottom lines: status text showing either "Sensor Fault", "Idle", or program state (shows current ramp rate, segment index and whether in RAMP/HOLD/PAUSE). When in hold, a countdown is shown.

- Button behavior (A0 ADC):
  - Short press (released < 2s): toggle start/pause of the current program (startProgram / pauseProgram).
 - Long press (>= 2s): cycle program selection through the loaded programs in `/programs.json`. If a program was running, long-press stops it before switching.
  - Debounce is implemented in software (50 ms debounce window).

Notes: there is no on-device program editor implemented in this release. Programs must be uploaded as `/programs.json` into LittleFS (or added at build time). A web upload endpoint can be added later to enable on-device editing.

## Control & Timing Details (implemented)
- Temperature sampling and PID update interval: 1 Hz (TEMP_INTERVAL_MS = 1000 ms).
- PID: discrete PID computed each sample with Kp=40, Ki=10, Kd=4 and a simple integral anti-windup clamp. Output is clamped to 0–100%.
- SSR time-proportional control window: 2000 ms (2 s). Within each window the SSR is ON for onTime = pidOutput% * windowLength.
- Serial logging: the firmware prints a line each update with T, SP, Out, program, segment and fault flag.
- Program progression: setpoint is computed from the segment's start temperature and ramp rate; when target is reached the firmware sets inHold and begins hold countdown. After hold completes it advances to the next segment or stops when program ends.


## Web UI & Wi‑Fi Configuration (NOT IMPLEMENTED)

**Status: This feature is not currently implemented in the firmware.**

The following Wi-Fi and web UI capabilities were planned but are not yet coded:

Wi‑Fi connectivity and setup (planned):
- On boot, the firmware would attempt to connect to saved Wi‑Fi credentials (stored in `/wifi.cfg`).
- If STA (Station mode) connection succeeds, the device would join the local network and a web server would be accessible at the device IP address.
- If STA connection fails (or no credentials saved), the device would start a SoftAP (Access Point) named `KilnController-Setup` and serve a web interface on `192.168.4.1`.
- The device could be configured via the web UI form to save new SSID and PSK to `/wifi.cfg`.

Planned web endpoints:
- **GET /** — HTML status page with current temperature, setpoint, program status, PID output, SSR state, and sensor fault status. Would include a form to configure Wi‑Fi SSID and PSK.
- **GET /status** — JSON status endpoint returning current device state (temperature, setpoint, program name, segment info, SSR status) suitable for remote monitoring or integration.
- **POST /save** — would accept form data with `ssid` and `psk` fields, save credentials to `/wifi.cfg`, and attempt to reconnect as STA.

**Future enhancement:** Wi-Fi and web server functionality can be added by integrating `ESP8266WebServer` and Wi-Fi connection code. This would require implementing STA/SoftAP mode switching, the web server request handlers, and the `/wifi.cfg` persistence layer.

## Safety Requirements (implemented)
- Emergency absolute max: firmware enforces ABSOLUTE_MAX_TEMP (1200 °C) and will disable SSR if the measured temperature exceeds this value.
- Thermocouple fault detection: invalid readings set `sensorFault=true` and SSR is disabled while the fault persists; the OLED shows "Sensor Fault".
- SSR is explicitly turned off when no program is running.
- Note: there is no additional hardware watchdog in firmware. Use hardware watchdog or external failsafe if required for your installation.

## Persistence & Configuration (implemented)
Persistence implemented using LittleFS:

- `/kiln_state.txt` — lightweight key=value runtime state file containing: pidSetpoint, programName, currentSegmentIndex, programRunning (0/1), segmentElapsed, inHold, holdElapsed. Written by `saveState()` in plain text format (one key=value per line).
  - State is throttled to be written no more frequently than STATE_SAVE_INTERVAL_MS (10 s) and only when stateDirty is set (e.g., when setpoint changed > 0.5 °C).
- `/programs.json` — JSON array of program objects (name, seqCount, segments[]). Programs are loaded at startup using `loadPrograms()` and can be saved back using `savePrograms()`. If missing on first boot, default example programs are created and written to the file.
  - Supports up to MAX_PROGRAMS (default 8) with up to 9 segments per program.
  - Each segment stores: rate_c_per_hour (ramp rate), target_c (target temperature), hold_seconds (hold duration).

**Not implemented:**
- `/wifi.cfg` — Wi-Fi credential persistence is not currently implemented (Wi-Fi functionality is not in the firmware).

Notes and limitations:
- Programs must be valid JSON; `loadPrograms()` uses `ArduinoJson` to parse and load up to `MAX_PROGRAMS` entries.
- There is no on-device editor in this release. To update programs you can upload a new `programs.json` into LittleFS (PlatformIO data upload or a future web upload endpoint).
- Wi-Fi functionality is not implemented in this version.

## Acceptance Criteria / Tests (updated)
Acceptance criteria (based on implementation):

- MAX6675 temperature reads and updates OLED at 1s intervals: PASS (implemented via `readTemperatureC()` and TEMP_INTERVAL_MS).
- PID loop and SSR time-proportional control operate at 1s sampling / 2s control window: PASS.
- Program runner advances segments and performs holds per segment definitions: PASS (with two built-in programs present).
- Persistence of runtime state to LittleFS: PASS (`/kiln_state.txt` written by `saveState()`).
- SSR disables on sensor fault or on exceeding ABSOLUTE_MAX_TEMP: PASS.
- Programs are loaded from `/programs.json` and the long-press cycles through all loaded programs: PASS.
- Not implemented / Deferred:
  - Wi-Fi configuration via SoftAP + web form: **NOT implemented**.
  - Web UI endpoints (GET /, GET /status, POST /save): **NOT implemented**.
  - `/wifi.cfg` persistence: **NOT implemented**.
  - On-device program editor: not implemented.
  - Explicit OLED message for "Temp < 100 °C": not implemented; the UI shows "Idle" or segment info instead.
  - Hardware watchdog/explicit MCU watchdog handling: not implemented in code (use platform watchdog if required).

## Implementation Notes & Recommendations
Implementation notes (matches `src/KilnController.cpp`):

- Uses LittleFS for state persistence.
- Uses Adafruit_SSD1306 and Adafruit_GFX for the OLED display.
- Uses a MAX6675 library for thermocouple reads.
- PID implemented in-source with simple anti-windup limiting.
- Control window default is 2 s; this can be increased in code to reduce SSR switching.
- Test extensively with isolated loads before connecting to kiln elements.

Additional implementation notes:
- Uses `ArduinoJson` (v6) for parsing and writing `/programs.json`. Make sure `platformio.ini` includes `bblanchon/ArduinoJson` in `lib_deps`.
- **Wi-Fi and web server are NOT currently implemented.** Future releases can add this by integrating `ESP8266WebServer` library and Wi-Fi connection logic.
- Programs[] is an array of in-memory Program instances; `currentProgram` is a pointer into that array. Persisted `programName` in `/kiln_state.txt` is resolved against the loaded programs on boot.

## Deliverables (current repo)
- Firmware (`src/KilnController.cpp`) implementing:
  - Pin mapping and drivers (SSD1306, MAX6675)
  - PID-based SSR time-proportional control
  - Program runner with JSON-backed programs (`/programs.json`) and start/pause/cycle via single button
  - Persistence of runtime state and setpoint to LittleFS (`/kiln_state.txt`)
  - Serial logging for monitoring
  - **NOT implemented:** Wi‑Fi setup, web UI, SoftAP mode, `/wifi.cfg` persistence
- Safety checklist and wiring notes remain the responsibility of the user and should be followed when wiring mains equipment.
- Document with the user manual that will show only the functionality of the product
- Document that will detail the implementation and technical details of the product.
