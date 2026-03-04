// WebPortalButtons.cpp
// [DEPRECATED] This file is superseded by WirelessManager class in webportal.cpp
// Handler implementations have been moved to the WirelessManager class:
// - handleButtonStartStop() -> WirelessManager::handleButtonStartStop()
// - handleButtonCycleProgram() -> WirelessManager::handleButtonCycleProgram()
//
// This file is kept for compatibility but contains no active code.

#if 0  // DEAD CODE - Disabled for cleanup

#include "WebPortalButtons.h"
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>

extern ESP8266WebServer server;

void handleButtonStartStop() {
    // Placeholder: implement start/stop logic
    server.send(200, "application/json", "{\"success\":true}");
}

void handleButtonCycleProgram() {
    // Placeholder: implement cycle program logic
    server.send(200, "application/json", "{\"success\":true}");
}

#endif  // End DEAD CODE
