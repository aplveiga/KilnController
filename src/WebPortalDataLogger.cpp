// WebPortalDataLogger.cpp
// Handles data logger endpoints for web portal
#include "WebPortalDataLogger.h"
#include <ESP8266WebServer.h>
#include <kiln_data_logger.h>
#include <ArduinoJson.h>

extern ESP8266WebServer server;
extern KilnDataLogger& logger;

void handleDataLog() {
    String data;
    logger.exportData(data);
    server.send(200, "application/json", data);
}

void handleClearData() {
    logger.clearAll();
    StaticJsonDocument<64> doc;
    doc["success"] = true;
    doc["message"] = "All data cleared";
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}
