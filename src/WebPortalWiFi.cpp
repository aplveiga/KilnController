// WebPortalWiFi.cpp
// [DEPRECATED] This file is superseded by WirelessManager class in webportal.cpp
// Handler implementations have been moved to the WirelessManager class:
// - handleWiFiScan() -> WirelessManager::handleScan()
// - handleWiFiConnect() -> WirelessManager::handleConnect()
// - handleWiFiReset() -> WirelessManager::handleReset()
//
// This file is kept for compatibility but contains no active code.

#if 0  // DEAD CODE - Disabled for cleanup

#include "WebPortalWiFi.h"
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

extern ESP8266WebServer server;

void handleWiFiScan() {
	int networkCount = WiFi.scanNetworks();
	// Reduced from 2048 to 512 bytes (saves ~1.5KB per scan)
	StaticJsonDocument<512> doc;
	JsonArray networks = doc.createNestedArray("networks");
	for (int i = 0; i < networkCount; i++) {
		JsonObject net = networks.createNestedObject();
		net["ssid"] = WiFi.SSID(i);
		net["rssi"] = WiFi.RSSI(i);
		net["channel"] = WiFi.channel(i);
		net["encryption"] = WiFi.encryptionType(i);
	}
	String response;
	serializeJson(doc, response);
	server.send(200, "application/json", response);
}

void handleWiFiConnect() {
	if (server.method() != HTTP_POST) {
		server.send(405, "text/plain", "Method Not Allowed");
		return;
	}
	String body = server.arg("plain");
	StaticJsonDocument<256> doc;
	DeserializationError error = deserializeJson(doc, body);
	if (error) {
		server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
		return;
	}
	String ssid = doc["ssid"].as<String>();
	String password = doc["password"].as<String>();
	WiFi.begin(ssid.c_str(), password.c_str());
	server.send(200, "application/json", "{\"success\":true}");
}

void handleWiFiReset() {
	if (server.method() != HTTP_POST) {
		server.send(405, "text/plain", "Method Not Allowed");
		return;
	}
	// Remove config file
	LittleFS.remove("/wifi_config.json");
	server.send(200, "application/json", "{\"success\":true}");
	delay(500);
	ESP.restart();
}
#endif  // End DEAD CODE