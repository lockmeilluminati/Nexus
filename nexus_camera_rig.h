#pragma once
#include "raylib.h"
#include <vector>
#include <string>

// ---------------------------------------------------------------
// A single camera waypoint — position + target + timing
// ---------------------------------------------------------------
struct CameraWaypoint {
    Vector3 position   = { 0.0f, 5.0f, 10.0f };
    Vector3 target     = { 0.0f, 0.0f,  0.0f };
    float   transitTime= 2.0f;   // NEW: Time it takes to fly to this point from the last one
    float   holdTime   = 1.0f;   // Absolute time the camera sits here
    float   fov        = 60.0f;
    std::string label  = "";
    
    // Legacy support for loading V1 saves
    float   legacyTimeStamp = 0.0f; 
};

enum class CamTrackMode { SPLINE, LINEAR, CUT };

// ---------------------------------------------------------------
// Dedicated rig to handle the advanced math of cinematic camera flight
// ---------------------------------------------------------------
class NexusCameraRig {
public:
    // The master speed controller for the entire track (only affects travel time!)
    float playbackSpeed = 1.0f; 

    // Evaluates the precise 3D state of the camera at any given real-world time
    void Evaluate(float t, const std::vector<CameraWaypoint>& waypoints, 
                  CamTrackMode mode, bool loop, float totalDuration, 
                  Vector3& outPos, Vector3& outTarget, float& outFov) const;

    // Recalculates the absolute length of the track based on speed and holds
    float CalculateDuration(const std::vector<CameraWaypoint>& waypoints) const;

private:
    static Vector3 CatmullRom(Vector3 p0, Vector3 p1, Vector3 p2, Vector3 p3, float t);
};