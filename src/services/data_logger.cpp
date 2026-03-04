#include <data_logger.h>

DataLogger dataLogger;

DataLogger::DataLogger() : cacheCount(0), cacheIndex(0), totalEntryCount(0), nextSequence(0) {
    initCache();
    
    // Check data logger version for compatibility
    if (!LittleFS.exists(LOG_INDEX_FILE)) {
        Serial.println("[DataLogger] Version check: Fresh start");
    } else {
        // Read version from index file
        File file = LittleFS.open(LOG_INDEX_FILE, "r");
        if (file) {
            DynamicJsonDocument doc(256);
            deserializeJson(doc, file);
            file.close();
            
            int savedVersion = doc["version"] | 0;
            if (savedVersion != DATA_LOGGER_VERSION) {
                Serial.printf("[DataLogger] Version mismatch: saved=%d, current=%d. Clearing logs for compatibility.\n", 
                            savedVersion, DATA_LOGGER_VERSION);
                clearLogs();
                return;
            }
        }
    }
    
    nextSequence = loadSequenceCounter();
    loadCacheFromFlash();
}

void DataLogger::initCache() {
    memset(cache, 0, sizeof(cache));
    cacheCount = 0;
    cacheIndex = 0;
}

bool DataLogger::logEntry(uint32_t timestamp, float temperature, float setpoint,
                          float target, const char* program, const char* status) {
    // Validate inputs
    if (!program || !status) {
        Serial.println("[DataLogger] Invalid log entry: null program or status");
        return false;
    }
    
    // Create entry
    DataLogEntry entry;
    entry.timestamp = timestamp;
    entry.sequence = nextSequence++;
    entry.temperature = temperature;
    entry.setpoint = setpoint;
    entry.target = target;
    
    // Copy strings safely
    strncpy(entry.program, program, sizeof(entry.program) - 1);
    entry.program[sizeof(entry.program) - 1] = '\0';
    strncpy(entry.status, status, sizeof(entry.status) - 1);
    entry.status[sizeof(entry.status) - 1] = '\0';
    
    // Add to in-memory cache
    cache[cacheIndex] = entry;
    if (cacheCount < CACHE_SIZE) {
        cacheCount++;
    }
    cacheIndex = (cacheIndex + 1) % CACHE_SIZE;
    
    // Append to flash storage
    if (!appendToFlash(entry)) {
        Serial.println("[DataLogger] Warning: Failed to save log entry to flash");
        return false;
    }
    
    // Save sequence counter to persist across boots
    saveSequenceCounter();
    
    Serial.printf("[DataLogger] Entry logged: Seq=%u T=%.1f SP=%.1f Prog=%s Status=%s (Total: %u)\n",
                  entry.sequence, temperature, setpoint, program, status, totalEntryCount);
    return true;
}

bool DataLogger::getEntries(DynamicJsonDocument& doc) {
    // Read entries from flash and create JSON
    if (!LittleFS.exists(LOG_FILE)) {
        doc.createNestedArray("entries");
        doc["count"] = 0;
        doc["max"] = (int)MAX_LOG_ENTRIES;
        return true;
    }
    
    File file = LittleFS.open(LOG_FILE, "r");
    if (!file) {
        Serial.println("[DataLogger] Failed to open log file for reading");
        return false;
    }
    
    JsonArray entries_array = doc.createNestedArray("entries");
    
    // Read all entries from flash
    DataLogEntry entry;
    while (file.read((uint8_t*)&entry, sizeof(DataLogEntry)) == sizeof(DataLogEntry)) {
        JsonObject entry_obj = entries_array.createNestedObject();
        entry_obj["sequence"] = entry.sequence;
        entry_obj["timestamp"] = entry.timestamp;
        entry_obj["temperature"] = entry.temperature;
        entry_obj["setpoint"] = entry.setpoint;
        entry_obj["target"] = entry.target;
        entry_obj["program"] = entry.program;
        entry_obj["status"] = entry.status;
        yield();  // Feed watchdog during file read
    }
    
    file.close();
    yield();
    
    doc["count"] = totalEntryCount;
    doc["max"] = (int)MAX_LOG_ENTRIES;
    
    return true;
}

bool DataLogger::exportToCSV(String& csvContent) {
    csvContent = "";
    
    // CSV Header
    csvContent += "Sequence,Timestamp,Temperature(C),Setpoint(C),Target(C),Program,Status\n";
    
    if (!LittleFS.exists(LOG_FILE)) {
        return true;  // Return empty CSV if no data
    }
    
    File file = LittleFS.open(LOG_FILE, "r");
    if (!file) {
        Serial.println("[DataLogger] Failed to open log file for export");
        return false;
    }
    
    // Read all entries and format as CSV
    DataLogEntry entry;
    uint32_t lineCount = 0;
    while (file.read((uint8_t*)&entry, sizeof(DataLogEntry)) == sizeof(DataLogEntry)) {
        csvContent += String(entry.sequence) + ",";
        csvContent += String(entry.timestamp) + ",";
        csvContent += String(entry.temperature, 2) + ",";
        csvContent += String(entry.setpoint, 2) + ",";
        csvContent += String(entry.target, 2) + ",";
        csvContent += String(entry.program) + ",";
        csvContent += String(entry.status);
        csvContent += "\n";
        
        lineCount++;
        if (lineCount % 50 == 0) {
            yield();  // Feed watchdog periodically
        }
    }
    
    file.close();
    yield();
    
    Serial.printf("[DataLogger] CSV export: %u entries, %u bytes\n", lineCount, csvContent.length());
    return true;
}

