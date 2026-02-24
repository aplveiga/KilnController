// ESP8266 WiFi Manager with Web Configuration Portal
// Features: WiFi connection, AP mode fallback, web configuration, OTA updates, flash storage

#include <wireless.h>
#include <kiln_data_logger.h>
#include <program_manager.h>
#include <time.h>

// Forward declaration
void setupOTA();

// Global instance
WirelessManager wirelessManager;

// Constructor
WirelessManager::WirelessManager() 
    : server(80), lastConnectionAttempt(0), lastScanTime(0), lastNTPSync(0), cachedNetworkCount(0), ntpSynced(false), firmwareUploadSuccess(false) {
    config.apMode = true;  // Start in AP mode by default
}

// ============================================================================
// Initialization
// ============================================================================

void WirelessManager::begin() {
    // Initialize LittleFS
    if (!LittleFS.begin()) {
        Serial.println("[WiFi] LittleFS initialization failed");
    } else {
        Serial.println("[WiFi] LittleFS initialized");
    }
    
    // Load saved configuration
    loadConfig();
    
    // Set WiFi mode - start with STA only to avoid interference
    WiFi.mode(WIFI_STA);
    WiFi.setAutoConnect(false);
    WiFi.setAutoReconnect(true);
    
    // Try to connect to known network first
    connectToKnownNetworks();
    
    // Only start AP mode if connection failed
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.mode(WIFI_AP_STA);  // Enable AP mode for fallback
        startAPMode();
    } else {
        // Keep only STA mode when connected to stay stable
        WiFi.mode(WIFI_STA);
        Serial.println("[WiFi] Operating in STA-only mode for stability");
    }
    
    // Setup web server
    setupWebServer();
    server.begin();
    
    Serial.println("[WiFi] Initialization complete");
    Serial.printf("[WiFi] AP SSID: %s\n", AP_SSID);
    Serial.println("[WiFi] Setup web server on http://192.168.4.1");
    
    // Setup OTA updates
    setupOTA();
}

// ============================================================================
// WiFi Connection Management
// ============================================================================

void WirelessManager::connectToKnownNetworks() {
    if (config.ssid.length() == 0) {
        Serial.println("[WiFi] No saved network configuration found");
        return;
    }
    
    Serial.printf("[WiFi] Attempting to connect to: %s\n", config.ssid.c_str());
    
    WiFi.begin(config.ssid.c_str(), config.password.c_str());
    
    lastConnectionAttempt = millis();
    unsigned long connectionStart = millis();
    
    // Wait for connection with timeout
    while (WiFi.status() != WL_CONNECTED && millis() - connectionStart < CONNECTION_TIMEOUT) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[WiFi] Connected successfully!");
        Serial.printf("[WiFi] IP address: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("[WiFi] Signal strength: %d dBm\n", WiFi.RSSI());
        Serial.println("[WiFi] DHCP server active on AP for configuration access");
        config.apMode = false;
    } else {
        Serial.println("[WiFi] Connection failed, staying in AP mode");
        config.apMode = true;
    }
}

void WirelessManager::startAPMode() {
    // Configure and start AP mode
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    
    IPAddress apIP(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(apIP, apIP, subnet);
    
    // Enable DHCP server for AP mode (allows IP assignment to connecting clients)
    WiFi.softAPIP();  // Ensure AP is ready
    
    Serial.printf("[WiFi] AP Mode started\n");
    Serial.printf("[WiFi] AP IP: %s\n", WiFi.softAPIP().toString().c_str());
    Serial.println("[WiFi] DHCP server enabled for AP mode");
}

void WirelessManager::handleWiFi() {
    // Handle web server requests
    server.handleClient();
    
    // Handle OTA updates
    ArduinoOTA.handle();
    
    // Synchronize time with NTP when WiFi connected
    if (WiFi.status() == WL_CONNECTED) {
        syncTimeWithNTP();
    }
    
    // Check if connection is lost and try to recover
    if (WiFi.status() != WL_CONNECTED && !config.apMode) {
        if (millis() - lastConnectionAttempt > CONNECTION_TIMEOUT) {
            Serial.println("[WiFi] Connection lost, enabling AP mode for fallback...");
            config.apMode = true;
            WiFi.mode(WIFI_AP_STA);  // Enable AP mode when connection lost
            startAPMode();
            lastConnectionAttempt = millis();  // Reset timer to try reconnection later
        }
    }
    
    // If AP mode is enabled and we have a connection, keep STA-only for stability
    if (WiFi.status() == WL_CONNECTED && config.apMode) {
        Serial.println("[WiFi] Connection restored, switching to STA-only mode");
        config.apMode = false;
        WiFi.mode(WIFI_STA);  // Disable AP mode to avoid interference
    }
    
    // Periodically scan for networks and auto-connect to known network
    if (millis() - lastScanTime > SCAN_INTERVAL) {
        scanNetworks();
        
        // If we're in AP mode but have a saved network, check if it's available
        if (config.apMode && config.ssid.length() > 0 && WiFi.status() != WL_CONNECTED) {
            bool foundKnownNetwork = false;
            for (int i = 0; i < cachedNetworkCount; i++) {
                if (WiFi.SSID(i) == config.ssid) {
                    foundKnownNetwork = true;
                    Serial.printf("[WiFi] Known network '%s' detected during periodic scan, attempting connection...\n", config.ssid.c_str());
                    connectToKnownNetworks();
                    break;
                }
            }
        }
    }
}

// ============================================================================
// Configuration Management
// ============================================================================

void WirelessManager::loadConfig() {
    if (!LittleFS.exists(CONFIG_FILE)) {
        Serial.println("[WiFi] Config file not found, using defaults");
        config.ssid = "";
        config.password = "";
        config.apMode = true;
        return;
    }
    
    File file = LittleFS.open(CONFIG_FILE, "r");
    if (!file) {
        Serial.println("[WiFi] Failed to open config file");
        return;
    }
    
    // Read and parse JSON
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        Serial.printf("[WiFi] JSON parse error: %s\n", error.c_str());
        return;
    }
    
    config.ssid = doc["ssid"].as<String>();
    config.password = doc["password"].as<String>();
    config.apMode = doc["apMode"] | true;
    
    Serial.printf("[WiFi] Configuration loaded: SSID=%s\n", config.ssid.c_str());
}

void WirelessManager::saveConfig() {
    StaticJsonDocument<256> doc;
    doc["ssid"] = config.ssid;
    doc["password"] = config.password;
    doc["apMode"] = config.apMode;
    
    File file = LittleFS.open(CONFIG_FILE, "w");
    if (!file) {
        Serial.println("[WiFi] Failed to open config file for writing");
        return;
    }
    
    if (serializeJson(doc, file) == 0) {
        Serial.println("[WiFi] Failed to write config file");
    } else {
        Serial.println("[WiFi] Configuration saved successfully");
    }
    file.close();
}

void WirelessManager::resetConfig() {
    if (LittleFS.remove(CONFIG_FILE)) {
        config.ssid = "";
        config.password = "";
        config.apMode = true;
        Serial.println("[WiFi] Configuration reset");
    } else {
        Serial.println("[WiFi] Failed to reset configuration");
    }
}

