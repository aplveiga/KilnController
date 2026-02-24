# Kiln Controller — User Manual (Operation & basic network configuration)

This short manual explains how to operate the kiln controller from the device (OLED + single button) and how to configure Wi‑Fi using the built-in web UI.

WARNING: This controller controls heating equipment. Ensure hardware safety measures (rated SSR, fuses, hardware interlocks) are in place before operating.

Basic UI
- The OLED shows the measured temperature prominently, the current setpoint (SP), the active program name, segment number and mode (RAMP/HOLD/PAUSE). It also displays simple messages (e.g., "Sensor Fault").

Button actions (single button on A0)
- Short press: Start/resume or pause the current program (toggle run/pause).
- Long press (hold >= 2s): Cycle through available programs (the selected program is shown briefly on-screen). The device supports multiple programs stored in `/programs.json`.

How to run a program
1. Power the controller and confirm the OLED shows the current temperature.
2. Short-press the button to start the currently selected program. The display will show segment info and mode.
3. While running:
   - The controller will automatically compute setpoints and control the SSR to follow the program.
   - Short-press will pause/resume the program. Long-press will switch programs (stops current run and selects the next program).
4. To stop completely: pause the program (short press). You may then long-press to select a different program if desired.

Alarms and messages
- "Sensor Fault" — probe or MAX6675 fault; SSR disabled. Check wiring.
- Over-temperature — if measured temperature exceeds the absolute maximum, heaters are disabled.

Wi‑Fi and web UI (configuration)
- The device can connect to a local Wi‑Fi network to serve a small web UI exposing status and a Wi‑Fi setup form.
- On first boot (or if no valid Wi‑Fi is saved) the device starts a SoftAP named `KilnController-Setup`. Connect a phone/laptop to that AP and open `http://192.168.4.1` in a browser to access the configuration page.
- If a saved Wi‑Fi network exists the device will attempt to connect as a station (STA) for ~8 seconds. If it connects, the web UI will be reachable at the assigned station IP (printed to Serial).

Web UI endpoints
- GET / — status page showing temperature, setpoint, program, segment, PID output, SSR state and a Wi‑Fi SSID/password form.
- GET /status — JSON status useful for monitoring or scripts.
- POST /save — saves SSID and PSK to `/wifi.cfg` and attempts to connect to the network. If connection succeeds the device will remain as STA; if it fails the device returns to AP mode.

Persistence and files
- `/programs.json` — program definitions (JSON array). The firmware loads programs from this file on boot. If missing, default example programs are created and saved here.
- `/kiln_state.txt` — lightweight runtime save used to resume a run (pidSetpoint, programName, segment index, elapsed times, flags).
- `/wifi.cfg` — saved SSID and PSK (plain text) when configured via the web UI.

Reset / clearing saved data
- To clear saved Wi‑Fi credentials: delete `/wifi.cfg` from LittleFS (reflash, or provide a small utility to remove the file). After clearing the file the device will boot into AP mode for reconfiguration.
- To restore example programs: remove `/programs.json` from LittleFS and reboot — firmware will recreate default examples.

Notes
- The web UI stores the Wi‑Fi password in plain text (`/wifi.cfg`). For production or shared environments consider improving security (e.g., do not echo PSK, encrypt the file, or rely on short-lived AP mode only).
- There is no on-device program editor yet — to change programs either edit `/programs.json` and upload it (via LittleFS upload tooling or a future web upload endpoint) or re-flash with updated defaults.

That's it — use the single button to start, pause, and switch programs. Use the web UI to configure Wi‑Fi and to view status remotely. Keep a log of test runs and tune PID values as needed for your kiln.
