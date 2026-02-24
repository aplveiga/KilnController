// WebPortalPrograms.cpp
// Handles kiln program management endpoints for web portal
#include "WebPortalPrograms.h"
#include <ESP8266WebServer.h>
#include <program_manager.h>
#include <ArduinoJson.h>

extern ESP8266WebServer server;
extern ProgramManager programManager;

void handleProgramList() {
    String jsonList;
    if (programManager.listPrograms(jsonList)) {
        server.send(200, "application/json", jsonList);
    } else {
        server.send(500, "application/json", "{\"success\":false,\"error\":\"Failed to list programs\"}");
    }
}

void handleProgramLoad() {
    if (server.arg("name").length() == 0) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"No program name provided\"}");
        return;
    }
    String name = server.arg("name");
    Program program;
    if (programManager.loadProgram(name.c_str(), program)) {
        DynamicJsonDocument doc(1024);
        doc["success"] = true;
        JsonObject prog = doc.createNestedObject("program");
        prog["name"] = program.name;
        prog["seqCount"] = program.seqCount;
        JsonArray segments = prog.createNestedArray("segments");
        for (int i = 0; i < program.seqCount; i++) {
            JsonObject seg = segments.createNestedObject();
            seg["rate_c_per_hour"] = program.segments[i].rate_c_per_hour;
            seg["target_c"] = program.segments[i].target_c;
            seg["hold_seconds"] = program.segments[i].hold_seconds;
        }
        String response;
        serializeJson(doc, response);
        server.send(200, "application/json", response);
    } else {
        server.send(404, "application/json", "{\"success\":false,\"error\":\"Program not found\"}");
    }
}