// ============================================================================
// Network Scanning
// ============================================================================

void WirelessManager::scanNetworks() {
    Serial.println("[WiFi] Scanning for available networks...");
    cachedNetworkCount = WiFi.scanNetworks();
    lastScanTime = millis();
    Serial.printf("[WiFi] Found %d networks\n", cachedNetworkCount);
}

// ============================================================================
// NTP Time Synchronization
// ============================================================================

void WirelessManager::syncTimeWithNTP() {
    // Only sync if connected to WiFi and not recently synced (once per hour)
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }
    
    unsigned long now = millis();
    if (ntpSynced && (now - lastNTPSync) < 3600000UL) {  // 1 hour cooldown
        return;
    }
    
    Serial.println("[NTP] Synchronizing time with NTP server...");
    
    // Configure time with NTP server
    // timezone offset in seconds (0 = UTC), daylight savings offset in seconds
    time_t now_sec = time(nullptr);
    
    // Use pool.ntp.org - multiple NTP servers worldwide
    // Format: configTime(timezone_offset, dst_offset, ntp_server1, ntp_server2, ntp_server3)
    configTime(0, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
    
    // Wait for time to be set (max 3 seconds)
    Serial.println("[NTP] Waiting for NTP time to be set...");
    time_t timeout = time(nullptr) + 3;  // 3 second timeout
    while (time(nullptr) < 24 * 3600 && time(nullptr) < timeout) {
        delay(100);
        yield();  // Allow ESP8266 to handle other tasks
    }
    
    time_t updated_time = time(nullptr);
    
    // Check if time was actually set (should be after year 2000, i.e., > 946684800)
    const time_t MIN_VALID_TIME = 946684800UL;  // Jan 1, 2000
    
    if (updated_time > MIN_VALID_TIME) {
        ntpSynced = true;
        lastNTPSync = now;
        
        struct tm* timeinfo = localtime(&updated_time);
        Serial.printf("[NTP] Time synchronized: %s\n", asctime(timeinfo));
    } else {
        Serial.println("[NTP] Failed to synchronize time");
    }
}

bool WirelessManager::isTimeValid() {
    time_t now = time(nullptr);
    const time_t MIN_VALID_TIME = 946684800UL;  // Jan 1, 2000
    return now > MIN_VALID_TIME;
}

// ============================================================================
// Web Server Handlers
// ============================================================================

void WirelessManager::setupWebServer() {
    server.on("/", [this]() { handleRoot(); });
    server.on("/api/scan", [this]() { handleScan(); });
    server.on("/api/connect", [this]() { handleConnect(); });
    server.on("/api/reset", [this]() { handleReset(); });
    server.on("/api/status", [this]() { 
        server.send(200, "application/json", getStatusJSON());
    });
    server.on("/api/kiln", [this]() { handleKilnStatus(); });
    
    // Firmware upload with proper dual-lambda handler
    // First lambda: Upload handler (processes file chunks)
    // Second lambda: Response handler (sends response after upload completes)
    server.on("/api/firmware", HTTP_POST, 
        [this]() {
            handleFirmwareUpload();
        },
        [this]() {
            server.sendHeader("Connection", "close");
            if (firmwareUploadSuccess && !Update.hasError()) {
                server.send(200, "application/json", "{\"success\":true,\"message\":\"Firmware updated. Rebooting...\"}");
                delay(500);
                ESP.restart();
            } else {
                server.send(400, "application/json", "{\"success\":false,\"error\":\"Firmware upload failed\"}");
            }
        }
    );
    
    server.on("/api/data", [this]() { handleDataLog(); });
    server.on("/api/cleardata", [this]() { handleClearData(); });
    
    // Program handlers
    server.on("/api/programs/list", [this]() { handleProgramList(); });
    server.on("/api/programs/load", [this]() { handleProgramLoad(); });
    server.on("/api/programs/save", [this]() { handleProgramSave(); });
    server.on("/api/programs/delete", [this]() { handleProgramDelete(); });
    
    // PID tuning handlers
    server.on("/api/pid/get", [this]() { handlePIDGet(); });
    server.on("/api/pid/set", [this]() { handlePIDSet(); });
    server.on("/api/ssr/rate/get", [this]() { handleSSRRateGet(); });
    server.on("/api/ssr/rate/set", [this]() { handleSSRRateSet(); });
    
    // Button action handlers
    server.on("/api/button/startstop", [this]() { handleButtonStartStop(); });
    server.on("/api/button/cycleprogram", [this]() { handleButtonCycleProgram(); });
    
    server.onNotFound([this]() { handleNotFound(); });
}

