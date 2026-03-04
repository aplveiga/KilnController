#include <program_manager.h>

ProgramManager programManager;

ProgramManager::ProgramManager() {
    ensureDirectoryExists();
    createDefaultPrograms();
}

bool ProgramManager::ensureDirectoryExists() {
    if (!LittleFS.exists(PROGRAMS_DIR)) {
        return LittleFS.mkdir(PROGRAMS_DIR);
    }
    return true;
}

String ProgramManager::getProgramPath(const char* name) {
    String path = String(PROGRAMS_DIR) + "/" + String(name) + ".json";
    return path;
}

bool ProgramManager::programExists(const char* name) {
    String path = getProgramPath(name);
    return LittleFS.exists(path);
}

bool ProgramManager::saveProgram(const Program& program) {
    if (!ensureDirectoryExists()) {
        Serial.println("[ProgramMgr] Failed to create programs directory");
        return false;
    }
    
    String path = getProgramPath(program.name);
    
    // Create JSON document on heap to avoid stack overflow
    // Need 1024 bytes for programs with up to 9 segments
    DynamicJsonDocument doc(1024);
    doc["name"] = program.name;
    doc["seqCount"] = program.seqCount;
    
    // Add segments array
    JsonArray segments = doc.createNestedArray("segments");
    for (uint8_t i = 0; i < program.seqCount; i++) {
        JsonObject seg = segments.createNestedObject();
        seg["rate_c_per_hour"] = program.segments[i].rate_c_per_hour;
        seg["target_c"] = program.segments[i].target_c;
        seg["hold_seconds"] = program.segments[i].hold_seconds;
    }
    
    // Write to file
    File file = LittleFS.open(path, "w");
    if (!file) {
        Serial.printf("[ProgramMgr] Failed to open file for writing: %s\n", path.c_str());
        return false;
    }
    
    // Feed watchdog during file write
    yield();
    if (serializeJson(doc, file) == 0) {
        Serial.printf("[ProgramMgr] Failed to serialize program: %s\n", program.name);
        file.close();
        return false;
    }
    
    // Feed watchdog before closing
    yield();
    file.close();
    yield();
    
    Serial.printf("[ProgramMgr] Program saved: %s\n", program.name);
    return true;
}

bool ProgramManager::loadProgram(const char* name, Program& program) {
    String path = getProgramPath(name);
    
    if (!LittleFS.exists(path)) {
        Serial.printf("[ProgramMgr] Program file not found: %s\n", path.c_str());
        return false;
    }
    
    File file = LittleFS.open(path, "r");
    if (!file) {
        Serial.printf("[ProgramMgr] Failed to open program file: %s\n", path.c_str());
        return false;
    }
    
    // Use DynamicJsonDocument on heap to avoid stack overflow
    // Need 1024 bytes for programs with up to 9 segments
    DynamicJsonDocument doc(1024);
    yield();  // Feed watchdog before parsing
    
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    yield();  // Feed watchdog after file operations
    
    if (error) {
        Serial.printf("[ProgramMgr] JSON parse error: %s\n", error.c_str());
        return false;
    }
    
    // Parse program data
    strncpy(program.name, doc["name"] | "Unknown", sizeof(program.name) - 1);
    program.name[sizeof(program.name) - 1] = '\0';
    program.seqCount = doc["seqCount"] | 0;
    
    if (program.seqCount > 9) {
        program.seqCount = 9;  // Limit to 9 segments
    }
    
    // Parse segments
    JsonArray segments = doc["segments"];
    if (!segments.isNull()) {
        for (uint8_t i = 0; i < program.seqCount && i < 9; i++) {
            program.segments[i].rate_c_per_hour = segments[i]["rate_c_per_hour"] | 0.0;
            program.segments[i].target_c = segments[i]["target_c"] | 0.0;
            program.segments[i].hold_seconds = segments[i]["hold_seconds"] | 0UL;
        }
    }
    
    yield();  // Final yield before returning
    Serial.printf("[ProgramMgr] Program loaded: %s\n", name);
    return true;
}

bool ProgramManager::deleteProgram(const char* name) {
    String path = getProgramPath(name);
    
    if (!LittleFS.exists(path)) {
        Serial.printf("[ProgramMgr] Program file not found: %s\n", path.c_str());
        return false;
    }
    
    if (LittleFS.remove(path)) {
        Serial.printf("[ProgramMgr] Program deleted: %s\n", name);
        return true;
    }
    
    Serial.printf("[ProgramMgr] Failed to delete program: %s\n", name);
    return false;
}

bool ProgramManager::listPrograms(String& jsonList) {
    DynamicJsonDocument doc(2048);  // Use DynamicJsonDocument to save stack space
    JsonArray programs = doc.createNestedArray("programs");
    
    Dir dir = LittleFS.openDir(PROGRAMS_DIR);
    while (dir.next()) {
        yield();  // Feed watchdog during directory iteration
        
        String filename = dir.fileName();
        if (filename.endsWith(".json")) {
            // Remove .json extension
            String programName = filename.substring(0, filename.length() - 5);
            
            JsonObject prog = programs.createNestedObject();
            prog["name"] = programName;
            
            // Load program to get segment count
            Program program;
            if (loadProgram(programName.c_str(), program)) {
                prog["seqCount"] = program.seqCount;
            }
        }
    }
    
    yield();
    serializeJson(doc, jsonList);
    yield();
    return true;
}

