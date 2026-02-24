#ifndef WIRELESS_H
#define WIRELESS_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <Updater.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// WiFi configuration structure
struct WiFiConfig {
    String ssid;
    String password;
    bool apMode;  // true = AP mode, false = Station mode
};

class WirelessManager {
public:
    WirelessManager();
    
    // Initialization
    void begin();
    
    // Connection management
    void connectToKnownNetworks();
    void startAPMode();
    void handleWiFi();
    
    // Configuration
    void loadConfig();
    void saveConfig();
    void resetConfig();
    
    // Status information
    String getStatusJSON();
    bool isConnected();
    String getAPSSID();
    
    // WiFi scanning
    void scanNetworks();
    
    // NTP Time Synchronization
    void syncTimeWithNTP();
    bool isTimeValid();
    
private:
    WiFiConfig config;
    ESP8266WebServer server;
    unsigned long lastConnectionAttempt;
    unsigned long lastScanTime;
    unsigned long lastNTPSync;
    int cachedNetworkCount;
    bool ntpSynced;
    
    // Web server handlers
    void setupWebServer();
    void handleRoot();
    void handleScan();
    void handleConnect();
    void handleReset();
    void handleKilnStatus();
    void handleDataLog();
    void handleClearData();
    void handleFirmwareUpload();
    void handleNotFound();
    
    // Configuration file operations
    const char* CONFIG_FILE = "/wifi_config.json";
    const char* AP_SSID = "KilnController";
    const char* AP_PASSWORD = "12345678";
    const unsigned long CONNECTION_TIMEOUT = 15000;  // 15 seconds
    const unsigned long SCAN_INTERVAL = 30000;       // 30 seconds
    
    // DHCP Server Configuration (AP mode)
    // The ESP8266 automatically enables DHCP server when in AP mode
    // Range: 192.168.4.1 to 192.168.4.254 (configurable in softAPConfig)
    // Devices connecting to KilnController AP will receive IPs via DHCP
};

extern WirelessManager wirelessManager;

#endif // WIRELESS_H