void WirelessManager::handleRoot() {
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");
    
    server.sendContent(
        "<!DOCTYPE html><html><head>"
        "<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1.0'>"
        "<title>Kiln Controller</title>"
        "<style>"
        "body{margin:0;padding:0;font-family:Arial,sans-serif;background:#f0f0f0}"
        ".header{background:#1976d2;color:white;padding:20px;text-align:center}"
        ".header h1{margin:0;font-size:28px}"
        ".wrapper{display:flex;height:calc(100vh-70px)}"
        ".menu{background:#333;color:white;width:200px;padding:0;list-style:none;margin:0;overflow-y:auto}"
        ".menu li{border-bottom:1px solid #555;margin:0}"
        ".menu button{width:100%;border:none;background:none;color:white;padding:15px;text-align:left;cursor:pointer;font-size:14px}"
        ".menu button:hover{background:#555}"
        ".menu button.active{background:#1976d2;font-weight:bold}"
        ".content{flex:1;overflow-y:auto;padding:20px}"
        ".tab{display:none}"
        ".tab.active{display:block}"
        ".tab-content{background:white;border-radius:8px;padding:20px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}"
        ".status-grid{display:grid;grid-template-columns:1fr 1fr;gap:15px;margin-bottom:20px}"
        ".status-box{background:#e3f2fd;padding:15px;border-radius:5px}"
        ".status-label{font-weight:bold;color:#1976d2;font-size:12px}"
        ".status-value{font-size:18px;color:#333;margin-top:5px;word-break:break-all}"
        ".network-list{max-height:300px;overflow-y:auto;border:1px solid #ddd;border-radius:5px;margin:15px 0}"
        ".network-item{padding:10px;border-bottom:1px solid #eee;cursor:pointer;background:#fafafa}"
        ".network-item:hover{background:#f0f0f0}"
        ".ssid-name{font-weight:bold;color:#333}"
        ".signal-strength{font-size:12px;color:#666;margin-top:5px}"
        ".form-group{margin-bottom:15px}"
        "label{display:block;margin-bottom:5px;font-weight:bold;color:#333;font-size:13px}"
        "input[type='text'],input[type='password'],input[type='file']{width:100%;padding:10px;border:1px solid #ddd;border-radius:5px;box-sizing:border-box;font-size:14px}"
        "input:focus{outline:none;border-color:#1976d2;box-shadow:0 0 5px rgba(25,118,210,0.3)}"
        ".button-group{display:flex;gap:10px;margin-top:15px}"
        "button{padding:10px 15px;border:none;border-radius:5px;cursor:pointer;font-size:13px;font-weight:bold}"
        ".btn-primary{background:#4caf50;color:white}.btn-primary:hover{background:#45a049}"
        ".btn-danger{background:#f44336;color:white}.btn-danger:hover{background:#da190b}"
        ".btn-info{background:#2196f3;color:white}.btn-info:hover{background:#0b7dda}"
        ".message{margin-top:15px;padding:10px;border-radius:5px;display:none}"
        ".message.success{background:#d4edda;color:#155724;border:1px solid #c3e6cb}"
        ".message.error{background:#f8d7da;color:#721c24;border:1px solid #f5c6cb}"
        ".spinner{border:3px solid #f3f3f3;border-top:3px solid #1976d2;border-radius:50%;width:20px;height:20px;animation:spin 1s linear infinite;display:none;margin:10px 0}"
        "@keyframes spin{0%{transform:rotate(0deg)}100%{transform:rotate(360deg)}}"
        ".dashboard-grid{display:grid;grid-template-columns:1fr 1fr;gap:15px;margin-bottom:20px}"
        ".dashboard-item{background:#f9f9f9;padding:15px;border-radius:5px;border-left:4px solid #1976d2}"
        ".dashboard-item-label{font-size:12px;color:#666;font-weight:bold}"
        ".dashboard-item-value{font-size:24px;color:#1976d2;margin-top:5px;font-weight:bold}"
        ".progress-bar{width:100%;height:8px;background:#ddd;border-radius:5px;margin-top:5px;overflow:hidden}"
        ".progress-fill{height:100%;background:#4caf50;width:50%}"
        "h3{color:#333;margin-top:0}"
        "table{width:100%;border-collapse:collapse;margin-top:10px;font-size:13px}"
        "table th{background:#1976d2;color:white;padding:10px;text-align:left;font-weight:bold}"
        "table td{padding:10px;border-bottom:1px solid #ddd}"
        "table tr:hover{background:#f5f5f5}"
        ".filter-row{display:flex;gap:10px;margin-bottom:20px;flex-wrap:wrap;align-items:flex-end}"
        ".filter-group{display:flex;flex-direction:column;gap:5px}"
        ".filter-group label{font-size:12px;margin:0}"
        ".filter-group input{padding:8px;border:1px solid #ddd;border-radius:3px;font-size:12px;min-width:150px}"
        ".table-container{max-height:600px;overflow-y:auto;border:1px solid #ddd;border-radius:5px}"
        ".table-info{color:#666;font-size:12px;margin-bottom:10px}"
        "</style></head><body>"
        "<div class='header'><h1>Kiln Controller</h1></div>"
        "<div class='wrapper'>"
        "<ul class='menu'>"
        "<li><button class='menu-btn' onclick='switchTab(1)'>📊 Dashboard</button></li>"
        "<li><button class='menu-btn' onclick='switchTab(4)'>⚙️ Programs</button></li>"
        "<li><button class='menu-btn' onclick='switchTab(5)'>🎛️ PID Tuning</button></li>"
        "<li><button class='menu-btn' onclick='switchTab(3)'>📋 Data Logger</button></li>"
        "<li><button class='menu-btn' onclick='switchTab(0)'>📡 WiFi Setup</button></li>"
        "<li><button class='menu-btn' onclick='switchTab(2)'>⬆️ Firmware</button></li>"
        "</ul>"
        "<div class='content'>"
    );
    
    // Dashboard Tab (Now first/active)
    server.sendContent(
        "<div class='tab active' id='tab-1'><div class='tab-content'>"
        "<h3>Kiln Status</h3>"
        "<div class='dashboard-grid'>"
        "<div class='dashboard-item'><div class='dashboard-item-label'>Temperature</div><div class='dashboard-item-value' id='dash-temp'>-- °C</div></div>"
        "<div class='dashboard-item'><div class='dashboard-item-label'>Set Point</div><div class='dashboard-item-value' id='dash-sp'>-- °C</div></div>"
        "<div class='dashboard-item'><div class='dashboard-item-label'>Program</div><div class='dashboard-item-value' id='dash-prog'>--</div></div>"
        "<div class='dashboard-item'><div class='dashboard-item-label'>Status</div><div class='dashboard-item-value' id='dash-status'>IDLE</div></div>"
        "<div class='dashboard-item'><div class='dashboard-item-label'>Rate</div><div class='dashboard-item-value' id='dash-rate'>-- °C/h</div></div>"
        "<div class='dashboard-item'><div class='dashboard-item-label'>Target</div><div class='dashboard-item-value' id='dash-target'>-- °C</div></div>"
        "</div>"
        "<h3>Controls</h3>"
        "<div class='button-group'>"
        "<button class='btn-primary' onclick='buttonStartStop()' style='flex:1'>▶️ Start/Stop</button>"
        "<button class='btn-info' onclick='buttonCycleProgram()' style='flex:1'>🔄 Cycle</button>"
        "</div>"
        "</div></div>"
    );
    
    // WiFi Setup Tab
    server.sendContent(
        "<div class='tab' id='tab-0'><div class='tab-content'>"
        "<h3>WiFi Configuration</h3>"
        "<div class='status-grid'>"
        "<div class='status-box'><div class='status-label'>Connection Status</div><div class='status-value' id='connStatus'>Loading...</div></div>"
        "<div class='status-box'><div class='status-label'>Current Network</div><div class='status-value' id='currentSSID'>N/A</div></div>"
        "<div class='status-box'><div class='status-label'>Signal Strength</div><div class='status-value' id='signal'>N/A</div></div>"
        "<div class='status-box'><div class='status-label'>IP Address</div><div class='status-value' id='ipAddr'>N/A</div></div>"
        "</div>"
        "<h3>Available Networks</h3>"
        "<div class='network-list' id='networkList'><div style='padding:20px;text-align:center;color:#999'>Click Scan to find networks</div></div>"
        "<div class='spinner' id='spinner-wifi'></div>"
        "<h3>Connect to Network</h3>"
        "<div class='form-group'><label>Network Name:</label><input type='text' id='ssid' placeholder='SSID'></div>"
        "<div class='form-group'><label>Password:</label><input type='password' id='password' placeholder='Password'></div>"
        "<div class='button-group'>"
        "<button class='btn-info' onclick='scanNetworks()'>Scan Networks</button>"
        "<button class='btn-primary' onclick='connectToNetwork()'>Connect</button>"
        "<button class='btn-danger' onclick='resetConfiguration()'>Reset WiFi</button>"
        "</div>"
        "<div class='message' id='msg-wifi'></div>"
        "</div></div>"
    );
    
    // Firmware Update Tab
    server.sendContent(
        "<div class='tab' id='tab-2'><div class='tab-content'>"
        "<h3>Firmware Update</h3>"
        "<p style='color:#666;font-size:13px'>Select a firmware file (.bin) to upload and update the device.</p>"
        "<div class='form-group'><label>Select Firmware File:</label><input type='file' id='firmwareFile' accept='.bin' onchange='updateFilename()'></div>"
        "<div id='fileInfo' style='color:#666;font-size:12px;margin:10px 0'></div>"
        "<div class='spinner' id='spinner-fw'></div>"
        "<div class='progress-bar' id='progressBar' style='display:none'><div class='progress-fill' id='progressFill' style='width:0%'></div></div>"
        "<div style='margin-top:10px' id='progressText' style='display:none'></div>"
        "<div class='button-group'>"
        "<button class='btn-info' onclick='browseFile()'>Browse</button>"
        "<button class='btn-primary' onclick='uploadFirmware()'>Upload & Update</button>"
        "</div>"
        "<div class='message' id='msg-fw'></div>"
        "</div></div>"
        
        // Data Logger Tab
        "<div class='tab' id='tab-3'><div class='tab-content'>"
        "<h3>Data Logger</h3>"
        "<p style='color:#666;font-size:13px'>View logged kiln operation data with filtering options.</p>"
        "<div class='filter-row'>"
        "<div class='filter-group'>"
        "<label>Sequence (Run):</label>"
        "<input type='number' id='filterSeq' placeholder='All' min='1'>"
        "</div>"
        "<div class='filter-group'>"
        "<label>From Time:</label>"
        "<input type='datetime-local' id='filterTimeFrom'>"
        "</div>"
        "<div class='filter-group'>"
        "<label>To Time:</label>"
        "<input type='datetime-local' id='filterTimeTo'>"
        "</div>"
        "<div class='button-group'>"
        "<button class='btn-info' onclick='loadDataLog()'>Load Data</button>"
        "<button class='btn-primary' onclick='refreshDataLog()'>Refresh</button>"
        "<button class='btn-danger' onclick='clearDataLog()'>Clear All</button>"
        "</div>"
        "</div>"
        "<div class='table-info' id='tableInfo'></div>"
        "<div class='table-container'>"
        "<table id='dataTable'>"
        "<thead><tr>"
        "<th>Seq</th><th>Time</th><th>Temp (°C)</th><th>SP (°C)</th><th>Rate</th><th>Target</th><th>Duration</th><th>Program</th><th>Status</th>"
        "</tr></thead>"
        "<tbody id='tableBody'></tbody>"
        "</table>"
        "</div>"
        "<div class='spinner' id='spinner-log'></div>"
        "<div class='message' id='msg-log'></div>"
        "</div></div>"
        
        // Programs Manager Tab (tab-4)
        "<div class='tab' id='tab-4'><div class='tab-content'>"
        "<h3>Kiln Programs</h3>"
        "<div class='form-group'><label>Available Programs:</label>"
        "<div class='network-list' id='programList'><div style='padding:20px;text-align:center;color:#999'>Loading programs...</div></div></div>"
        "<div class='form-group'><label>Program Name (max 9 chars):</label>"
        "<input type='text' id='programName' placeholder='Name' maxlength='9'></div>"
        "<div class='form-group'><label>Number of Segments (1-9):</label>"
        "<input type='number' id='numSegments' min='1' max='9' value='1'></div>"
        "<div id='segmentsForm'></div>"
        "<div class='button-group'>"
        "<button class='btn-info' onclick='reloadPrograms()'>Reload</button>"
        "<button class='btn-primary' onclick='createSegmentFields()'>Create New</button>"
        "<button class='btn-primary' onclick='saveProgram()'>Save</button>"
        "</div><div class='message' id='msg-prog'></div></div></div>"
        
        // PID Tuning Tab (tab-5)
        "<div class='tab' id='tab-5'><div class='tab-content'>"
        "<h3>PID Tuning</h3>"
        "<p style='color:#666;font-size:13px'>Adjust PID parameters (values saved to flash)</p>"
        "<div class='form-group'>"
        "<label>Kp (Proportional): <span id='kpVal'>--</span></label>"
        "<input type='range' id='kpSlider' min='0' max='40' step='0.1' oninput='updateKp()'>"
        "<input type='number' id='kpInput' min='0' max='40' step='0.1' style='width:80px' oninput='syncKp()'>"
        "</div>"
        "<div class='form-group'>"
        "<label>Ki (Integral): <span id='kiVal'>--</span></label>"
        "<input type='range' id='kiSlider' min='0' max='40' step='0.1' oninput='updateKi()'>"
        "<input type='number' id='kiInput' min='0' max='40' step='0.1' style='width:80px' oninput='syncKi()'>"
        "</div>"
        "<div class='form-group'>"
        "<label>Kd (Derivative): <span id='kdVal'>--</span></label>"
        "<input type='range' id='kdSlider' min='0' max='40' step='0.1' oninput='updateKd()'>"
        "<input type='number' id='kdInput' min='0' max='40' step='0.1' style='width:80px' oninput='syncKd()'>"
        "</div>"
        "<hr style='border:1px solid #ccc;margin:20px 0'>"
        "<div class='form-group'>"
        "<label>SSR Update Rate: <span id='ssrRateVal'>--</span> ms</label>"
        "<input type='range' id='ssrRateSlider' min='100' max='10000' step='100' oninput='updateSSRRate()'>"
        "<small style='color:#666'>Minimum time between SSR state changes (protection for relay)</small>"
        "</div>"
        "<div class='button-group'>"
        "<button class='btn-info' onclick='loadDefaults()'>Load Defaults</button>"
        "<button class='btn-primary' onclick='savePID()'>Save PID to Flash</button>"
        "<button class='btn-primary' onclick='saveSSRRate()'>Save SSR Rate</button>"
        "</div><div class='message' id='msg-pid'></div></div></div>"
        "</div></div>"
    );
    
    // JavaScript
    server.sendContent(
        "<script>"
        "function switchTab(n){"
        "document.querySelectorAll('.tab').forEach(t=>t.classList.remove('active'));"
        "document.querySelectorAll('.menu-btn').forEach(b=>b.classList.remove('active'));"
        "document.getElementById('tab-'+n).classList.add('active');"
        "event.target.classList.add('active');"
        "}"
        "function showMessage(id,t,y){const m=document.getElementById(id);m.textContent=t;m.className='message '+y;m.style.display='block';if(y==='success')setTimeout(()=>{m.style.display='none'},5000);}"
        "function updateStatus(){"
        "fetch('/api/status').then(r=>r.json()).then(d=>{"
        "document.getElementById('connStatus').textContent=d.connected?'Connected':'Disconnected';"
        "document.getElementById('currentSSID').textContent=d.connected?d.ssid:'KilnController (AP)';"
        "document.getElementById('signal').textContent=d.connected?d.rssi+' dBm':'N/A';"
        "document.getElementById('ipAddr').textContent=d.ip||'N/A';"
        "});"
        "}"
        "function scanNetworks(){"
        "document.getElementById('spinner-wifi').style.display='block';"
        "fetch('/api/scan').then(r=>r.json()).then(d=>{"
        "const l=document.getElementById('networkList');l.innerHTML='';"
        "if(d.networks.length===0){l.innerHTML='<div style=\"padding:20px;text-align:center;color:#999\">No networks found</div>';}"
        "else{d.networks.forEach(n=>{"
        "const i=document.createElement('div');i.className='network-item';"
        "i.innerHTML='<div class=\"ssid-name\">'+n.ssid+'</div><div class=\"signal-strength\">Signal: '+n.rssi+' dBm | Ch: '+n.channel+'</div>';"
        "i.onclick=()=>{document.getElementById('ssid').value=n.ssid;document.getElementById('password').focus();};"
        "l.appendChild(i);"
        "});"
        "}"
        "document.getElementById('spinner-wifi').style.display='none';"
        "}).catch(e=>{showMessage('msg-wifi','Scan failed: '+e,'error');document.getElementById('spinner-wifi').style.display='none';});"
        "}"
        "function connectToNetwork(){"
        "const s=document.getElementById('ssid').value;const p=document.getElementById('password').value;"
        "if(!s){showMessage('msg-wifi','Enter a network name','error');return;}"
        "fetch('/api/connect',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:s,password:p})}).then(r=>r.json()).then(d=>{"
        "if(d.success){showMessage('msg-wifi','Connecting...','success');setTimeout(()=>updateStatus(),3000);}"
        "else{showMessage('msg-wifi','Failed: '+(d.error||'Unknown'),'error');}"
        "}).catch(e=>{showMessage('msg-wifi','Error: '+e,'error');});"
        "}"
        "function resetConfiguration(){"
        "if(confirm('Reset all WiFi settings?')){fetch('/api/reset',{method:'POST'}).then(r=>r.json()).then(d=>{showMessage('msg-wifi','Resetting...','success');setTimeout(()=>location.reload(),2000);});}"
        "}"
        "function updateFilename(){"
        "const f=document.getElementById('firmwareFile');const info=document.getElementById('fileInfo');"
        "if(f.files.length>0){const sz=f.files[0].size;const kb=Math.round(sz/1024);const mb=(sz/1048576).toFixed(2);"
        "if(sz>4194304){info.textContent='File: '+f.files[0].name+' - WARNING: File too large ('+mb+'MB, max 4MB)';info.style.color='#ff6b6b';}"
        "else{info.textContent='File: '+f.files[0].name+' ('+kb+'KB)';info.style.color='';}"
        "}else{info.textContent='';info.style.color='';}}"
        "function uploadFirmware(){"
        "const f=document.getElementById('firmwareFile');const sz=f.files[0]?.size;"
        "if(!f.files.length){showMessage('msg-fw','Select a firmware file first','error');return;}"
        "if(sz>4194304){showMessage('msg-fw','Firmware file too large (max 4MB)','error');return;}"
        "if(confirm('Upload firmware? Device will reboot after successful update.')){document.getElementById('spinner-fw').style.display='block';"
        "const fd=new FormData();fd.append('firmware',f.files[0]);"
        "fetch('/api/firmware',{method:'POST',body:fd,timeout:60000}).then(r=>r.json()).then(d=>{"
        "document.getElementById('spinner-fw').style.display='none';"
        "if(d.success){showMessage('msg-fw','✓ '+d.message,'success');setTimeout(()=>{location.reload();},3000);}"
        "else{showMessage('msg-fw','✗ Upload failed: '+(d.error||'Unknown error'),'error');}"
        "}).catch(e=>{document.getElementById('spinner-fw').style.display='none';"
        "showMessage('msg-fw','✗ Upload error: '+e,'error');});}}"
        "function browseFile(){document.getElementById('firmwareFile').click();}"
        "function reloadPrograms(){"
        "fetch('/api/programs/list').then(r=>r.json()).then(d=>{"
        "const pl=document.getElementById('programList');pl.innerHTML='';"
        "if(!d.programs||!d.programs.length){pl.innerHTML='<div style=\"padding:20px;text-align:center;color:#999\">No programs</div>';return;}"
        "d.programs.forEach(p=>{"
        "const div=document.createElement('div');div.className='network-item';div.style.display='flex';div.style.justifyContent='space-between';"
        "const info=document.createElement('div');info.style.flex='1';info.style.cursor='pointer';"
        "info.innerHTML='<strong>'+p.name+'</strong><br><small>'+p.seqCount+' segments</small>';"
        "info.onclick=()=>{document.getElementById('programName').value=p.name;loadProgram(p.name);};"
        "div.appendChild(info);"
        "if(p.name!=='4-step'&&p.name!=='9-step'){"
        "const btn=document.createElement('button');btn.innerHTML='🗑️';btn.style.cssText='background:#f44336;color:white;border:none;padding:5px 10px;margin-left:10px;cursor:pointer;';"
        "btn.onclick=e=>{e.stopPropagation();if(confirm('Delete '+p.name+'?')){deleteProgram(p.name);}};"
        "div.appendChild(btn);"
        "}"
        "pl.appendChild(div);"
        "});"
        "});}"
        "function deleteProgram(n){"
        "fetch('/api/programs/delete?name='+encodeURIComponent(n),{method:'POST'}).then(r=>r.json()).then(d=>{"
        "if(d.success){showMessage('msg-prog','Deleted: '+n,'success');reloadPrograms();document.getElementById('programName').value='';}});"
        "}"
        "function loadProgram(n){"
        "fetch('/api/programs/load?name='+encodeURIComponent(n)).then(r=>r.json()).then(d=>{"
        "if(d.success){document.getElementById('numSegments').value=d.program.seqCount;createSegmentFields();d.program.segments.forEach((s,i)=>{if(i<d.program.seqCount){document.getElementById('rate'+i).value=s.rate_c_per_hour;document.getElementById('target'+i).value=s.target_c;document.getElementById('hold'+i).value=Math.round(s.hold_seconds/60);}});}});"
        "}"
        "function createSegmentFields(){"
        "const n=parseInt(document.getElementById('numSegments').value)||1;const c=document.getElementById('segmentsForm');c.innerHTML='';"
        "for(let i=0;i<n;i++){const d=document.createElement('div');d.innerHTML='<fieldset style=\"border:1px solid #ddd;padding:10px;margin:10px 0\"><legend>Segment '+(i+1)+'</legend><div class=\"form-group\"><label>Rate:</label><input type=\"number\" id=\"rate'+i+'\" value=\"50\" step=\"0.1\"></div><div class=\"form-group\"><label>Target:</label><input type=\"number\" id=\"target'+i+'\" value=\"100\" step=\"0.1\"></div><div class=\"form-group\"><label>Hold (min):</label><input type=\"number\" id=\"hold'+i+'\" value=\"0\" step=\"1\"></div></fieldset>';c.appendChild(d);}"
        "}"
        "function saveProgram(){"
        "const name=document.getElementById('programName').value.trim();if(!name){showMessage('msg-prog','Enter name','error');return;}"
        "const cnt=parseInt(document.getElementById('numSegments').value)||0;if(cnt<1||cnt>9){showMessage('msg-prog','1-9 segments','error');return;}"
        "const seg=[];"
        "for(let i=0;i<cnt;i++){seg.push({rate_c_per_hour:parseFloat(document.getElementById('rate'+i).value)||0,target_c:parseFloat(document.getElementById('target'+i).value)||0,hold_seconds:Math.round((parseFloat(document.getElementById('hold'+i).value)||0)*60)});}"
        "const prog={name,seqCount:cnt,segments:seg};"
        "fetch('/api/programs/save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(prog)}).then(r=>r.json()).then(d=>{"
        "if(d.success){showMessage('msg-prog','Saved!','success');reloadPrograms();document.getElementById('programName').value='';}});"
        "}"
        "function updateKp(){document.getElementById('kpVal').textContent=parseFloat(document.getElementById('kpSlider').value).toFixed(2);document.getElementById('kpInput').value=parseFloat(document.getElementById('kpSlider').value).toFixed(2);}"
        "function syncKp(){document.getElementById('kpSlider').value=document.getElementById('kpInput').value;updateKp();}"
        "function updateKi(){document.getElementById('kiVal').textContent=parseFloat(document.getElementById('kiSlider').value).toFixed(2);document.getElementById('kiInput').value=parseFloat(document.getElementById('kiSlider').value).toFixed(2);}"
        "function syncKi(){document.getElementById('kiSlider').value=document.getElementById('kiInput').value;updateKi();}"
        "function updateKd(){document.getElementById('kdVal').textContent=parseFloat(document.getElementById('kdSlider').value).toFixed(2);document.getElementById('kdInput').value=parseFloat(document.getElementById('kdSlider').value).toFixed(2);}"
        "function syncKd(){document.getElementById('kdSlider').value=document.getElementById('kdInput').value;updateKd();}"
        "function updateSSRRate(){document.getElementById('ssrRateVal').textContent=document.getElementById('ssrRateSlider').value;}"
        "function loadCurrentPIDValues(){"
        "fetch('/api/pid/get').then(r=>r.json()).then(d=>{"
        "if(d.success){document.getElementById('kpSlider').value=d.kp;document.getElementById('kpInput').value=parseFloat(d.kp).toFixed(2);document.getElementById('kpVal').textContent=parseFloat(d.kp).toFixed(2);"
        "document.getElementById('kiSlider').value=d.ki;document.getElementById('kiInput').value=parseFloat(d.ki).toFixed(2);document.getElementById('kiVal').textContent=parseFloat(d.ki).toFixed(2);"
        "document.getElementById('kdSlider').value=d.kd;document.getElementById('kdInput').value=parseFloat(d.kd).toFixed(2);document.getElementById('kdVal').textContent=parseFloat(d.kd).toFixed(2);}"
        "});"
        "fetch('/api/ssr/rate/get').then(r=>r.json()).then(d=>{"
        "if(d.success){document.getElementById('ssrRateSlider').value=d.rateMs;document.getElementById('ssrRateVal').textContent=d.rateMs;}"
        "});"
        "}"
        "function loadDefaults(){"
        "document.getElementById('kpSlider').value='15.5';document.getElementById('kpInput').value='15.50';document.getElementById('kpVal').textContent='15.50';"
        "document.getElementById('kiSlider').value='13.1';document.getElementById('kiInput').value='13.10';document.getElementById('kiVal').textContent='13.10';"
        "document.getElementById('kdSlider').value='1.2';document.getElementById('kdInput').value='1.20';document.getElementById('kdVal').textContent='1.20';"
        "showMessage('msg-pid','Defaults loaded','success');"
        "}"
        "function savePID(){"
        "const kp=parseFloat(document.getElementById('kpInput').value);const ki=parseFloat(document.getElementById('kiInput').value);const kd=parseFloat(document.getElementById('kdInput').value);"
        "if(isNaN(kp)||isNaN(ki)||isNaN(kd)){showMessage('msg-pid','Invalid values','error');return;}"
        "fetch('/api/pid/set',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'kp='+kp+'&ki='+ki+'&kd='+kd}).then(r=>r.json()).then(d=>{"
        "if(d.success){showMessage('msg-pid','PID Saved!','success');}else{showMessage('msg-pid','Error','error');}});"
        "}"
        "function saveSSRRate(){"
        "const rate=parseInt(document.getElementById('ssrRateSlider').value);"
        "if(isNaN(rate)||rate<100||rate>10000){showMessage('msg-pid','Rate must be 100-10000 ms','error');return;}"
        "fetch('/api/ssr/rate/set',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'rateMs='+rate}).then(r=>r.json()).then(d=>{"
        "if(d.success){showMessage('msg-pid','SSR Rate Saved: '+rate+'ms','success');}else{showMessage('msg-pid','Error','error');}});"
        "}"
        "function buttonStartStop(){fetch('/api/button/startstop',{method:'POST'}).then(r=>r.json()).then(d=>{updateKilnStatus();});}"
        "function buttonCycleProgram(){fetch('/api/button/cycleprogram',{method:'POST'}).then(r=>r.json()).then(d=>{updateKilnStatus();reloadPrograms();});}"
        "function updateKilnStatus(){"
        "fetch('/api/kiln').then(r=>r.json()).then(d=>{"
        "document.getElementById('dash-temp').textContent=(d.temperature||0).toFixed(1)+' °C';"
        "document.getElementById('dash-sp').textContent=(d.setpoint||0).toFixed(1)+' °C';"
        "document.getElementById('dash-prog').textContent=d.program||'--';"
        "document.getElementById('dash-status').textContent=d.status||'IDLE';"
        "document.getElementById('dash-rate').textContent=(d.rate||0).toFixed(1)+' °C/h';"
        "document.getElementById('dash-target').textContent=(d.target||0).toFixed(1)+' °C';"
        "});"
        "}"
        "function loadDataLog(){"
        "const seq=document.getElementById('filterSeq').value;"
        "const tFrom=document.getElementById('filterTimeFrom').value;"
        "const tTo=document.getElementById('filterTimeTo').value;"
        "document.getElementById('spinner-log').style.display='block';"
        "document.getElementById('msg-log').textContent='';"
        "fetch('/api/data').then(r=>r.json()).then(d=>{"
        "let rows=d.data||[];"
        "if(seq&&seq.trim()!==''){rows=rows.filter(row=>row.seq==parseInt(seq));}"
        "if(tFrom){const tFromMs=new Date(tFrom).getTime();rows=rows.filter(row=>row.ts*1000>=tFromMs);}"
        "if(tTo){const tToMs=new Date(tTo).getTime()+86400000;rows=rows.filter(row=>row.ts*1000<=tToMs);}"
        "const tb=document.getElementById('tableBody');tb.innerHTML='';"
        "rows.forEach(row=>{"
        "const dt=new Date(row.ts*1000);"
        "const time=dt.toLocaleString('es-ES',{year:'numeric',month:'2-digit',day:'2-digit',hour:'2-digit',minute:'2-digit',second:'2-digit'});"
        "const tr=document.createElement('tr');"
        "tr.innerHTML='<td>'+row.seq+'</td><td>'+time+'</td><td>'+(row.t||0).toFixed(1)+'</td><td>'+(row.sp||0).toFixed(1)+'</td><td>'+(row.r||0).toFixed(1)+'</td><td>'+(row.tgt||0).toFixed(1)+'</td><td>'+(row.dur||0)+'</td><td>'+row.prog+'</td><td>'+row.stat+'</td>';"
        "tb.appendChild(tr);"
        "});"
        "document.getElementById('msg-log').textContent=rows.length+' entries displayed';"
        "document.getElementById('spinner-log').style.display='none';"
        "}).catch(e=>{document.getElementById('spinner-log').style.display='none';showMessage('msg-log','Error loading data: '+e,'error');});"
        "}"
        "function refreshDataLog(){"
        "loadDataLog();"
        "}"
        "function clearDataLog(){"
        "if(confirm('Delete all logged data? This cannot be undone.')){fetch('/api/cleardata',{method:'POST'}).then(r=>r.json()).then(d=>{"
        "if(d.success){showMessage('msg-log','All data cleared','success');loadDataLog();}"
        "else{showMessage('msg-log','Failed to clear data: '+(d.error||'Unknown'),'error');}"
        "}).catch(e=>{showMessage('msg-log','Error: '+e,'error');});"
        "}"
        "}"
        "updateStatus();setInterval(updateStatus,5000);"
        "updateKilnStatus();setInterval(updateKilnStatus,1000);"
        "loadCurrentPIDValues();"
        "</script></body></html>"
    );
}

