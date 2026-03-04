// ============================================================================
// Service Layer - Wireless Manager Interface (Refactored)
// ============================================================================
// Manages WiFi connectivity and OTA updates
// Provides: WiFi connection management, web server hosting, OTA handling
// ============================================================================

#ifndef WIRELESS_MANAGER_H
#define WIRELESS_MANAGER_H

#include <Arduino.h>
#include "../config/config.h"
#include "../config/types.h"

class WirelessManager {
public:
    WirelessManager();
    
    // Lifecycle
    void init();                    // Initialize WiFi and web server
    void shutdown();
    
    // Connection Management
    void connectToNetwork(const String& ssid, const String& password);
    void startAPMode();             // Start access point
    void stopAPMode();
    void disconnect();
    
    // Status Queries
    bool isConnected();             // Connected in station mode
    bool isAPActive();              // AP mode active
    String getConnectionStatus();   // "STA", "AP", "AP+STA", "DISCONNECTED"
    
    // Network Information
    String getLocalIP();            // Station mode IP
    String getAPIP();               // AP mode IP
    String getAPSSID();
    String getMACAddress();
    int getSignalStrength();        // WiFi RSSI (dBm)
    
    // Configuration
    void saveNetworkConfig(const String& ssid, const String& password);
    bool loadNetworkConfig(String& ssid, String& password);
    void resetNetworkConfig();
    
    // Web Server
    void handleLoop();              // Call regularly from main loop
    int getWebServerPort() const;
    
    // OTA Update Handling
    void setupOTA();                // Configure OTA
    void handleOTA();               // Process OTA requests
    bool isOTAInProgress();
    int getOTAProgress() const;     // 0-100%
    
    // Time Sync
    void syncTimeWithNTP();         // Sync with NTP server
    bool isTimeSynced();
    uint32_t getUnixTime();
    
    // Network Scanning
    void scanNetworks();
    struct NetworkInfo {
        char ssid[64];
        int signal_strength;
        bool is_secure;
    };
    
    int getScannedNetworkCount() const;
    String getScannedNetworksJSON();  // Returns JSON array of networks
    
    // Status
    bool isInitialized() const;
    String getStatusJSON() const;

private:
    bool initialized_;
    bool ap_active_;
    unsigned long last_connection_attempt_ms_;
    unsigned long last_ntp_sync_ms_;
    unsigned long last_scan_time_ms_;
    
    WiFiConfig saved_config_;
    
    // Web server setup
    void setupWebServer();
    
    // Helpers
    bool isValidSSID(const String& ssid);
    void logConnectionStatus();
};

#endif // WIRELESS_MANAGER_H
