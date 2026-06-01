#include "nexus_project.h"
#include "nexus_environment.h" // Needed to spawn the objects back in
#include "nexus_character.h"
#include <fstream>
#include <sstream>

void SaveNexusProject(const std::string& filename, const std::vector<SceneObject>& scene, const std::vector<Vector3>& rotations, const NexusTimeline& timeline) {
    std::ofstream file(filename);
    if (!file.is_open()) return;

    file << "NEXUS_PROJECT_V1\n";
    file << "ASSET_COUNT " << scene.size() << "\n";

    for (size_t i = 0; i < scene.size(); i++) {
        file << "ASSET_START\n";
        file << "NAME " << timeline.tracks[i].assetName << "\n";
        file << "PATH " << scene[i].filePath << "\n";
        file << "IS_ENV " << timeline.tracks[i].isEnvironment << "\n";
        file << "POS " << scene[i].position.x << " " << scene[i].position.y << " " << scene[i].position.z << "\n";
        file << "ROT " << rotations[i].x << " " << rotations[i].y << " " << rotations[i].z << "\n";
        file << "SCALE " << scene[i].scale << "\n";
        file << "TRACK_START " << timeline.tracks[i].startFrame << "\n";
        file << "TRACK_END " << timeline.tracks[i].endFrame << "\n";
        file << "ASSET_END\n";
    }
    file.close();
}

// NEW: The Deserializer Engine
void LoadNexusProject(const std::string& filename, std::vector<SceneObject>& scene, std::vector<Vector3>& rotations, NexusTimeline& timeline) {
    std::ifstream file(filename);
    if (!file.is_open()) return;

    // 1. Wipe the current grid clean
    for (auto& obj : scene) {
        UnloadModel(obj.model);
        if (obj.isAnimated && obj.anims != nullptr) UnloadModelAnimations(obj.anims, obj.animCount);
    }
    scene.clear();
    rotations.clear();
    timeline.tracks.clear();

    // 2. Parse the file and rebuild
    std::string line;
    SceneObject tempObj;
    std::string tempName, tempPath;
    bool tempIsEnv = false;
    Vector3 tempPos = {0}, tempRot = {0};
    float tempScale = 1.0f;
    int tStart = 0, tEnd = 300;

    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string token;
        iss >> token;

        if (token == "NAME") { std::getline(iss >> std::ws, tempName); }
        else if (token == "PATH") { std::getline(iss >> std::ws, tempPath); }
        else if (token == "IS_ENV") { iss >> tempIsEnv; }
        else if (token == "POS") { iss >> tempPos.x >> tempPos.y >> tempPos.z; }
        else if (token == "ROT") { iss >> tempRot.x >> tempRot.y >> tempRot.z; }
        else if (token == "SCALE") { iss >> tempScale; }
        else if (token == "TRACK_START") { iss >> tStart; }
        else if (token == "TRACK_END") { iss >> tEnd; }
        else if (token == "ASSET_END") {
            // Physically spawn the model back into Raylib
            if (tempIsEnv) tempObj = SpawnEnvironment(tempPath);
            else tempObj = SpawnCharacter(tempPath);

            tempObj.position = tempPos;
            tempObj.scale = tempScale;
            scene.push_back(tempObj);
            rotations.push_back(tempRot);

            int frames = 0;
            if (tempObj.isAnimated && tempObj.anims != nullptr) frames = tempObj.anims[0].keyframeCount;
            timeline.AddTrack(tempName, frames, tempIsEnv);
            timeline.tracks.back().startFrame = tStart;
            timeline.tracks.back().endFrame = tEnd;
        }
    }
    file.close();
}