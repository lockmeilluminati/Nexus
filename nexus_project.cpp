#include "nexus_project.h"
#include "nexus_timeline.h"
#include "nexus_character.h"
#include "nexus_prefab.h" 
#include <fstream>
#include <sstream>
#include <algorithm>

void SaveNexusProject(const std::string& savePath, const std::vector<SceneObject>& scene, const std::vector<Vector3>& sceneRotations, const NexusTimeline& timeline, const NexusCameraTrack& activeCamTrack, bool playerInit, bool useMath, int playerTargetIdx) {
    std::ofstream file(savePath);
    if (!file.is_open()) return;

    file << "[NexusProject]\n";
    
    // SAVE LIVE PLAYER AND ACTIVE CAMERA STATE
    file << "PlayerInit=" << (playerInit ? "1" : "0") << "\n";
    file << "UseMath=" << (useMath ? "1" : "0") << "\n";
    file << "PlayerIdx=" << playerTargetIdx << "\n";

    file << "ActiveCamActive=" << (activeCamTrack.isActive ? "1" : "0") << "\n";
    file << "ActiveCamSpeed=" << activeCamTrack.rig.playbackSpeed << "\n";
    for (const auto& wp : activeCamTrack.waypoints) {
        file << "ActiveCamWP=" << wp.transitTime << "," << wp.position.x << "," << wp.position.y << "," << wp.position.z << ","
             << wp.target.x << "," << wp.target.y << "," << wp.target.z << "," << wp.fov << "," << wp.holdTime << "," << wp.label << "\n";
    }

    for (size_t i = 0; i < scene.size(); i++) {
        file << "Object=" << scene[i].filePath << "\n";
        file << "Pos=" << scene[i].position.x << "," << scene[i].position.y << "," << scene[i].position.z << "\n";
        file << "Rot=" << sceneRotations[i].x << "," << sceneRotations[i].y << "," << sceneRotations[i].z << "\n";
        file << "Scale=" << scene[i].scale << "\n";
        file << "WalkSpeed=" << scene[i].walkSpeed << "\n";
        file << "LoopWaypoints=" << (scene[i].loopWaypoints ? "1" : "0") << "\n";

        // SAVE EMITTER DATA
        file << "AudioMode=" << (int)scene[i].audioEmitter.playbackMode << "\n";
        file << "AudioTrigger=" << (int)scene[i].audioEmitter.triggerType << "\n";
        file << "AudioRadius=" << scene[i].audioEmitter.triggerRadius << "\n";
        file << "AudioCooldown=" << scene[i].audioEmitter.cooldownTime << "\n";
        file << "AudioKey=" << scene[i].audioEmitter.interactKey << "\n"; 
        for (const auto& line : scene[i].audioEmitter.playlist) {
            file << "AudioLine=" << line.filePath << "\n";
        }

        for (const auto& wp : scene[i].waypoints) {
            file << "Waypoint=" << wp.x << "," << wp.y << "," << wp.z << "\n";
        }
        for (const auto& path : scene[i].parasitePaths) {
            file << "Parasite=" << path << "\n";
        }
        for (const auto& m : scene[i].montage) {
            file << "Montage=" << m.sourcePath << "," << m.animIndex << "," << m.animName << "," 
                 << m.startTime << "," << m.duration << "," << m.blendInTime << ","
                 << m.positionOffset.x << "," << m.positionOffset.y << "," << m.positionOffset.z << ","
                 << m.rotationOffset.x << "," << m.rotationOffset.y << "," << m.rotationOffset.z << ","
                 << m.scaleOffset << "\n";
        }
        
        // THE FIX: Save the Animation Dropdown Index to the Track data!
        if (i < timeline.tracks.size()) {
            file << "Track=" << timeline.tracks[i].startFrame << "," << timeline.tracks[i].endFrame << "," 
                 << (timeline.tracks[i].stayOnScene ? "1" : "0") << "," << timeline.tracks[i].currentAnimIndex << "\n";
        }
    }

    file << "CamTrackCount=" << timeline.cameraTracks.size() << "\n";
    for (const auto& ct : timeline.cameraTracks) {
        file << "CamTrackDef=" << ct.savedTrackName << "," << ct.timelineStartFrame << "," << ct.timelineEndFrame << "," << ct.trimStart << "," << ct.rig.playbackSpeed << "\n";
        for (const auto& wp : ct.waypoints) {
            file << "CamWP=" << wp.transitTime << "," << wp.position.x << "," << wp.position.y << "," << wp.position.z << ","
                 << wp.target.x << "," << wp.target.y << "," << wp.target.z << "," << wp.fov << "," << wp.holdTime << "," << wp.label << "\n";
        }
    }

    for (const auto& gz : timeline.gameZones) {
        file << "GameZone=" << gz.startFrame << "," << gz.endFrame << ","
             << gz.timeLimitSecs << "," << (gz.infiniteTime ? "1" : "0") << ","
             << gz.label << "\n";
    }

    for (const auto& trk : timeline.audioManager.tracks) {
        file << "TimelineAudio=" << trk.filePath << "," << trk.startFrame << "," << trk.assetName << "\n";
    }
    
    for (const auto& txt : timeline.textTracks) {
        file << "TimelineText=" << txt.startFrame << "," << txt.endFrame << "," << txt.text << "\n";
    }

    file.close();
}