const char* ProgramManager::getLastSelectedProgram() {
    static char lastSelectedName[64] = "";
    
    if (LittleFS.exists(LAST_SELECTED_PROGRAM_FILE)) {
        File file = LittleFS.open(LAST_SELECTED_PROGRAM_FILE, "r");
        if (file && file.available()) {
            size_t size = file.size();
            if (size < sizeof(lastSelectedName)) {
                file.readBytes(lastSelectedName, size);
                lastSelectedName[size] = '\0';
                file.close();
                yield();
                return lastSelectedName;
            }
        }
    }
    
    return "9-step";
}

bool ProgramManager::setLastSelectedProgram(const char* name) {
    File file = LittleFS.open(LAST_SELECTED_PROGRAM_FILE, "w");
    if (!file) {
        Serial.println("[ProgramMgr] Failed to create last selected program file");
        return false;
    }
    
    yield();
    if (file.write((const uint8_t*)name, strlen(name)) == strlen(name)) {
        file.close();
        yield();
        Serial.printf("[ProgramMgr] Last selected program set to: %s\n", name);
        return true;
    }
    
    file.close();
    yield();
    Serial.println("[ProgramMgr] Failed to write last selected program");
    return false;
}

bool ProgramManager::loadLastSelectedProgram(Program& program) {
    const char* lastSelectedName = getLastSelectedProgram();
    return loadProgram(lastSelectedName, program);
}

void ProgramManager::createDefaultPrograms() {
    // 4-Step Program
    if (!programExists("4-step")) {
        Program prog4;
        strncpy(prog4.name, "4-step", sizeof(prog4.name) - 1);
        prog4.name[sizeof(prog4.name) - 1] = '\0';
        prog4.seqCount = 4;

        // Segment 1: Ramp to 200°C at 50°C/h, hold 30 min
        prog4.segments[0].rate_c_per_hour = 50.0;
        prog4.segments[0].target_c = 200.0;
        prog4.segments[0].hold_seconds = 1800;  // 30 minutes

        // Segment 2: Ramp to 500°C at 100°C/h, hold 30 min
        prog4.segments[1].rate_c_per_hour = 100.0;
        prog4.segments[1].target_c = 500.0;
        prog4.segments[1].hold_seconds = 1800;

        // Segment 3: Ramp to 800°C at 50°C/h, hold 1 hour
        prog4.segments[2].rate_c_per_hour = 50.0;
        prog4.segments[2].target_c = 800.0;
        prog4.segments[2].hold_seconds = 3600;

        // Segment 4: Ramp to 1000°C at 20°C/h, hold 2 hours
        prog4.segments[3].rate_c_per_hour = 20.0;
        prog4.segments[3].target_c = 1000.0;
        prog4.segments[3].hold_seconds = 7200;

        saveProgram(prog4);
        Serial.println("[ProgramMgr] Default program created: 4-step");
    }

    // 9-Step Program
    if (!programExists("9-step")) {
        Program prog9;
        strncpy(prog9.name, "9-step", sizeof(prog9.name) - 1);
        prog9.name[sizeof(prog9.name) - 1] = '\0';
        prog9.seqCount = 9;

        // Segment 1: Ramp to 150°C at 30°C/h, hold 15 min
        prog9.segments[0].rate_c_per_hour = 30.0;
        prog9.segments[0].target_c = 150.0;
        prog9.segments[0].hold_seconds = 900;

        // Segment 2: Ramp to 300°C at 50°C/h, hold 20 min
        prog9.segments[1].rate_c_per_hour = 50.0;
        prog9.segments[1].target_c = 300.0;
        prog9.segments[1].hold_seconds = 1200;

        // Segment 3: Ramp to 450°C at 75°C/h, hold 20 min
        prog9.segments[2].rate_c_per_hour = 75.0;
        prog9.segments[2].target_c = 450.0;
        prog9.segments[2].hold_seconds = 1200;

        // Segment 4: Ramp to 600°C at 100°C/h, hold 30 min
        prog9.segments[3].rate_c_per_hour = 100.0;
        prog9.segments[3].target_c = 600.0;
        prog9.segments[3].hold_seconds = 1800;

        // Segment 5: Ramp to 700°C at 75°C/h, hold 30 min
        prog9.segments[4].rate_c_per_hour = 75.0;
        prog9.segments[4].target_c = 700.0;
        prog9.segments[4].hold_seconds = 1800;

        // Segment 6: Ramp to 850°C at 50°C/h, hold 45 min
        prog9.segments[5].rate_c_per_hour = 50.0;
        prog9.segments[5].target_c = 850.0;
        prog9.segments[5].hold_seconds = 2700;

        // Segment 7: Ramp to 950°C at 30°C/h, hold 1 hour
        prog9.segments[6].rate_c_per_hour = 30.0;
        prog9.segments[6].target_c = 950.0;
        prog9.segments[6].hold_seconds = 3600;

        // Segment 8: Ramp to 1050°C at 20°C/h, hold 2 hours
        prog9.segments[7].rate_c_per_hour = 20.0;
        prog9.segments[7].target_c = 1050.0;
        prog9.segments[7].hold_seconds = 7200;

        // Segment 9: Cooldown at 10°C/h to 500°C, hold 30 min
        prog9.segments[8].rate_c_per_hour = 10.0;
        prog9.segments[8].target_c = 500.0;
        prog9.segments[8].hold_seconds = 1800;

        saveProgram(prog9);
        Serial.println("[ProgramMgr] Default program created: 9-step");
    }
}