void WirelessManager::handleScan() {
    scanNetworks();
    
    StaticJsonDocument<2048> doc;
    JsonArray networks = doc.createNestedArray("networks");
    bool foundKnownNetwork = false;
    String foundSSID = "";
    int foundSignal = -100;
    
    for (int i = 0; i < cachedNetworkCount; i++) {
        String scannedSSID = WiFi.SSID(i);
        int scannedRSSI = WiFi.RSSI(i);
        
        JsonObject net = networks.createNestedObject();
        net["ssid"] = scannedSSID;
        net["rssi"] = scannedRSSI;
        net["channel"] = WiFi.channel(i);
        net["encryption"] = WiFi.encryptionType(i);
        
        // Check if this is the saved network and has better signal than previously found
        if (scannedSSID == config.ssid && config.ssid.length() > 0 && scannedRSSI > foundSignal) {
            foundKnownNetwork = true;
            foundSSID = scannedSSID;
            foundSignal = scannedRSSI;
            Serial.printf("[WiFi] Known network found: %s (Signal: %d dBm)\n", foundSSID.c_str(), foundSignal);
        }
    }
    
    // If a known network is found and we're not already connected, attempt connection
    if (foundKnownNetwork && WiFi.status() != WL_CONNECTED) {
        Serial.printf("[WiFi] Auto-connecting to known network: %s\n", foundSSID.c_str());
        connectToKnownNetworks();
    }
    
    doc["autoConnectAttempted"] = foundKnownNetwork;
    doc["connectedSSID"] = foundSSID;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void WirelessManager::handleConnect() {
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
    
    config.ssid = doc["ssid"].as<String>();
    config.password = doc["password"].as<String>();
    config.apMode = false;
    
    saveConfig();
    
    // Initiate connection in background
    connectToKnownNetworks();
    
    server.send(200, "application/json", "{\"success\":true}");
}

void WirelessManager::handleReset() {
    if (server.method() != HTTP_POST) {
        server.send(405, "text/plain", "Method Not Allowed");
        return;
    }
    
    resetConfig();
    server.send(200, "application/json", "{\"success\":true}");
    
    // Schedule restart after response
    delay(500);
    ESP.restart();
}

void WirelessManager::handleFirmwareUpload() {
    HTTPUpload& upload = server.upload();
    
    if (upload.status == UPLOAD_FILE_START) {
        firmwareUploadSuccess = false;  // Reset flag at start
        Serial.printf("[OTA] Firmware upload start: %s (size: %u bytes)\n", upload.filename.c_str(), upload.totalSize);
        
        // Start OTA update with unknown size (0 means auto-detect)
        if (!Update.begin(0, U_FLASH)) {
            Serial.println("[OTA] Update.begin() failed!");
            Update.printError(Serial);
            return;
        }
    }
    else if (upload.status == UPLOAD_FILE_WRITE) {
        // Write firmware chunks to flash
        size_t written = Update.write(upload.buf, upload.currentSize);
        if (written != upload.currentSize) {
            Serial.printf("[OTA] Update.write() failed: wrote %u of %u bytes at offset %u\n", written, upload.currentSize, upload.totalSize);
            Update.printError(Serial);
            return;
        }
        
        // Log progress every 50KB
        if (upload.totalSize % 51200 == 0) {
            Serial.printf("[OTA] Progress: %u bytes written\n", upload.totalSize);
        }
        yield();
    }
    else if (upload.status == UPLOAD_FILE_END) {
        // Finalize the update
        Serial.printf("[OTA] Upload complete, finalizing... (total: %u bytes)\n", upload.totalSize);
        
        if (Update.end(true)) {  // true = set size to current progress
            Serial.printf("[OTA] Firmware upload successful: %u bytes\n", upload.totalSize);
            firmwareUploadSuccess = true;
        } else {
            Serial.printf("[OTA] Update.end() failed!\n");
            Update.printError(Serial);
            firmwareUploadSuccess = false;
        }
    }
    else if (upload.status == UPLOAD_FILE_ABORTED) {
        Serial.println("[OTA] Upload aborted!");
        Update.end(false);
        firmwareUploadSuccess = false;
    }
}

void WirelessManager::handleKilnStatus() {
    // Forward kiln status request to main application
    // This function is declared externally in KilnController.cpp
    extern String getKilnStatusJSON();
    String response = getKilnStatusJSON();
    server.send(200, "application/json", response);
}

void WirelessManager::handleDataLog() {
    // Export logger data as JSON
    extern KilnDataLogger& logger;
    String data;
    logger.exportData(data);
    server.send(200, "application/json", data);
}

void WirelessManager::handleClearData() {
    // Clear all logged data
    extern KilnDataLogger& logger;
    logger.clearAll();
    StaticJsonDocument<64> doc;
    doc["success"] = true;
    doc["message"] = "All data cleared";
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

// ============================================================================
// Program Management Handlers
// ============================================================================

void WirelessManager::handleProgramList() {
    extern ProgramManager programManager;
    String jsonList;
    if (programManager.listPrograms(jsonList)) {
        server.send(200, "application/json", jsonList);
    } else {
        server.send(500, "application/json", "{\"success\":false,\"error\":\"Failed to list programs\"}");
    }
}

void WirelessManager::handleProgramLoad() {
    if (server.arg("name").length() == 0) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"No program name provided\"}");
        return;
    }
    
    String name = server.arg("name");
    extern ProgramManager programManager;
    Program program;
    yield();
    
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
        doc.clear();
        yield();
        server.send(200, "application/json", response);
    } else {
        server.send(404, "application/json", "{\"success\":false,\"error\":\"Program not found\"}");
    }
}

