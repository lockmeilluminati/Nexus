#pragma once
#include "nexus_core.h"
#include "raylib.h"
#include <vector>
#include <string>

struct LoadedAnim {
    std::string sourcePath;
    int originalIndex;
    std::string displayName;
    ModelAnimation* animPtr; 
};

class NexusAnimator {
private:
    SceneObject previewObj;
    bool isBaseLoaded = false;
    std::vector<LoadedAnim> availableAnims;

    RenderTexture2D previewTarget = {0};
    Camera3D previewCam;
    
    float previewCamDist = 5.0f;
    Vector3 previewCenter = {0.0f, 1.0f, 0.0f};

    Vector2 previewCamAngle = { 45.0f, 30.0f };
    Vector3 previewCamOffset = { 0.0f, 0.0f, 0.0f };

    float playheadTime = 0.0f;
    bool isPlaying = false;
    
    // --- NEW: TIMELINE SCRUBBING STATE ---
    bool isScrubbing = false;

    int draggingBlockIndex = -1;
    int blockDragMode = 0; 
    int selectedBlockIndex = -1;
    
    char saveNameBuf[128] = "";

    float zoomScale = 1.0f;
    float scrollOffset = 0.0f;

public:
    NexusAnimator();
    ~NexusAnimator();
    
    bool needsRescan = false; 
    
    void ResetStudio();
    void Draw(bool& isOpen);
};