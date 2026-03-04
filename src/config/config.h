// ============================================================================
// Configuration Header - All constants in one place
// ============================================================================

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

namespace Pins {
    // I2C Communication for SSD1306 Display
    constexpr int I2C_SDA = D5;      // GPIO14
    constexpr int I2C_SCL = D6;      // GPIO12
    
    // SPI Communication for MAX6675 Thermocouple
    constexpr int THERMOCOUPLE_SCK = D4;  // GPIO2
    constexpr int THERMOCOUPLE_CS  = D3;  // GPIO0 (boot pin)
    constexpr int THERMOCOUPLE_OUT = D2;  // GPIO4
    
    // Output Control
    constexpr int SSR_RELAY = D0;    // GPIO16
    
    // Input Sensing
    constexpr int BUTTON_ADC = A0;   // Potentiometer / Button
}

namespace Timing {
    // Temperature Reading & PID Computation
    constexpr unsigned long TEMP_READ_INTERVAL_MS     = 1000UL;  // 1 second
    constexpr unsigned long PID_COMPUTE_WINDOW_MS     = 2000UL;  // 2 second window
    
    // Button Interaction
    constexpr unsigned long BUTTON_DEBOUNCE_MS        = 50UL;
    constexpr unsigned long BUTTON_LONG_PRESS_MS      = 2000UL;
    
    // State Persistence
    constexpr unsigned long STATE_SAVE_INTERVAL_MS    = 10000UL; // 10 seconds
    
    // SSR Rate Limiting
    constexpr unsigned long SSR_CHANGE_RATE_LIMIT_MS  = 1000UL;  // Min 1s between changes
    
    // Data Logger
    constexpr unsigned long DATA_LOG_INTERVAL_MS      = 1000UL;  // Log per second
    constexpr unsigned long DATA_CLEANUP_INTERVAL_MS  = 3600000UL; // Check hourly
    
    // WiFi
    constexpr unsigned long WIFI_CONNECTION_TIMEOUT   = 15000UL; // 15 seconds
}

namespace Safety {
    // Temperature Limits
    constexpr float ABSOLUTE_MAX_TEMPERATURE  = 1200.0f;  // Emergency cutoff °C
    constexpr float ABSOLUTE_MIN_TEMPERATURE  = -50.0f;   // Minimum valid reading
    
    // Sensor validity
    constexpr float SENSOR_MIN_VALID = -50.0f;
    constexpr float SENSOR_MAX_VALID = 1100.0f;
    
    // Default safe setpoint
    constexpr float DEFAULT_SETPOINT = 25.0f;
}

namespace Display {
    // SSD1306 Settings
    constexpr int SCREEN_WIDTH   = 128;
    constexpr int SCREEN_HEIGHT  = 64;
    constexpr int DISPLAY_ADDRESS = 0x3C;
}

namespace PID {
    // Default PID Tuning Parameters
    // Note: These can be overridden by saved values in /pid.json
    constexpr float DEFAULT_KP = 15.5f;   // Proportional
    constexpr float DEFAULT_KI = 13.1f;   // Integral
    constexpr float DEFAULT_KD = 1.20f;   // Derivative
    
    // Output limits (0-100% PWM equivalent)
    constexpr float OUTPUT_MIN = 0.0f;
    constexpr float OUTPUT_MAX = 100.0f;
    
    // Sample time (milliseconds)
    constexpr unsigned long SAMPLE_TIME_MS = 1000UL;
}

namespace Button {
    // Button ADC thresholds (0-1023 range)
    constexpr int ADC_THRESHOLD = 200;  // Consider pressed if below this
}

namespace FileSystem {
    // File Paths (LittleFS)
    const char* PROGRAMS_DIR                 = "/programs";
    const char* LAST_SELECTED_PROGRAM_FILE   = "/last_selected_program.txt";
    const char* KILN_STATE_FILE              = "/kiln_state.txt";
    const char* PID_PARAMS_FILE              = "/pid.json";
    const char* DATA_LOG_FILE                = "/kiln_data.json";
    const char* WIFI_CONFIG_FILE             = "/wifi_config.json";
}

namespace DataLogger {
    constexpr int MAX_DATA_POINTS        = 86400;      // 24 hours
    constexpr int DATA_RETENTION_HOURS   = 24;
    constexpr int DATA_RETENTION_SECONDS = 24 * 3600;
}

namespace WiFi {
    // AP Mode Configuration
    const char* AP_SSID     = "KilnController";
    const char* AP_PASSWORD = "kilnpass123";
    
    // Web Server
    constexpr int WEB_SERVER_PORT = 80;
    
    // OTA Update
    const char* OTA_HOSTNAME = "kilncontroller";
}

#endif // CONFIG_H
