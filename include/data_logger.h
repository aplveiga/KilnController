#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// Data log entry structure - keeps track of kiln state at time of logging
struct DataLogEntry {
    uint32_t timestamp;       // Unix timestamp (seconds since epoch)
    uint32_t sequence;        // Sequential number persisted across boots
    float temperature;        // Current temperature in °C
    float setpoint;          // SP (Set Point) in °C
    float target;            // Target temperature for current segment
    char program[64];        // Program name
    char status[16];         // Status: IDLE, RAMP, HOLD, PAUSE, FAULT
};

#define MAX_LOG_ENTRIES 3000  // Total entries to keep
#define CACHE_SIZE 20         // Small in-memory cache for fast access
#define DATA_LOGGER_VERSION 2 // Version number for format compatibility

class DataLogger {
public:
    DataLogger();
    
    // Logging operations
    bool logEntry(uint32_t timestamp, float temperature, float setpoint, 
                  float target, const char* program, const char* status);
    
    // Data retrieval
    bool getEntries(DynamicJsonDocument& doc);
    bool exportToCSV(String& csvContent);
    
    // Management
    bool clearLogs();
    uint16_t getEntryCount() const;
    
private:
    const char* LOG_FILE = "/kiln_data.bin";      // Binary flash storage
    const char* LOG_INDEX_FILE = "/kiln_index.txt"; // Index info
    const char* SEQ_FILE = "/log_seq.txt";        // Persistent sequence counter
    
    // Small in-memory cache only (reduces RAM usage from 288KB to ~2KB)
    DataLogEntry cache[CACHE_SIZE];
    uint16_t cacheCount;        // Current entries in cache
    uint16_t cacheIndex;        // Position in circular cache
    uint16_t totalEntryCount;   // Total entries stored (in flash)
    uint32_t nextSequence;      // Next sequence number to assign
    
    // Helper functions
    bool appendToFlash(const DataLogEntry& entry);
    bool loadCacheFromFlash();
    bool saveCacheInfo();
    void initCache();
    uint32_t loadSequenceCounter();
    void saveSequenceCounter();
};

extern DataLogger dataLogger;

#endif // DATA_LOGGER_H
