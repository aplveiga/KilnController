#ifndef PROGRAM_MANAGER_H
#define PROGRAM_MANAGER_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// Program segment structure
struct Segment {
    float rate_c_per_hour;  // °C/h
    float target_c;         // target temperature for this segment
    uint32_t hold_seconds;  // hold duration in seconds
};

// Program structure
struct Program {
    char name[64];          // program name
    Segment segments[9];    // max 9 segments
    uint8_t seqCount;       // number of segments
};

class ProgramManager {
public:
    ProgramManager();
    
    // Program file operations
    bool saveProgram(const Program& program);
    bool loadProgram(const char* name, Program& program);
    bool deleteProgram(const char* name);
    bool listPrograms(String& jsonList);
    
    // Get/Set last selected program
    const char* getLastSelectedProgram();
    bool setLastSelectedProgram(const char* name);
    
    // Load last selected program into memory
    bool loadLastSelectedProgram(Program& program);
    
private:
    const char* PROGRAMS_DIR = "/programs";
    const char* LAST_SELECTED_PROGRAM_FILE = "/last_selected_program.txt";
    
    // Helper functions
    String getProgramPath(const char* name);
    bool ensureDirectoryExists();
    bool programExists(const char* name);
    void createDefaultPrograms();
};

extern ProgramManager programManager;

#endif // PROGRAM_MANAGER_H