void WirelessManager::handleProgramSave() {
    if (server.method() != HTTP_POST) {
        server.send(405, "text/plain", "Method Not Allowed");
        return;
    }
    
    String body = server.arg("plain");
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
    }
    
    String name = doc["name"] | "";
    int seqCount = doc["seqCount"] | 0;
    
    if (name.length() == 0 || seqCount < 1 || seqCount > 9) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid program data\"}");
        return;
    }
    
    Program program;
    strncpy(program.name, name.c_str(), sizeof(program.name) - 1);
    program.name[sizeof(program.name) - 1] = '\0';
    program.seqCount = seqCount;
    
    JsonArray segments = doc["segments"];
    for (int i = 0; i < seqCount && i < 9; i++) {
        if (segments[i].is<JsonObject>()) {
            program.segments[i].rate_c_per_hour = segments[i]["rate_c_per_hour"] | 0.0f;
            program.segments[i].target_c = segments[i]["target_c"] | 0.0f;
            program.segments[i].hold_seconds = segments[i]["hold_seconds"] | 0;
        }
    }
    
    extern ProgramManager programManager;
    yield();
    
    if (programManager.saveProgram(program)) {
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Program saved\"}");
    } else {
        server.send(500, "application/json", "{\"success\":false,\"error\":\"Failed to save program\"}");
    }
    
    doc.clear();
    yield();
}

