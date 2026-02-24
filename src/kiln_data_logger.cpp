#include <kiln_data_logger.h>

// Static instance
KilnDataLogger KilnDataLogger::instance;
KilnDataLogger& logger = KilnDataLogger::getInstance();

// Constructor
KilnDataLogger::KilnDataLogger()
    : runSequence(0), lastCleanupTime(0), lastLogTime(0), 
      currentRunStartTime(0), programWasRunning(false) {
}

// ============================================================================
// Initialization
// ============================================================================

void KilnDataLogger::begin() {
    Serial.println("[Logger] Initializing Kiln Data Logger");
    
    if (!LittleFS.begin()) {
        Serial.println("[Logger] ERROR: LittleFS initialization failed");
        return;
    }
    
    // Initialize system time (needed for timestamps)
    initializeTime();
    
    // Load sequence number
    loadSequence();
    
    // Initialize cleanup timer
    lastCleanupTime = millis();
    
    Serial.printf("[Logger] Data logger initialized, sequence: %u\n", runSequence);
}

// ============================================================================
// Time initialization
// ============================================================================

void KilnDataLogger::initializeTime() {
    // Set system time (for ESP8266, you can use NTP)
    // For now, we'll use device uptime from millis()
    // In production, consider syncing with NTP server
    time_t now = time(nullptr);
    if (now < 24 * 3600) {
        // Time not set, use a reasonable default
        // This is okay for relative timestamps
        Serial.println("[Logger] System time not set, using relative timestamps");
    }
}

// ============================================================================
// Sequence Management
// ============================================================================

void KilnDataLogger::loadSequence() {
    if (!LittleFS.exists("/seq.txt")) {
        runSequence = 1;
        saveSequence();
        return;
    }
    
    File f = LittleFS.open("/seq.txt", "r");
    if (f) {
        String content = f.readString();
        f.close();
        runSequence = content.toInt();
        if (runSequence < 1) runSequence = 1;
    } else {
        runSequence = 1;
    }
}

void KilnDataLogger::saveSequence() {
    File f = LittleFS.open("/seq.txt", "w");
    if (f) {
        f.printf("%u\n", runSequence);
        f.close();
    }
}

// ============================================================================
// Core Logging
// ============================================================================

void KilnDataLogger::logData(float temperature, float setpoint, float rate, 
                             float target, uint32_t duration, 
                             const char* program, const char* status) {
    // Only log if we have valid inputs
    if (program == nullptr || status == nullptr) return;
    
    unsigned long now = millis();

    // Rate limit logging (max once per 10 minutes)
    if (now - lastLogTime < 600000) return;
    lastLogTime = now;
    
    // Create data point
    DataPoint point;
    point.timestamp = (uint32_t)(time(nullptr));
    point.temperature = temperature;
    point.setpoint = setpoint;
    point.rate = rate;
    point.target = target;
    point.duration = duration;
    point.sequence = runSequence;
    point.program = program;
    point.status = status;
    
    // Append to database
    appendToDatabase(point);
    
    // Periodic cleanup (check every hour)
    if (now - lastCleanupTime > AUTO_CLEANUP_INTERVAL_MS) {
        cleanup();
        lastCleanupTime = now;
    }
}

void KilnDataLogger::appendToDatabase(const DataPoint& point) {
    // Load existing database or create new one
    DynamicJsonDocument doc(8192);  // 8KB buffer for JSON
    JsonArray data;
    
    // Try to load existing data
    if (LittleFS.exists(DATA_LOG_FILE)) {
        File f = LittleFS.open(DATA_LOG_FILE, "r");
        if (f) {
            DeserializationError error = deserializeJson(doc, f);
            f.close();
            
            if (error) {
                Serial.printf("[Logger] JSON parse error: %s, creating new database\n", error.c_str());
                doc.clear();
                data = doc.createNestedArray("data");
            } else {
                data = doc["data"];
                if (data.isNull()) {
                    doc.clear();
                    data = doc.createNestedArray("data");
                }
            }
        } else {
            data = doc.createNestedArray("data");
        }
    } else {
        data = doc.createNestedArray("data");
    }
    
    // Check if we're at max capacity
    if (data.size() >= MAX_DATA_POINTS) {
        Serial.println("[Logger] WARNING: Max data points reached, removing oldest entries");
        // Remove oldest 3600 entries (1 hour)
        for (int i = 0; i < 3600 && data.size() > 0; i++) {
            data.remove(0);
        }
    }
    
    // Add new data point
    JsonObject pointObj = data.createNestedObject();
    pointObj["ts"] = point.timestamp;
    pointObj["t"] = point.temperature;
    pointObj["sp"] = point.setpoint;
    pointObj["r"] = point.rate;
    pointObj["tgt"] = point.target;
    pointObj["dur"] = point.duration;
    pointObj["seq"] = point.sequence;
    pointObj["prog"] = point.program;
    pointObj["stat"] = point.status;
    
    // Save to file
    File f = LittleFS.open(DATA_LOG_FILE, "w");
    if (f) {
        serializeJson(doc, f);
        f.close();
        Serial.printf("[Logger] Data logged: T=%.1fC SP=%.1fC Status=%s\n", 
                     point.temperature, point.setpoint, point.status);
    } else {
        Serial.println("[Logger] ERROR: Failed to open database file for writing");
    }
}

