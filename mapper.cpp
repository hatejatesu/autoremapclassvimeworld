#include <windows.h>
#include <jni.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <algorithm>

struct FieldMapping {
    std::string srgName;
    std::string mcpName;
    std::string owner;
    std::string descriptor;
};

struct MethodMapping {
    std::string srgName;
    std::string mcpName;
    std::string owner;
    std::string descriptor;
};

struct ClassMapping {
    std::string srgName;
    std::string mcpName;
};

JavaVM* jvm = nullptr;
std::map<std::string, ClassMapping> classMappings;
std::map<std::string, FieldMapping> fieldMappings;
std::map<std::string, MethodMapping> methodMappings;
std::ofstream logFile;

void Log(const std::string& message) {
    logFile.open("C:\\automapper_log.txt", std::ios::app);
    logFile << message << std::endl;
    logFile.close();
}

// Парсинг SRG файла
void ParseSrgFile(const std::string& filename) {
    std::ifstream file(filename);
    std::string line;
    
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        std::istringstream iss(line);
        std::string type;
        iss >> type;
        
        if (type == "CL:") { // Class mapping
            ClassMapping mapping;
            iss >> mapping.mcpName >> mapping.srgName;
            classMappings[mapping.srgName] = mapping;
            Log("Class map: " + mapping.srgName + " -> " + mapping.mcpName);
        }
        else if (type == "FD:") { // Field mapping
            FieldMapping mapping;
            std::string temp;
            iss >> temp;
            
            // Разделяем owner/field (формат: owner/field)
            size_t slashPos = temp.find('/');
            if (slashPos != std::string::npos) {
                mapping.owner = temp.substr(0, slashPos);
                mapping.srgName = temp.substr(slashPos + 1);
                iss >> mapping.mcpName;
                fieldMappings[mapping.owner + "." + mapping.srgName] = mapping;
            }
        }
        else if (type == "MD:") { // Method mapping
            MethodMapping mapping;
            std::string temp;
            iss >> temp;
            
            // Разделяем owner/method (формат: owner/method)
            size_t slashPos = temp.find('/');
            if (slashPos != std::string::npos) {
                mapping.owner = temp.substr(0, slashPos);
                mapping.srgName = temp.substr(slashPos + 1);
                iss >> mapping.descriptor >> mapping.mcpName;
                methodMappings[mapping.owner + "." + mapping.srgName + mapping.descriptor] = mapping;
            }
        }
    }
}

void LoadMappings() {
    Log("Loading MCP mappings...");
    
    // ✅ АБСОЛЮТНЫЙ путь к файлу маппингов
    std::string mappingPath = "C:\\Users\\hazel\\AutoMapper\\mappings\\joined.srg";
    Log("Looking for: " + mappingPath);
    
    // Проверяем существует ли файл
    std::ifstream testFile(mappingPath);
    if (!testFile.good()) {
        Log("ERROR: Mapping file not found!");
        testFile.close();
        return;
    }
    testFile.close();
    
    // Парсим основные маппинги
    ParseSrgFile(mappingPath);
    
    Log("Mappings loaded: " + std::to_string(classMappings.size()) + " classes, " +
        std::to_string(fieldMappings.size()) + " fields, " +
        std::to_string(methodMappings.size()) + " methods");
}

std::string GetMcpClassName(const std::string& srgName) {
    auto it = classMappings.find(srgName);
    if (it != classMappings.end()) {
        return it->second.mcpName;
    }
    return srgName;
}

std::string GetMcpFieldName(const std::string& owner, const std::string& srgName) {
    auto it = fieldMappings.find(owner + "." + srgName);
    if (it != fieldMappings.end()) {
        return it->second.mcpName;
    }
    return srgName;
}

std::string GetMcpMethodName(const std::string& owner, const std::string& srgName, const std::string& descriptor) {
    auto it = methodMappings.find(owner + "." + srgName + descriptor);
    if (it != methodMappings.end()) {
        return it->second.mcpName;
    }
    return srgName;
}

jclass FindClassRedirect(JNIEnv* env, const char* name) {
    std::string srgName(name);
    std::string mcpName = GetMcpClassName(srgName);
    
    if (srgName != mcpName) {
        Log("Class redirect: " + srgName + " -> " + mcpName);
    }
    
    return env->FindClass(mcpName.c_str());
}

void TestMappings(JNIEnv* env) {
    Log("=== Testing Mappings ===");
    
    // Тестируем класс EntityPlayerSP (ave)
    jclass entityPlayerClass = FindClassRedirect(env, "ave");
    if (entityPlayerClass != nullptr) {
        Log("SUCCESS: Found ave -> net/minecraft/client/entity/EntityPlayerSP");
        
        // Пробуем найти методы (пример: getPosX)
        // В SRG: MD: ave/s ()D getPosX
        jmethodID getPosX = env->GetMethodID(entityPlayerClass, "s", "()D");
        if (getPosX != nullptr) {
            Log("Found method: ave.s()D -> getPosX()");
        }
        
        // Пробуем найти поля
        jfieldID posXField = env->GetFieldID(entityPlayerClass, "a", "D");
        if (posXField != nullptr) {
            Log("Found field: ave.a -> field_70165_t (posX)");
        }
    }
    
    // Тестируем Minecraft класс (bhz)
    jclass minecraftClass = FindClassRedirect(env, "bhz");
    if (minecraftClass != nullptr) {
        Log("SUCCESS: Found bhz -> net/minecraft/client/Minecraft");
    }
}

void StartMapping(JNIEnv* env) {
    Log("=== VimeWorld AutoMapper Started ===");
    LoadMappings();
    TestMappings(env);
    Log("=== AutoMapper Ready ===");
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        Log("=== DLL Injected into Minecraft ===");
        
        jint result = JNI_GetCreatedJavaVMs(&jvm, 1, nullptr);
        if (result == JNI_OK && jvm != nullptr) {
            JNIEnv* env;
            jvm->AttachCurrentThread((void**)&env, nullptr);
            StartMapping(env);
            jvm->DetachCurrentThread();
        }
        break;
    }
    return TRUE;
}