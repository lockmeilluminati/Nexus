#include "nexus_environment.h"
#include "nexus_texture_extractor.h"
#include <fstream>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

SceneObject SpawnEnvironment(const std::string& filePath) {
    SceneObject env;
    env.name = "Environment";
    
    env.filePath = IngestAsset(filePath, "Envs");
    env.model = LoadModel(env.filePath.c_str());

    for (int i = 0; i < env.model.materialCount; i++) {
        env.model.materials[i].maps[MATERIAL_MAP_ALBEDO].color = WHITE;
    }
    
    // --- RELATIVE PATH RESOLUTION FIX ---
    fs::path pathObj(env.filePath);
    fs::path nxmatPath = pathObj.parent_path() / (pathObj.stem().string() + ".nxmat");
    
    if (fs::exists(nxmatPath)) {
        TraceLog(LOG_INFO, TextFormat("NXMAT: Loading material overrides from %s", nxmatPath.string().c_str()));
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
            else if (currentMatId >= 0 && currentMatId < env.model.materialCount) {
                auto parseMap = [&](const std::string& mapName, int mapType) {
                    if (line.find("\"" + mapName + "\":") != std::string::npos) {
                        size_t start = line.find('"', line.find(':')) + 1;
                        size_t end = line.find('"', start);
                        if (start != std::string::npos && end != std::string::npos) {
                            std::string texFileName = line.substr(start, end - start);
                            if (!texFileName.empty()) {
                                // Dynamically look inside the specific asset folder!
                                fs::path fullTexPath = pathObj.parent_path() / texFileName;
                                if (fs::exists(fullTexPath)) {
                                    Texture2D tex = LoadTexture(fullTexPath.string().c_str());
                                    env.model.materials[currentMatId].maps[mapType].texture = tex;
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
    
    env.position = { 0.0f, 0.0f, 0.0f }; 
    env.scale = 1.0f;
    env.isAnimated = false;
    env.anims = nullptr;
    env.bounds = GetModelBoundingBox(env.model);
    
    return env;
}