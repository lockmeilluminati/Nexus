#pragma once
#include "raylib.h"
#include "raymath.h"
#include "nexus_mocap_track.h" 
#include <vector>
#include <string>

// Available Raylib Math Skins
enum class MocapSkin { 
    TACTICAL_VOXEL, 
    GRAPHENE_CARBON, 
    TARTARIAN_RESONATOR,
    CUSTOM_MATH_CODE,
    PROCEDURAL_HUMANOID
};

class NexusMocapStudio {
public:
    NexusMocapStudio();
    ~NexusMocapStudio();

    void Init();
    void Shutdown();
    void Update(float dt);
    
    void UpdateViewport();
    void DrawUI(bool& isOpen);

    // Moves RenderSkin to public so main.cpp can use it to draw the math characters on the timeline
    void RenderSkin(const MocapFrame& frame, int overrideSkinID = -1);

    // Playback State
    bool isRecording = false;
    bool isPlaying = false;
    float playheadTime = 0.0f;
    float fps = 30.0f;

    // Skin & Audio State
    MocapSkin activeSkin = MocapSkin::CUSTOM_MATH_CODE;
    bool simulateAudioTalking = false;
    float audioLevels = 0.0f;
    
    // Object State
    Vector3 rootPosition = {0.0f, 1.5f, 0.0f}; 
    float scale = 1.0f; 

    // UI Buffers
    char customMathCodeBuffer[8192]; 
    char saveNameBuffer[128];
    bool snapToInteractiveObj = true;
    
    int trimStart = 0;
    int trimEnd = 0;
    
    // Engine Pipeline Triggers
    bool requestSaveToTimeline = false;
    bool requestAssetRescan = false;
    NexusMocapTrack outputTrack;

    // UI Feedback
    std::string studioFeedback = "";
    float studioFeedbackTimer = 0.0f;

private:
    std::vector<MocapFrame> recordedSequence;
    void* sharedMemoryPtr = nullptr;
    
    RenderTexture2D renderTarget;
    Camera3D previewCamera;
    bool isViewportHovered = false;
    bool isCameraDragging = false;
    
    MocapFrame GetLiveFrame();
};