void WirelessManager::handleProgramDelete() {
    if (server.method() != HTTP_POST) {
        server.send(405, "text/plain", "Method Not Allowed");
        return;
    }
    
    if (server.arg("name").length() == 0) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"No program name provided\"}");
        return;
    }
    
    String name = server.arg("name");
    extern ProgramManager programManager;
    yield();
    
    if (programManager.deleteProgram(name.c_str())) {
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Program deleted\"}");
    } else {
        server.send(500, "application/json", "{\"success\":false,\"error\":\"Failed to delete program\"}");
    }
    yield();
}

// ============================================================================
// PID Tuning Handlers
// ============================================================================

void WirelessManager::handlePIDGet() {
    extern float Kp, Ki, Kd;
    
    StaticJsonDocument<256> doc;
    doc["success"] = true;
    doc["kp"] = Kp;
    doc["ki"] = Ki;
    doc["kd"] = Kd;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void WirelessManager::handlePIDSet() {
    if (server.method() != HTTP_POST) {
        server.send(405, "text/plain", "Method Not Allowed");
        return;
    }
    
    extern float Kp, Ki, Kd;
    
    float kp = (float)server.arg("kp").toDouble();
    float ki = (float)server.arg("ki").toDouble();
    float kd = (float)server.arg("kd").toDouble();
    
    // Validate ranges
    if (kp < 0 || kp > 40 || ki < 0 || ki > 40 || kd < 0 || kd > 40) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Values out of range\"}");
        return;
    }
    
    Kp = kp;
    Ki = ki;
    Kd = kd;
    
    // Save to flash
    DynamicJsonDocument doc(256);
    doc["kp"] = Kp;
    doc["ki"] = Ki;
    doc["kd"] = Kd;
    
    File file = LittleFS.open("/pid.json", "w");
    if (file) {
        serializeJson(doc, file);
        file.close();
        doc.clear();
        yield();
        server.send(200, "application/json", "{\"success\":true,\"message\":\"PID values saved\"}");
    } else {
        doc.clear();
        server.send(500, "application/json", "{\"success\":false,\"error\":\"Failed to save PID values\"}");
    }
}

