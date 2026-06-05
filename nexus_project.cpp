#include "nexus_project.h"
#include "nexus_character.h"
#include "nexus_prefab.h" 
#include <fstream>
#include <sstream>
#include <algorithm>

void SaveNexusProject(const std::string& savePath, const std::vector<SceneObject>& scene, const std::vector<Vector3>& sceneRotations, const NexusTimeline& timeline) {
    std::ofstream file(savePath);
    if (!file.is_open()) return;

    file << "[NexusProject]\n";
    for (size_t i = 0; i < scene.size(); i++) {
        file << "Object=" << scene[i].filePath << "\n";
        file << "Pos=" << scene[i].position.x << "," << scene[i].position.y << "," << scene[i].position.z << "\n";
        file << "Rot=" << sceneRotations[i].x << "," << sceneRotations[i].y << "," << sceneRotations[i].z << "\n";
        file << "Scale=" << scene[i].scale << "\n";
        file << "WalkSpeed=" << scene[i].walkSpeed << "\n";
        file << "LoopWaypoints=" << (scene[i].loopWaypoints ? "1" : "0") << "\n";

        // --- NEW: PROJECT LEVEL SERIALIZATION FOR WAYPOINTS ---
        for (const auto& wp : scene[i].waypoints) {
            file << "Waypoint=" << wp.x << "," << wp.y << "," << wp.z << "\n";
        }
        
        // --- NEW: PROJECT LEVEL SERIALIZATION FOR PARASITES ---
        for (const auto& path : scene[i].parasitePaths) {
            file << "Parasite=" << path << "\n";
        }
        
        // --- NEW: PROJECT LEVEL SERIALIZATION FOR MONTAGES & OFFSETS ---
        for (const auto& m : scene[i].montage) {
            file << "Montage=" << m.sourcePath << "," << m.animIndex << "," << m.animName << "," 
                 << m.startTime << "," << m.duration << "," << m.blendInTime << ","
                 << m.positionOffset.x << "," << m.positionOffset.y << "," << m.positionOffset.z << ","
                 << m.rotationOffset.x << "," << m.rotationOffset.y << "," << m.rotationOffset.z << ","
                 << m.scaleOffset << "\n";
        }
        
        if (i < timeline.tracks.size()) {
            file << "Track=" << timeline.tracks[i].startFrame << "," << timeline.tracks[i].endFrame << "," << (timeline.tracks[i].stayOnScene ? "1" : "0") << "\n";
        }
    }
    file.close();
}

