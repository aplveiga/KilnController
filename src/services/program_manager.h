// ============================================================================
// Service Layer - Program Manager Interface (Refactored)
// ============================================================================
// Manages program storage and retrieval
// Provides: save, load, delete, list programs
// ============================================================================

#ifndef PROGRAM_MANAGER_H
#define PROGRAM_MANAGER_H

#include <Arduino.h>
#include "../config/config.h"
#include "../config/types.h"

class ProgramManager {
public:
    ProgramManager();
    
    // Lifecycle
    void init();                    // Ensure storage ready
    void shutdown();
    
    // Program Operations
    bool saveProgram(const Program& program);       // Save program to flash
    bool loadProgram(const String& name, Program& program);  // Load program
    bool deleteProgram(const String& name);         // Delete program
    bool renameProgram(const String& old_name, const String& new_name);
    
    // Listing
    struct ProgramInfo {
        char name[64];
        uint8_t segment_count;
        uint32_t file_size_bytes;
    };
    
    // Returns array of programs via string (for web JSON response)
    bool listPrograms(String& jsonList);                 // Get all programs as JSON
    int getProgramCount() const;
    
    // Last Selected
    const char* getLastSelectedProgram();           // Get last used
    bool setLastSelectedProgram(const String& name);  // Remember selection
    bool loadLastSelected(Program& program);        // Load last used
    
    // Validation
    bool validate(const Program& program);          // Check program validity
    bool programExists(const String& name);
    
    // Defaults
    void createDefaultPrograms();   // Create factory defaults
    void resetToDefaults();         // Restore factory programs
    
    // Status
    bool isInitialized() const;
    String getStatusJSON() const;

private:
    bool initialized_;
    char last_selected_[64];
    
    // File operations
    String getProgramPath(const String& name);
    bool ensureDirectoryExists();
    bool readProgramJSON(const String& name, Program& program);
    bool writeProgramJSON(const Program& program);
    
    // Helpers
    bool isValidProgramName(const String& name) const;
};

#endif // PROGRAM_MANAGER_H
