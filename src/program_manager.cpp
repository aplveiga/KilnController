#include <program_manager.h>

ProgramManager programManager;

ProgramManager::ProgramManager() {
    ensureDirectoryExists();
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
