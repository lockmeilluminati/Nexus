#include "nexus_character.h"
#include "nexus_texture_extractor.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

// --- GLB LOADER WITH ABSOLUTE PATH RESOLUTION ---
Model LoadCharacterGLB(const std::string& filePath) {
    TraceLog(LOG_INFO, "NEXUS IMPORTER: Initializing GLB pipeline...");
    
    Model model = LoadModel(filePath.c_str());
    
    for (int i = 0; i < model.materialCount; i++) {
        model.materials[i].maps[MATERIAL_MAP_ALBEDO].color = WHITE;
    }

    fs::path pathObj(filePath);
    fs::path nxmatPath = pathObj.parent_path() / (pathObj.stem().string() + ".nxmat");
    
    if (fs::exists(nxmatPath)) {
        TraceLog(LOG_INFO, TextFormat("NXMAT: Loading character material overrides from %s", nxmatPath.string().c_str()));
        std::ifstream file(nxmatPath);
        std::string line;
        int currentMatId = -1;
        
        while (std::getline(file, line)) {
            if (line.find("\"id\":") != std::string::npos) {
                size_t colon = line.find(':');
                size_t comma = line.find(',');
                if (colon != std::string::npos && comma != std::string::npos) {
                    try {
                        currentMatId = std::stoi(line.substr(colon + 1, comma - colon - 1));
                    } catch (...) {}
                }
            }
            else if (currentMatId >= 0 && currentMatId < model.materialCount) {
                auto parseMap = [&](const std::string& mapName, int mapType) {
                    if (line.find("\"" + mapName + "\":") != std::string::npos) {
                        size_t start = line.find('"', line.find(':')) + 1;
                        size_t end = line.find('"', start);
                        if (start != std::string::npos && end != std::string::npos) {
                            std::string texFileName = line.substr(start, end - start);
                            if (!texFileName.empty()) {
                                // Prepend the folder path so it loads successfully
                                fs::path fullTexPath = pathObj.parent_path() / texFileName;
                                if (fs::exists(fullTexPath)) {
                                    Texture2D tex = LoadTexture(fullTexPath.string().c_str());
                                    model.materials[currentMatId].maps[mapType].texture = tex;
                                }
                            }
                        }
                    }
                };
                
                parseMap("albedo", MATERIAL_MAP_ALBEDO);
                parseMap("normal", MATERIAL_MAP_NORMAL);
                parseMap("metalness", MATERIAL_MAP_METALNESS);
            }
        }
        file.close();
    }

    return model;
}

Model LoadCharacterGLTF(const std::string& filePath) {
    TraceLog(LOG_INFO, "NEXUS IMPORTER: Initializing GLTF pipeline...");
    
    Model model = LoadModel(filePath.c_str());
    
    for (int i = 0; i < model.materialCount; i++) {
        model.materials[i].maps[MATERIAL_MAP_ALBEDO].color = WHITE;
    }

    fs::path pathObj(filePath);
    fs::path nxmatPath = pathObj.parent_path() / (pathObj.stem().string() + ".nxmat");
    
    if (fs::exists(nxmatPath)) {
        TraceLog(LOG_INFO, TextFormat("NXMAT: Loading character material overrides from %s", nxmatPath.string().c_str()));
        std::ifstream file(nxmatPath);
        std::string line;
        int currentMatId = -1;
        
        while (std::getline(file, line)) {
            if (line.find("\"id\":") != std::string::npos) {
                size_t colon = line.find(':');
                size_t comma = line.find(',');
                if (colon != std::string::npos && comma != std::string::npos) {
                    try {
                        currentMatId = std::stoi(line.substr(colon + 1, comma - colon - 1));
                    } catch (...) {}
                }
            }
            else if (currentMatId >= 0 && currentMatId < model.materialCount) {
                auto parseMap = [&](const std::string& mapName, int mapType) {
                    if (line.find("\"" + mapName + "\":") != std::string::npos) {
                        size_t start = line.find('"', line.find(':')) + 1;
                        size_t end = line.find('"', start);
                        if (start != std::string::npos && end != std::string::npos) {
                            std::string texFileName = line.substr(start, end - start);
                            if (!texFileName.empty()) {
                                // Prepend the folder path so it loads successfully
                                fs::path fullTexPath = pathObj.parent_path() / texFileName;
                                if (fs::exists(fullTexPath)) {
                                    Texture2D tex = LoadTexture(fullTexPath.string().c_str());
                                    model.materials[currentMatId].maps[mapType].texture = tex;
                                }
                            }
                        }
                    }
                };
                
                parseMap("albedo", MATERIAL_MAP_ALBEDO);
                parseMap("normal", MATERIAL_MAP_NORMAL);
                parseMap("metalness", MATERIAL_MAP_METALNESS);
            }
        }
        file.close();
    }

    return model;
}

SceneObject SpawnCharacter(const std::string& filePath) {
    SceneObject character;
    character.name = "Character";
    character.filePath = IngestAsset(filePath, "Chars");

    std::string ext = character.filePath.substr(character.filePath.find_last_of("."));
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".glb") {
        character.model = LoadCharacterGLB(character.filePath);
    } else {
        character.model = LoadCharacterGLTF(character.filePath);
    }

    int animCount = 0;
    character.anims = LoadModelAnimations(character.filePath.c_str(), &animCount);
    
    if (animCount > 0) {
        character.animCount = animCount;
        character.currentFrame = 60; 
        character.isAnimated = true;
    } else {
        character.isAnimated = false;
        character.anims = nullptr;
    }
    
    character.position = { 0.0f, 0.0f, 0.0f };
    character.scale = 1.0f;
    
    if (character.isAnimated) {
        character.bounds = { {-1.0f, 0.0f, -1.0f}, {1.0f, 2.5f, 1.0f} };
        TraceLog(LOG_INFO, "NEXUS: Applied Safe Bounds to skeletal character.");
    } else {
        character.bounds = GetModelBoundingBox(character.model);
    }
    
    character.waypoints.clear();
    character.targetWaypointIndex = 0;
    character.walkSpeed = 3.0f;
    character.loopWaypoints = false; 
    
    return character;
}