bool DataLogger::clearLogs() {
    initCache();
    totalEntryCount = 0;
    nextSequence = 0;
    
    // Remove flash files
    if (LittleFS.exists(LOG_FILE)) {
        LittleFS.remove(LOG_FILE);
    }
    if (LittleFS.exists(LOG_INDEX_FILE)) {
        LittleFS.remove(LOG_INDEX_FILE);
    }
    if (LittleFS.exists(SEQ_FILE)) {
        LittleFS.remove(SEQ_FILE);
    }
    
    // Save current version to index file so we don't keep clearing
    saveCacheInfo();
    saveSequenceCounter();
    
    Serial.println("[DataLogger] All logs cleared");
    return true;
}

uint16_t DataLogger::getEntryCount() const {
    return totalEntryCount;
}

bool DataLogger::appendToFlash(const DataLogEntry& entry) {
    // Open file in append mode and add entry
    File file = LittleFS.open(LOG_FILE, "a");
    if (!file) {
        Serial.println("[DataLogger] Failed to open log file for append");
        return false;
    }
    
    // Check if we've exceeded max entries
    size_t fileSize = file.size();
    uint32_t currentEntries = fileSize / sizeof(DataLogEntry);
    
    // If we've hit the maximum, we need to rotate the file by removing oldest entry
    file.close();
    yield();
    
    if (currentEntries >= MAX_LOG_ENTRIES) {
        // Read all entries, skip first one, write back
        File readFile = LittleFS.open(LOG_FILE, "r");
        if (!readFile) {
            return false;
        }
        
        // Skip first entry (oldest)
        readFile.read((uint8_t*)&entry, sizeof(DataLogEntry));
        
        // Create temporary file
        File tempFile = LittleFS.open("/temp.bin", "w");
        if (!tempFile) {
            readFile.close();
            return false;
        }
        
        // Copy rest of entries
        DataLogEntry tempEntry;
        while (readFile.read((uint8_t*)&tempEntry, sizeof(DataLogEntry)) == sizeof(DataLogEntry)) {
            tempFile.write((uint8_t*)&tempEntry, sizeof(DataLogEntry));
            yield();
        }
        
        // Write new entry at the end
        tempFile.write((uint8_t*)&entry, sizeof(DataLogEntry));
        
        readFile.close();
        tempFile.close();
        yield();
        
        // Replace original with temp
        LittleFS.remove(LOG_FILE);
        LittleFS.rename("/temp.bin", LOG_FILE);
        yield();
        
        // Entry count stays at MAX
        totalEntryCount = MAX_LOG_ENTRIES;
    } else {
        // File not full yet, just append
        file = LittleFS.open(LOG_FILE, "a");
        if (!file) {
            return false;
        }
        file.write((uint8_t*)&entry, sizeof(DataLogEntry));
        file.close();
        yield();
        
        totalEntryCount = currentEntries + 1;
    }
    
    // Save index info
    saveCacheInfo();
    
    return true;
}

bool DataLogger::loadCacheFromFlash() {
    if (!LittleFS.exists(LOG_FILE)) {
        totalEntryCount = 0;
        return false;
    }
    
    // Load last CACHE_SIZE entries into memory for quick access
    File file = LittleFS.open(LOG_FILE, "r");
    if (!file) {
        return false;
    }
    
    // Count total entries
    size_t fileSize = file.size();
    totalEntryCount = fileSize / sizeof(DataLogEntry);
    
    // Load last N entries into cache
    if (totalEntryCount <= CACHE_SIZE) {
        // Load all entries
        cacheCount = totalEntryCount;
        file.seek(0);
        for (int i = 0; i < totalEntryCount; i++) {
            file.read((uint8_t*)&cache[i], sizeof(DataLogEntry));
        }
        cacheIndex = (cacheCount % CACHE_SIZE);
    } else {
        // Load last CACHE_SIZE entries
        uint32_t skipEntries = totalEntryCount - CACHE_SIZE;
        file.seek(skipEntries * sizeof(DataLogEntry));
        cacheCount = CACHE_SIZE;
        for (int i = 0; i < CACHE_SIZE; i++) {
            file.read((uint8_t*)&cache[i], sizeof(DataLogEntry));
        }
        cacheIndex = 0;
    }
    
    file.close();
    yield();
    
    Serial.printf("[DataLogger] Loaded cache: %u/%u entries from flash\n", cacheCount, totalEntryCount);
    return totalEntryCount > 0;
}

bool DataLogger::saveCacheInfo() {
    // Save metadata about the log
    DynamicJsonDocument doc(256);
    doc["version"] = DATA_LOGGER_VERSION;
    doc["totalEntries"] = totalEntryCount;
    doc["maxEntries"] = (int)MAX_LOG_ENTRIES;
    doc["timestamp"] = time(nullptr);
    
    File file = LittleFS.open(LOG_INDEX_FILE, "w");
    if (file) {
        serializeJson(doc, file);
        file.close();
        yield();
    }
    
    return true;
}

uint32_t DataLogger::loadSequenceCounter() {
    // Load the sequence counter from persistent file
    if (!LittleFS.exists(SEQ_FILE)) {
        Serial.println("[DataLogger] Sequence file not found, starting from 0");
        return 0;
    }
    
    File file = LittleFS.open(SEQ_FILE, "r");
    if (!file) {
        return 0;
    }
    
    // Read the sequence number as text
    String seqStr = "";
    while (file.available()) {
        seqStr += (char)file.read();
    }
    file.close();
    yield();
    
    uint32_t seq = seqStr.toInt();
    Serial.printf("[DataLogger] Loaded sequence counter: %u\n", seq);
    return seq;
}

void DataLogger::saveSequenceCounter() {
    // Save the sequence counter to persistent file
    File file = LittleFS.open(SEQ_FILE, "w");
    if (file) {
        file.print(nextSequence);
        file.close();
        yield();
    }
}

