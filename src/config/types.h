// ============================================================================
// Common Types & Enumerations
// ============================================================================

#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>

// ============================================================================
// Enumerations
// ============================================================================

enum class KilnMode {
    IDLE = 0,       // No program running
    RAMPING = 1,    // Heating/cooling to target
    HOLDING = 2,    // Maintaining target temperature
    PAUSED = 3,     // Program paused
    FAULT = 4,      // Sensor or safety fault
    COOLING = 5     // Post-program cooldown
};

enum class SafetyAlert {
    NONE = 0,
    SENSOR_FAULT = 1,
    OVERHEAT = 2,
    RATE_EXCEEDED = 3,
    PROGRAM_ERROR = 4
};

// ============================================================================
// Data Structures
// ============================================================================

// PID Tuning Parameters
struct PIDParams {
    float kp;           // Proportional gain
    float ki;           // Integral gain
    float kd;           // Derivative gain
    
    PIDParams(float p = 15.5f, float i = 13.1f, float d = 1.20f) 
        : kp(p), ki(i), kd(d) {}
};

// Program Segment Definition
struct Segment {
    float rate_c_per_hour;  // °C/h rate (negative for cooling)
    float target_c;         // Target temperature
    uint32_t hold_seconds;  // Hold duration after reaching target
};

// Program Definition
struct Program {
    char name[64];                  // Program name
    Segment segments[9];            // Array of segments (max 9)
    uint8_t segment_count;          // Number of actual segments
    
    Program() : segment_count(0) {
        memset(name, 0, sizeof(name));
    }
};

// Single Data Point for Logging
struct KilnReading {
    uint32_t timestamp;             // Unix timestamp (seconds)
    float temperature;              // Current temperature °C
    float setpoint;                 // Current setpoint °C
    float rate;                     // Rate in °C/h
    float target;                   // Target temperature °C
    uint32_t duration;              // Elapsed time (seconds)
    uint16_t sequence;              // Run sequence number
    const char* program_name;       // Program name (pointer)
    const char* status;             // "RAMP", "HOLD", "PAUSE", "IDLE", "FAULT"
};

// Current Kiln Status
struct KilnStatus {
    float temperature;              // Current temperature °C
    float setpoint;                 // Current setpoint °C
    float pid_output;               // PID output (0-100%)
    
    KilnMode mode;                  // Current operating mode
    const Program* program;         // Pointer to current program (nullptr if idle)
    uint8_t segment_index;          // Current segment index
    uint8_t segment_count;          // Total segments
    
    bool ssr_on;                    // SSR relay state
    bool sensor_fault;              // Sensor is faulted
    SafetyAlert alert;              // Current alert (if any)
    
    // Timing information
    unsigned long segment_elapsed;  // Seconds since segment start
    unsigned long hold_elapsed;     // Seconds into hold (0 if not holding)
    unsigned long hold_remaining;   // Seconds until hold complete
    
    bool is_paused;                 // Program paused
    bool is_holding;                // Program in hold phase
    
    // Convenience getters
    bool is_running() const { return mode == KilnMode::RAMPING || mode == KilnMode::HOLDING; }
    bool is_error() const { return mode == KilnMode::FAULT; }
    bool is_idle() const { return mode == KilnMode::IDLE; }
};

// WiFi Configuration
struct WiFiConfig {
    char ssid[64];                  // Network SSID
    char password[64];              // Network password
    bool ap_mode;                   // true = AP mode, false = STA mode
    
    WiFiConfig() : ap_mode(true) {
        memset(ssid, 0, sizeof(ssid));
        memset(password, 0, sizeof(password));
    }
};

// HTTP Request/Response (simple abstraction)
struct HttpRequest {
    String method;                  // "GET", "POST", "PUT", "DELETE"
    String path;                    // URI path
    String body;                    // Request body
    String query;                   // Query parameters
};

struct HttpResponse {
    int status_code;                // HTTP status (200, 404, 500, etc.)
    String content_type;            // "application/json", "text/html"
    String body;                    // Response body
    
    HttpResponse() : status_code(200), content_type("application/json") {}
};

#endif // TYPES_H