void WirelessManager::handleSSRRateGet() {
    extern unsigned long SSR_CHANGE_RATE_LIMIT;
    
    StaticJsonDocument<256> doc;
    doc["success"] = true;
    doc["rateMs"] = SSR_CHANGE_RATE_LIMIT;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void WirelessManager::handleSSRRateSet() {
    if (server.method() != HTTP_POST) {
        server.send(405, "text/plain", "Method Not Allowed");
        return;
    }
    
    extern unsigned long SSR_CHANGE_RATE_LIMIT;
    extern void setSSRRateLimit(unsigned long rateMs);
    
    unsigned long rateMs = (unsigned long)server.arg("rateMs").toInt();
    
    // Validate range (100ms to 10 seconds)
    if (rateMs < 100 || rateMs > 10000) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Rate must be 100-10000 ms\"}");
        return;
    }
    
    setSSRRateLimit(rateMs);
    
    StaticJsonDocument<256> doc;
    doc["success"] = true;
    doc["rateMs"] = SSR_CHANGE_RATE_LIMIT;
    doc["message"] = "SSR rate limit updated";
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

// ============================================================================
// Button Action Handlers
// ============================================================================

void WirelessManager::handleButtonStartStop() {
    if (server.method() != HTTP_POST) {
        server.send(405, "text/plain", "Method Not Allowed");
        return;
    }
    
    extern void buttonActionStartStop();
    buttonActionStartStop();
    
    server.send(200, "application/json", "{\"success\":true}");
}

