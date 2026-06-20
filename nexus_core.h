#pragma once
#include "raylib.h"
#include <string>
#include <vector>
#include "nexus_audio_emitter.h" // --- NEW: Include the Gameplay Audio Emitter

struct Asset {
    std::string name;
    std::string filePath;
    Texture2D thumbnail;
};

struct MontageBlock {
    std::string sourcePath; 
    int animIndex;         
    std::string animName;  
    float startTime;       
    float duration;        
    float blendInTime;     

    // --- NEW: ROOT MOTION OFFSETS ---
    Vector3 positionOffset = { 0.0f, 0.0f, 0.0f };
    Vector3 rotationOffset = { 0.0f, 0.0f, 0.0f };
    float scaleOffset = 1.0f;
};

struct SceneObject {
    std::string name = "";
    std::string filePath = "";
    
    Model model = { 0 };
    Vector3 position = { 0.0f, 0.0f, 0.0f };
    float scale = 1.0f;
    BoundingBox bounds = { 0 };
    bool isAnimated = false;
    ModelAnimation* anims = nullptr; 
    int animCount = 0;
    int currentFrame = 0;

    std::vector<Vector3> waypoints;
    int targetWaypointIndex = 0;
    float walkSpeed = 3.0f;
    bool loopWaypoints = false;

    std::vector<MontageBlock> montage;

    std::vector<std::string> parasitePaths;
    std::vector<ModelAnimation*> parasiteAnims;
    std::vector<int> parasiteAnimCounts;

    // Animation slot assignments set by Character Creator (index into anims[])
    // Indexed by (int)PlayerState — 14 states total
    int playerAnimSlots[14] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};

    // --- NEW: GAMEPLAY AUDIO COMPONENT ---
    // Handles proximity triggers, interact triggers, and the playlist
    CharacterAudioComponent audioEmitter;
};