void LoadNexusProject(const std::string& loadPath, std::vector<SceneObject>& scene, std::vector<Vector3>& sceneRotations, NexusTimeline& timeline, NexusCameraTrack& activeCamTrack, bool& playerInit, bool& useMath, int& playerTargetIdx) {
    scene.clear();
    sceneRotations.clear();
    timeline.tracks.clear();
    timeline.cameraTracks.clear();
    timeline.gameZones.clear();
    timeline.textTracks.clear();
    timeline.audioManager.UnloadAll();
    
    activeCamTrack.waypoints.clear();
    activeCamTrack.isActive = false;
    playerInit = false;
    useMath = false;
    playerTargetIdx = -1;

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
    bool inCamTrack = false;

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
            
            // PARSING LIVE PLAYER AND ACTIVE CAMERA STATE
            if (line.find("PlayerInit=") == 0) playerInit = (line.substr(11) == "1");
            else if (line.find("UseMath=") == 0) useMath = (line.substr(8) == "1");
            else if (line.find("PlayerIdx=") == 0) playerTargetIdx = std::stoi(line.substr(10));
            else if (line.find("ActiveCamActive=") == 0) activeCamTrack.isActive = (line.substr(16) == "1");
            else if (line.find("ActiveCamSpeed=") == 0) activeCamTrack.rig.playbackSpeed = std::stof(line.substr(15));
            else if (line.find("ActiveCamWP=") == 0) {
                std::string data = line.substr(12);
                std::stringstream ss(data); std::string tok;
                CameraWaypoint wp;
                auto g = [&](float& v){ std::getline(ss,tok,','); try{v=std::stof(tok);}catch(...){}};
                
                g(wp.legacyTimeStamp); 
                g(wp.position.x); g(wp.position.y); g(wp.position.z);
                g(wp.target.x);   g(wp.target.y);   g(wp.target.z);
                g(wp.fov);
                
                if (std::getline(ss, tok, ',')) {
                    try {
                        size_t pos; wp.holdTime = std::stof(tok, &pos);
                        if (pos != tok.length()) throw std::invalid_argument("Found label");
                        if (std::getline(ss, tok)) wp.label = tok; 
                    } catch (...) {
                        wp.holdTime = 1.0f; wp.label = tok;
                        std::string rest; if (std::getline(ss, rest)) wp.label += "," + rest;
                    }
                } else { wp.holdTime = 1.0f; }
                
                wp.transitTime = wp.legacyTimeStamp; 
                activeCamTrack.waypoints.push_back(wp);
            }

            else if (line.find("Object=") == 0) {
                if (!isFirstObject) finalizeObject();
                isFirstObject = false;

                hasTrack = false;
                loadedParasites.clear();
                clearedWaypoints = false;
                clearedMontage = false;
                inCamTrack = false;
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
            else if (line.find("Scale=") == 0) currentObj.scale = std::stof(line.substr(6));
            else if (line.find("WalkSpeed=") == 0) currentObj.walkSpeed = std::stof(line.substr(10));
            else if (line.find("LoopWaypoints=") == 0) currentObj.loopWaypoints = (line.substr(14) == "1");

            else if (line.find("AudioMode=") == 0) currentObj.audioEmitter.playbackMode = (AudioPlaybackMode)std::stoi(line.substr(10));
            else if (line.find("AudioTrigger=") == 0) currentObj.audioEmitter.triggerType = (AudioTriggerType)std::stoi(line.substr(13));
            else if (line.find("AudioRadius=") == 0) currentObj.audioEmitter.triggerRadius = std::stof(line.substr(12));
            else if (line.find("AudioCooldown=") == 0) currentObj.audioEmitter.cooldownTime = std::stof(line.substr(14));
            else if (line.find("AudioKey=") == 0) currentObj.audioEmitter.interactKey = std::stoi(line.substr(9)); 
            else if (line.find("AudioLine=") == 0) currentObj.audioEmitter.AddLine(line.substr(10));
            
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
                
                // THE FIX: Reload the Animation Dropdown Index from the save file!
                if (std::getline(ss, token, ',')) {
                    try { currentTrack.currentAnimIndex = std::stoi(token); }
                    catch (...) { currentTrack.currentAnimIndex = 0; }
                } else {
                    currentTrack.currentAnimIndex = 0;
                }
                
                std::string trackName = currentObj.filePath.substr(currentObj.filePath.find_last_of("/\\") + 1);
                currentTrack.assetName = trackName.substr(0, trackName.find_last_of('.'));
                
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
            else if (line.find("CamTrackCount=") == 0) {
                inCamTrack = true;
                if (!isFirstObject) finalizeObject();
                isFirstObject = true; 
            }
            else if (line.find("CamTrackDef=") == 0) {
                NexusCameraTrack ct;
                std::string data = line.substr(12);
                std::stringstream ss(data); std::string tok;
                std::getline(ss, ct.savedTrackName, ',');
                std::getline(ss, tok, ','); ct.timelineStartFrame = std::stoi(tok);
                std::getline(ss, tok, ','); ct.timelineEndFrame = std::stoi(tok);
                std::getline(ss, tok, ','); ct.trimStart = std::stof(tok);
                if (std::getline(ss, tok, ',')) ct.rig.playbackSpeed = std::stof(tok); 
                
                timeline.cameraTracks.push_back(ct);
                inCamTrack = true;
            }
            else if (line.find("CamWP=") == 0 && inCamTrack) {
                if (!timeline.cameraTracks.empty()) {
                    std::string data = line.substr(6);
                    std::stringstream ss(data); std::string tok;
                    CameraWaypoint wp;
                    auto g = [&](float& v){ std::getline(ss,tok,','); try{v=std::stof(tok);}catch(...){}};
                    
                    g(wp.legacyTimeStamp); 
                    g(wp.position.x); g(wp.position.y); g(wp.position.z);
                    g(wp.target.x);   g(wp.target.y);   g(wp.target.z);
                    g(wp.fov);
                    
                    if (std::getline(ss, tok, ',')) {
                        try {
                            size_t pos; wp.holdTime = std::stof(tok, &pos);
                            if (pos != tok.length()) throw std::invalid_argument("Found label");
                            if (std::getline(ss, tok)) wp.label = tok; 
                        } catch (...) {
                            wp.holdTime = 1.0f; wp.label = tok;
                            std::string rest; if (std::getline(ss, rest)) wp.label += "," + rest;
                        }
                    } else { wp.holdTime = 1.0f; }
                    
                    wp.transitTime = wp.legacyTimeStamp; 
                    timeline.cameraTracks.back().waypoints.push_back(wp);
                }
            }
        }
        if (!isFirstObject && !inCamTrack) finalizeObject();
        file.close();
    }
    
    {
        std::ifstream f2(loadPath);
        std::string ln;
        while (std::getline(f2, ln)) {
            if (ln.find("GameZone=") == 0) {
                std::stringstream ss(ln.substr(9)); std::string tok;
                GameZone gz;
                std::getline(ss,tok,','); gz.startFrame=(int)std::stof(tok);
                std::getline(ss,tok,','); gz.endFrame=(int)std::stof(tok);
                std::getline(ss,tok,','); gz.timeLimitSecs=std::stof(tok);
                std::getline(ss,tok,','); gz.infiniteTime=(tok=="1");
                if (std::getline(ss,tok)) gz.label=tok;
                timeline.gameZones.push_back(gz);
            }
            else if (ln.find("TimelineAudio=") == 0) {
                std::stringstream ss(ln.substr(14)); std::string tok;
                std::string path; 
                std::getline(ss, path, ',');
                timeline.audioManager.AddAudioTrack(path);
                
                if (!timeline.audioManager.tracks.empty()) {
                    if (std::getline(ss, tok, ',')) timeline.audioManager.tracks.back().startFrame = std::stoi(tok);
                    if (std::getline(ss, tok)) timeline.audioManager.tracks.back().assetName = tok;
                }
            }
            else if (ln.find("TimelineText=") == 0) {
                std::stringstream ss(ln.substr(13)); std::string tok;
                timeline.textTracks.push_back({}); 
                auto& txt = timeline.textTracks.back();
                try {
                    std::getline(ss, tok, ','); txt.startFrame = std::stoi(tok);
                    std::getline(ss, tok, ','); txt.endFrame = std::stoi(tok);
                    std::getline(ss, txt.text); 
                } catch(...) {}
            }
        }
    }
    
    activeCamTrack.RefreshDuration();
    for (auto& ct : timeline.cameraTracks) {
        ct.RefreshDuration();
    }
}