// ============================================================================
// Program Lifecycle
// ============================================================================

void KilnDataLogger::onProgramStart() {
    runSequence++;
    saveSequence();
    currentRunStartTime = time(nullptr);
    programWasRunning = true;
    Serial.printf("[Logger] Program started, sequence: %u\n", runSequence);
}

void KilnDataLogger::onProgramStop() {
    programWasRunning = false;
    Serial.printf("[Logger] Program stopped, sequence: %u\n", runSequence);
}

// ============================================================================
// Data Cleanup
// ============================================================================

void KilnDataLogger::cleanup() {
    Serial.println("[Logger] Starting data cleanup...");
    
    if (!LittleFS.exists(DATA_LOG_FILE)) {
        Serial.println("[Logger] No data file to clean");
        return;
    }
    
    // Load database
    DynamicJsonDocument doc(8192);
    File f = LittleFS.open(DATA_LOG_FILE, "r");
    if (!f) {
        Serial.println("[Logger] ERROR: Could not open data file for cleanup");
        return;
    }
    
    DeserializationError error = deserializeJson(doc, f);
    f.close();
    
    if (error) {
        Serial.printf("[Logger] JSON parse error during cleanup: %s\n", error.c_str());
        return;
    }
    
    JsonArray data = doc["data"];
    if (data.isNull()) {
        Serial.println("[Logger] No data array found in database");
        return;
    }
    
    // Get current time
    time_t now = time(nullptr);
    time_t cutoffTime = now - DATA_RETENTION_SECONDS;
    
    int removed = 0;
    
    // Remove old entries (iterate backwards to avoid index issues)
    for (int i = data.size() - 1; i >= 0; i--) {
        JsonObject point = data[i];
        uint32_t timestamp = point["ts"];
        
        if (timestamp < cutoffTime) {
            data.remove(i);
            removed++;
        }
    }
    
    // Save cleaned database if any entries were removed
    if (removed > 0) {
        f = LittleFS.open(DATA_LOG_FILE, "w");
        if (f) {
            serializeJson(doc, f);
            f.close();
            Serial.printf("[Logger] Cleanup complete: removed %d old entries (kept %d)\n", 
                         removed, data.size());
        } else {
            Serial.println("[Logger] ERROR: Could not save cleaned database");
        }
    } else {
        Serial.printf("[Logger] Cleanup complete: no old data to remove\n");
    }
}

// ============================================================================
// Utility Functions
// ============================================================================

void KilnDataLogger::printStats() {
    if (!LittleFS.exists(DATA_LOG_FILE)) {
        Serial.println("[Logger] No data available");
        return;
    }
    
    DynamicJsonDocument doc(8192);
    File f = LittleFS.open(DATA_LOG_FILE, "r");
    
    if (!f) {
        Serial.println("[Logger] Could not open data file");
        return;
    }
    
    DeserializationError error = deserializeJson(doc, f);
    f.close();
    
    if (error) {
        Serial.printf("[Logger] JSON error: %s\n", error.c_str());
        return;
    }
    
    JsonArray data = doc["data"];
    Serial.printf("[Logger] Database stats:\n");
    Serial.printf("  Total entries: %u\n", data.size());
    
    if (data.size() > 0) {
        JsonObject first = data[0];
        JsonObject last = data[data.size() - 1];
        
        Serial.printf("  First entry timestamp: %u\n", (uint32_t)first["ts"]);
        Serial.printf("  Last entry timestamp: %u\n", (uint32_t)last["ts"]);
        Serial.printf("  Program: %s\n", (const char*)last["prog"]);
        Serial.printf("  Latest status: %s\n", (const char*)last["stat"]);
        
        // Check file size
        f = LittleFS.open(DATA_LOG_FILE, "r");
        if (f) {
            size_t fileSize = f.size();
            f.close();
            Serial.printf("  File size: %u bytes\n", fileSize);
        }
    }
}

void KilnDataLogger::clearAll() {
    if (LittleFS.remove(DATA_LOG_FILE)) {
        Serial.println("[Logger] All data cleared");
        runSequence = 1;
        saveSequence();
    } else {
        Serial.println("[Logger] ERROR: Could not clear data");
    }
}

void KilnDataLogger::exportData(String& output) {
    if (!LittleFS.exists(DATA_LOG_FILE)) {
        output = "{\"data\":[]}";
        return;
    }
    
    File f = LittleFS.open(DATA_LOG_FILE, "r");
    if (!f) {
        output = "{\"data\":[]}";
        return;
    }
    
    output = f.readString();
    f.close();
}
