#ifndef KILN_DATA_LOGGER_H
#define KILN_DATA_LOGGER_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <time.h>

// ============================================================================
// Data Logger for Kiln Controller - LittleFS + JSON
// Logs per-second: Temperature, Set Point, Program, Rate, Target, Duration, Sequence
// Auto-deletes data older than 24 hours
// ============================================================================

#define DATA_LOG_FILE "/kiln_data.json"
#define MAX_DATA_POINTS 86400  // 24 hours * 60 min * 60 sec = 86,400 seconds
#define AUTO_CLEANUP_INTERVAL_MS 3600000  // Check every hour (3600000ms)
#define DATA_RETENTION_HOURS 24
#define DATA_RETENTION_SECONDS (DATA_RETENTION_HOURS * 3600)

// Single data point structure (logged per second)
struct DataPoint {
    uint32_t timestamp;      // Unix timestamp (seconds since epoch)
    float temperature;       // Current temperature in °C
    float setpoint;          // Current setpoint in °C
    float rate;              // Rate in °C/h
    float target;            // Target temperature in °C
    uint32_t duration;       // Total program duration in seconds
    uint16_t sequence;       // Sequence number (run ID)
    const char* program;     // Program name
    const char* status;      // Status: RAMP, HOLD, PAUSE, IDLE, FAULT
};

class KilnDataLogger {
private:
    uint16_t runSequence;
    unsigned long lastCleanupTime;
    unsigned long lastLogTime;
    uint32_t currentRunStartTime;
    bool programWasRunning;
    
    // Private singleton
    static KilnDataLogger instance;
    KilnDataLogger();
    
public:
    // Singleton pattern
    static KilnDataLogger& getInstance() {
        return instance;
    }
    
    // Core functions
    void begin();
    void logData(float temperature, float setpoint, float rate, float target, 
                 uint32_t duration, const char* program, const char* status);
    void onProgramStart();
    void onProgramStop();
    void cleanup();  // Remove old data
    
    // Utility functions
    uint16_t getSequence() const { return runSequence; }
    void printStats();
    void clearAll();
    void exportData(String& output);  // Export all data as JSON string
    
private:
    void loadSequence();
    void saveSequence();
    void appendToDatabase(const DataPoint& point);
    void removeOldData();
    void initializeTime();
};

// Global logger instance for easy access
extern KilnDataLogger& logger;

#endif
