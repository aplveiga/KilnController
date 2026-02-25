// WebPortalButtons.cpp
// Handles button action endpoints for web portal
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
