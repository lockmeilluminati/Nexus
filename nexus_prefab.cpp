#include "nexus_prefab.h"
#include "nexus_character.h"
#include <fstream>
#include <sstream>

void SaveNexusCharacterPrefab(const SceneObject& obj, const std::string& savePath) {
    std::ofstream file(savePath);
    if (!file.is_open()) return;

    file << "[NexusCharacter]\n";
    file << "BaseMesh=" << obj.filePath << "\n";
    file << "WalkSpeed=" << obj.walkSpeed << "\n";
    file << "LoopWaypoints=" << (obj.loopWaypoints ? "1" : "0") << "\n";

    for (const auto& wp : obj.waypoints) {
        file << "Waypoint=" << wp.x << "," << wp.y << "," << wp.z << "\n";
    }

    for (const auto& path : obj.parasitePaths) {
        file << "Parasite=" << path << "\n";
    }

    // --- NEW: SAVE OFFSETS ---
    for (const auto& m : obj.montage) {
        file << "Montage=" << m.sourcePath << "," << m.animIndex << "," << m.animName << "," 
             << m.startTime << "," << m.duration << "," << m.blendInTime << ","
             << m.positionOffset.x << "," << m.positionOffset.y << "," << m.positionOffset.z << ","
             << m.rotationOffset.x << "," << m.rotationOffset.y << "," << m.rotationOffset.z << ","
             << m.scaleOffset << "\n";
    }

    file.close();
}

SceneObject LoadNexusCharacterPrefab(const std::string& loadPath) {
    std::ifstream file(loadPath);
    std::string line;
    std::string baseMeshPath = "";
    float walkSpeed = 3.0f;
    bool loopWaypoints = false;
    std::vector<Vector3> waypoints;
    std::vector<std::string> loadedParasites;
    std::vector<MontageBlock> montage; 

    if (file.is_open()) {
        while (std::getline(file, line)) {
            if (line.find("BaseMesh=") == 0) baseMeshPath = line.substr(9);
            else if (line.find("WalkSpeed=") == 0) walkSpeed = std::stof(line.substr(10));
            else if (line.find("LoopWaypoints=") == 0) loopWaypoints = (line.substr(14) == "1");
            else if (line.find("Waypoint=") == 0) {
                std::string coords = line.substr(9);
                std::stringstream ss(coords);
                std::string token;
                float vals[3] = {0};
                int i = 0;
                while (std::getline(ss, token, ',') && i < 3) vals[i++] = std::stof(token);
                waypoints.push_back({vals[0], vals[1], vals[2]});
            }
            else if (line.find("Parasite=") == 0) {
                loadedParasites.push_back(line.substr(9));
            }
            else if (line.find("Montage=") == 0) {
                std::string data = line.substr(8);
                std::stringstream ss(data);
                std::string token;
                MontageBlock mb;
                
                std::getline(ss, token, ','); mb.sourcePath = token;
                std::getline(ss, token, ','); mb.animIndex = std::stoi(token);
                std::getline(ss, token, ','); mb.animName = token;
                std::getline(ss, token, ','); mb.startTime = std::stof(token);
                std::getline(ss, token, ','); mb.duration = std::stof(token);
                std::getline(ss, token, ','); mb.blendInTime = std::stof(token);
                
                // --- NEW: BACKWARD COMPATIBLE PARSING ---
                if (std::getline(ss, token, ',')) {
                    mb.positionOffset.x = std::stof(token);
                    std::getline(ss, token, ','); mb.positionOffset.y = std::stof(token);
                    std::getline(ss, token, ','); mb.positionOffset.z = std::stof(token);
                    std::getline(ss, token, ','); mb.rotationOffset.x = std::stof(token);
                    std::getline(ss, token, ','); mb.rotationOffset.y = std::stof(token);
                    std::getline(ss, token, ','); mb.rotationOffset.z = std::stof(token);
                    std::getline(ss, token, ','); mb.scaleOffset = std::stof(token);
                } else {
                    // Fallback if loading an older file
                    mb.positionOffset = {0.0f, 0.0f, 0.0f}; 
                    mb.rotationOffset = {0.0f, 0.0f, 0.0f}; 
                    mb.scaleOffset = 1.0f;
                }
                
                montage.push_back(mb);
            }
        }
        file.close();
    }

    SceneObject obj = SpawnCharacter(baseMeshPath);
    obj.walkSpeed = walkSpeed;
    obj.loopWaypoints = loopWaypoints;
    obj.waypoints = waypoints;
    obj.montage = montage; 

    for (const auto& path : loadedParasites) {
        int count = 0;
        ModelAnimation* anims = LoadModelAnimations(path.c_str(), &count);
        if (count > 0) {
            obj.parasitePaths.push_back(path);
            obj.parasiteAnims.push_back(anims);
            obj.parasiteAnimCounts.push_back(count);
        }
    }

    std::string name = loadPath.substr(loadPath.find_last_of("/\\") + 1);
    obj.name = name.substr(0, name.find_last_of('.'));

    return obj;
}