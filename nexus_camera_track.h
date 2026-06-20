#pragma once
#include "raylib.h"
#include "raymath.h"
#include "nexus_camera_rig.h" 
#include <vector>
#include <string>

class NexusCameraTrack {
public:
    std::vector<CameraWaypoint> waypoints;
    CamTrackMode mode       = CamTrackMode::SPLINE;

    bool  isActive          = false;   
    bool  isRecording       = false;   
    float playheadTime      = 0.0f;   
    float totalDuration     = 0.0f;   
    bool  loop              = false;
    bool  publishEnabled    = true;   

    NexusCameraRig rig;

    // Editor state
    int         selectedWaypoint = -1;
    bool        showUI           = false;
    float       quickPlaceTime   = 0.0f; // THE FIX: Restored to instantly fix main.cpp!
    std::string saveFilePath     = "camera_track.nxcam";
    bool        saveToTimeline   = false;  
    std::string savedTrackName   = "Camera 1"; 
    int         savedTrackCount  = 0;       
    float       savedTrackDuration = 0.0f;  
    bool        showWaypoints    = true;    

    // Timeline Trim
    int         timelineStartFrame = 0;
    int         timelineEndFrame   = 300;
    float       trimStart          = 0.0f; 

    void Evaluate(float t, Vector3& outPos, Vector3& outTarget, float& outFov) const;
    void Update(float dt);
    void ApplyToCamera(Camera3D& cam) const;
    void RecordWaypoint(const Camera3D& cam);
    void DeleteWaypoint(int index);
    void RefreshDuration();
    void DrawPath() const;
    void DrawUI(bool& isOpen, Camera3D& editorCam, float currentTimeSeconds);
    void SaveTrack(const std::string& path) const;
    void LoadTrack(const std::string& path);
};