void LoadNexusProject(const std::string& loadPath, std::vector<SceneObject>& scene, std::vector<Vector3>& sceneRotations, NexusTimeline& timeline) {
    scene.clear();
    sceneRotations.clear();
    timeline.tracks.clear();

    std::ifstream file(loadPath);
    std::string line;
    
    SceneObject currentObj;
    Vector3 currentRot = {0};
    TimelineTrack currentTrack;
    bool hasTrack = false;
    std::vector<std::string> loadedParasites;
    bool isFirstObject = true;
    
    bool clearedWaypoints = false;
    bool clearedMontage = false;

    // Helper lambda to safely push the completely built object to memory
    auto finalizeObject = [&]() {
        if (currentObj.model.meshCount > 0) {
            for (const auto& path : loadedParasites) {
                bool exists = false;
                for(auto& p : currentObj.parasitePaths) { if(p == path) { exists = true; break; } }
                if (!exists) {
                    int count = 0;
                    ModelAnimation* anims = LoadModelAnimations(path.c_str(), &count);
                    if (count > 0) {
                        currentObj.parasitePaths.push_back(path);
                        currentObj.parasiteAnims.push_back(anims);
                        currentObj.parasiteAnimCounts.push_back(count);
                    }
                }
            }
            scene.push_back(currentObj);
            sceneRotations.push_back(currentRot);
            if (hasTrack) timeline.tracks.push_back(currentTrack);
        }
    };

    if (file.is_open()) {
        while (std::getline(file, line)) {
            if (line.find("Object=") == 0) {
                if (!isFirstObject) finalizeObject();
                isFirstObject = false;

                hasTrack = false;
                loadedParasites.clear();
                clearedWaypoints = false;
                clearedMontage = false;
                currentRot = {0, 0, 0};

                std::string path = line.substr(7);
                std::string ext = path.substr(path.find_last_of("."));
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                
                if (ext == ".nxchar") currentObj = LoadNexusCharacterPrefab(path);
                else currentObj = SpawnCharacter(path);
            }
            else if (line.find("Pos=") == 0) {
                std::stringstream ss(line.substr(4)); std::string token;
                std::getline(ss, token, ','); currentObj.position.x = std::stof(token);
                std::getline(ss, token, ','); currentObj.position.y = std::stof(token);
                std::getline(ss, token, ','); currentObj.position.z = std::stof(token);
            }
            else if (line.find("Rot=") == 0) {
                std::stringstream ss(line.substr(4)); std::string token;
                std::getline(ss, token, ','); currentRot.x = std::stof(token);
                std::getline(ss, token, ','); currentRot.y = std::stof(token);
                std::getline(ss, token, ','); currentRot.z = std::stof(token);
            }
            else if (line.find("Scale=") == 0) {
                currentObj.scale = std::stof(line.substr(6));
            }
            else if (line.find("WalkSpeed=") == 0) {
                currentObj.walkSpeed = std::stof(line.substr(10));
            }
            else if (line.find("LoopWaypoints=") == 0) {
                currentObj.loopWaypoints = (line.substr(14) == "1");
            }
            else if (line.find("Waypoint=") == 0) {
                if (!clearedWaypoints) { currentObj.waypoints.clear(); clearedWaypoints = true; }
                std::string coords = line.substr(9);
                std::stringstream ss(coords); std::string token;
                float vals[3] = {0}; int i = 0;
                while (std::getline(ss, token, ',') && i < 3) vals[i++] = std::stof(token);
                currentObj.waypoints.push_back({vals[0], vals[1], vals[2]});
            }
            else if (line.find("Parasite=") == 0) {
                loadedParasites.push_back(line.substr(9));
            }
            else if (line.find("Montage=") == 0) {
                if (!clearedMontage) { currentObj.montage.clear(); clearedMontage = true; }
                std::string data = line.substr(8);
                std::stringstream ss(data); std::string token;
                MontageBlock mb;
                
                std::getline(ss, token, ','); mb.sourcePath = token;
                std::getline(ss, token, ','); mb.animIndex = std::stoi(token);
                std::getline(ss, token, ','); mb.animName = token;
                std::getline(ss, token, ','); mb.startTime = std::stof(token);
                std::getline(ss, token, ','); mb.duration = std::stof(token);
                std::getline(ss, token, ','); mb.blendInTime = std::stof(token);
                
                if (std::getline(ss, token, ',')) {
                    mb.positionOffset.x = std::stof(token);
                    std::getline(ss, token, ','); mb.positionOffset.y = std::stof(token);
                    std::getline(ss, token, ','); mb.positionOffset.z = std::stof(token);
                    std::getline(ss, token, ','); mb.rotationOffset.x = std::stof(token);
                    std::getline(ss, token, ','); mb.rotationOffset.y = std::stof(token);
                    std::getline(ss, token, ','); mb.rotationOffset.z = std::stof(token);
                    std::getline(ss, token, ','); mb.scaleOffset = std::stof(token);
                } else {
                    mb.positionOffset = {0,0,0}; mb.rotationOffset = {0,0,0}; mb.scaleOffset = 1.0f;
                }
                currentObj.montage.push_back(mb);
            }
            else if (line.find("Track=") == 0) {
                std::stringstream ss(line.substr(6)); std::string token;
                std::getline(ss, token, ','); currentTrack.startFrame = std::stoi(token);
                std::getline(ss, token, ','); currentTrack.endFrame = std::stoi(token);
                std::getline(ss, token, ','); currentTrack.stayOnScene = (token == "1");
                
                std::string trackName = currentObj.filePath.substr(currentObj.filePath.find_last_of("/\\") + 1);
                currentTrack.assetName = trackName.substr(0, trackName.find_last_of('.'));
                currentTrack.currentAnimIndex = 0;
                
                int frames = 0;
                if (!currentObj.montage.empty()) {
                    float maxTime = 0.0f;
                    for (auto& b : currentObj.montage) { if (b.startTime + b.duration > maxTime) maxTime = b.startTime + b.duration; }
                    frames = (int)(maxTime * 60.0f);
                } else if (currentObj.isAnimated && currentObj.anims != nullptr) {
                    frames = currentObj.anims[0].keyframeCount;
                }
                currentTrack.sourceDuration = frames;
                currentTrack.isEnvironment = (currentObj.name == "Environment");
                hasTrack = true;
            }
        }
        if (!isFirstObject) finalizeObject();
        file.close();
    }
}