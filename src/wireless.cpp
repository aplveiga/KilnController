// ESP8266 WiFi Manager with Web Configuration Portal
// Features: WiFi connection, AP mode fallback, web configuration, OTA updates, flash storage

#include <wireless.h>
#include <kiln_data_logger.h>
#include <time.h>

// Forward declaration
void setupOTA();

// Global instance
WirelessManager wirelessManager;

// Constructor
WirelessManager::WirelessManager() 
    : server(80), lastConnectionAttempt(0), lastScanTime(0), lastNTPSync(0), cachedNetworkCount(0), ntpSynced(false) {
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
    server.on("/api/firmware", [this]() { handleFirmwareUpload(); });
    server.on("/api/data", [this]() { handleDataLog(); });
    server.on("/api/cleardata", [this]() { handleClearData(); });
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
    if (server.method() != HTTP_POST) {
        server.send(405, "text/plain", "Method Not Allowed");
        return;
    }
    
    // Check for multipart form data (file upload via FormData)
    if (server.hasHeader("Content-Type")) {
        String contentType = server.header("Content-Type");
        if (contentType.indexOf("multipart/form-data") != -1) {
            // Get the total size from Content-Length header
            uint32_t contentLength = 0;
            if (server.hasHeader("Content-Length")) {
                contentLength = atoi(server.header("Content-Length").c_str());
            }
            
            // Validate firmware size (max 4MB for OTA partition)
            if (contentLength > 0 && contentLength <= 4194304) {
                // Start OTA update
                Serial.printf("[OTA] Starting firmware update, size: %d bytes\n", contentLength);
                
                if (!Update.begin(contentLength, U_FLASH)) {
                    Serial.println("[OTA] Update.begin() failed");
                    server.send(400, "application/json", "{\"success\":false,\"error\":\"OTA initialization failed\"}");
                    return;
                }
                
                // Read multipart body and extract binary data
                uint32_t written = 0;
                uint8_t buf[512];
                bool inData = false;
                
                while (server.client().connected() && written < contentLength) {
                    int available = server.client().available();
                    if (available > 0) {
                        int len = server.client().readBytes(buf, min(available, 512));
                        if (len > 0) {
                            // Find firmware data start (after multipart headers)
                            if (!inData) {
                                // Look for the double CRLF that ends multipart headers
                                for (int i = 0; i < len - 3; i++) {
                                    if (buf[i] == '\r' && buf[i+1] == '\n' && 
                                        buf[i+2] == '\r' && buf[i+3] == '\n') {
                                        inData = true;
                                        int dataStart = i + 4;
                                        int dataLen = len - dataStart - 2; // Subtract trailing CRLF
                                        if (dataLen > 0) {
                                            Update.write(&buf[dataStart], dataLen);
                                            written += dataLen;
                                        }
                                        break;
                                    }
                                }
                            } else {
                                // Skip trailing boundary (last 2 bytes are CRLF before boundary)
                                int writeLen = len - 2;
                                if (writeLen > 0) {
                                    Update.write(buf, writeLen);
                                    written += writeLen;
                                }
                            }
                        }
                    } else {
                        yield(); // Prevent watchdog reset
                    }
                }
                
                // Finalize OTA update
                if (Update.end()) {
                    Serial.println("[OTA] Firmware update successful, rebooting...");
                    server.send(200, "application/json", "{\"success\":true,\"message\":\"Firmware updated successfully. Rebooting...\"}");
                    delay(1000);
                    ESP.restart();
                } else {
                    Serial.printf("[OTA] Update failed, error: %d\n", Update.getError());
                    server.send(400, "application/json", "{\"success\":false,\"error\":\"Firmware write failed\"}");
                }
            } else {
                server.send(413, "application/json", "{\"success\":false,\"error\":\"Firmware file too large (max 4MB) or invalid\"}");
            }
            return;
        }
    }
    
    // Not a multipart upload
    server.send(400, "application/json", "{\"success\":false,\"error\":\"No firmware file provided or invalid format\"}");
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