void WirelessManager::handleButtonCycleProgram() {
    if (server.method() != HTTP_POST) {
        server.send(405, "text/plain", "Method Not Allowed");
        return;
    }
    
    extern void buttonActionCycleProgram();
    buttonActionCycleProgram();
    
    server.send(200, "application/json", "{\"success\":true}");
}

void WirelessManager::handleNotFound() {
    server.send(404, "text/plain", "Not Found");
}

// ============================================================================
// Status Information
// ============================================================================

String WirelessManager::getStatusJSON() {
    StaticJsonDocument<256> doc;
    
    doc["connected"] = WiFi.status() == WL_CONNECTED;
    doc["mode"] = (WiFi.status() == WL_CONNECTED) ? "STA" : "AP";
    
    if (WiFi.status() == WL_CONNECTED) {
        doc["ip"] = WiFi.localIP().toString();
        doc["gateway"] = WiFi.gatewayIP().toString();
        doc["rssi"] = WiFi.RSSI();
        doc["ssid"] = WiFi.SSID();
    } else {
        doc["ip"] = WiFi.softAPIP().toString();
        doc["rssi"] = 0;
        doc["ssid"] = AP_SSID;
    }
    
    String response;
    serializeJson(doc, response);
    return response;
}

bool WirelessManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

String WirelessManager::getAPSSID() {
    return String(AP_SSID);
}

// ============================================================================
// OTA Firmware Update
// ============================================================================

void setupOTA() {
    ArduinoOTA.setHostname("kilncontroller");
    
    ArduinoOTA.onStart([]() {
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
        Serial.println("[OTA] Start updating " + type);
    });
    
    ArduinoOTA.onEnd([]() {
        Serial.println("\n[OTA] Update complete!");
    });
    
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("[OTA] Progress: %u%%\r", (progress / (total / 100)));
    });
    
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("[OTA] Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) {
            Serial.println("Auth Failed");
        } else if (error == OTA_BEGIN_ERROR) {
            Serial.println("Begin Failed");
        } else if (error == OTA_CONNECT_ERROR) {
            Serial.println("Connect Failed");
        } else if (error == OTA_RECEIVE_ERROR) {
            Serial.println("Receive Failed");
        } else if (error == OTA_END_ERROR) {
            Serial.println("End Failed");
        }
    });
    
    ArduinoOTA.begin();
    Serial.println("[OTA] OTA updates enabled");
}
