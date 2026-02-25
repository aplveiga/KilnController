// WebPortalFirmware.cpp
// Handles firmware upload/OTA endpoints for web portal
#include "WebPortalFirmware.h"
#include <Updater.h>
#include <ESP8266WebServer.h>

extern ESP8266WebServer server;

void handleFirmwareUpload() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("[OTA] Firmware upload start: %s (size: %u bytes)\n", upload.filename.c_str(), upload.totalSize);
        if (!Update.begin(0, U_FLASH)) {
            Serial.println("[OTA] Update.begin() failed!");
            Update.printError(Serial);
            return;
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        size_t written = Update.write(upload.buf, upload.currentSize);
        if (written != upload.currentSize) {
            Serial.printf("[OTA] Update.write() failed: wrote %u of %u bytes\n", written, upload.currentSize);
            Update.printError(Serial);
            return;
        }
        yield();
    } else if (upload.status == UPLOAD_FILE_END) {
        Serial.printf("[OTA] Upload complete, finalizing... (total: %u bytes)\n", upload.totalSize);
        if (Update.end(true)) {
            Serial.printf("[OTA] Firmware upload successful: %u bytes\n", upload.totalSize);
            server.send(200, "application/json", "{\"success\":true,\"message\":\"Firmware updated. Rebooting...\"}");
            delay(500);
            ESP.restart();
        } else {
            Serial.printf("[OTA] Update.end() failed!\n");
            Update.printError(Serial);
            server.send(400, "application/json", "{\"success\":false,\"error\":\"Firmware upload failed\"}");
        }
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        Serial.println("[OTA] Upload aborted!");
        Update.end(false);
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Upload aborted\"}");
    }
}
