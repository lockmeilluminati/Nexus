#include "nexus_camera_rig.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

Vector3 NexusCameraRig::CatmullRom(Vector3 p0, Vector3 p1, Vector3 p2, Vector3 p3, float t) {
    float t2 = t * t;
    float t3 = t2 * t;
    return {
        0.5f * ((2*p1.x) + (-p0.x+p2.x)*t + (2*p0.x-5*p1.x+4*p2.x-p3.x)*t2 + (-p0.x+3*p1.x-3*p2.x+p3.x)*t3),
        0.5f * ((2*p1.y) + (-p0.y+p2.y)*t + (2*p0.y-5*p1.y+4*p2.y-p3.y)*t2 + (-p0.y+3*p1.y-3*p2.y+p3.y)*t3),
        0.5f * ((2*p1.z) + (-p0.z+p2.z)*t + (2*p0.z-5*p1.z+4*p2.z-p3.z)*t2 + (-p0.z+3*p1.z-3*p2.z+p3.z)*t3)
    };
}

float NexusCameraRig::CalculateDuration(const std::vector<CameraWaypoint>& waypoints) const {
    if (waypoints.empty()) return 0.0f;
    float realDuration = 0.0f;
    for (const auto& wp : waypoints) {
        // Playback speed only dilates the flight time, never the hold time!
        realDuration += (wp.transitTime / playbackSpeed) + wp.holdTime;
    }
    return realDuration;
}

void NexusCameraRig::Evaluate(float t, const std::vector<CameraWaypoint>& waypoints, 
                              CamTrackMode mode, bool loop, float totalDuration, 
                              Vector3& outPos, Vector3& outTarget, float& outFov) const {
    if (waypoints.empty()) return;
    
    // Handle Single Waypoint Holds perfectly
    if (waypoints.size() == 1) {
        outPos    = waypoints[0].position;
        outTarget = waypoints[0].target;
        outFov    = waypoints[0].fov;
        return;
    }

    float dur = totalDuration > 0.0f ? totalDuration : 1.0f;
    if (loop) t = fmodf(t, dur);
    t = std::clamp(t, 0.0f, dur);

    float currentRealTime = 0.0f;
    for (size_t i = 0; i < waypoints.size(); i++) {
        
        float transitReal = waypoints[i].transitTime / playbackSpeed; 
        
        // 1. ARE WE FLYING TO THIS WAYPOINT?
        if (t < currentRealTime + transitReal) {
            if (i == 0) { 
                outPos = waypoints[0].position; outTarget = waypoints[0].target; outFov = waypoints[0].fov;
                return;
            }
            
            float localTransitT = t - currentRealTime;
            float normalizedT = (transitReal > 0.001f) ? (localTransitT / transitReal) : 1.0f;
            normalizedT = std::clamp(normalizedT, 0.0f, 1.0f);
            
            // CINEMATIC EASING: Smoothly accelerates and decelerates!
            float easeT = 0.5f * (1.0f - cosf(PI * normalizedT));
            
            if (mode == CamTrackMode::CUT) {
                outPos = waypoints[i-1].position; outTarget = waypoints[i-1].target; outFov = waypoints[i-1].fov;
                return;
            }
            if (mode == CamTrackMode::LINEAR) {
                outPos    = Vector3Lerp(waypoints[i-1].position, waypoints[i].position, easeT);
                outTarget = Vector3Lerp(waypoints[i-1].target,   waypoints[i].target,   easeT);
                outFov    = waypoints[i-1].fov + (waypoints[i].fov - waypoints[i-1].fov) * easeT;
                return;
            }
            
            // SPLINE MATH
            int n = (int)waypoints.size();
            int i0 = std::max((int)i - 2, 0);
            int i1 = i - 1;
            int i2 = i;
            int i3 = std::min((int)i + 1, n - 1);

            outPos    = CatmullRom(waypoints[i0].position, waypoints[i1].position,
                                   waypoints[i2].position, waypoints[i3].position, easeT);
            outTarget = CatmullRom(waypoints[i0].target,   waypoints[i1].target,
                                   waypoints[i2].target,   waypoints[i3].target,   easeT);
            outFov    = waypoints[i1].fov + (waypoints[i2].fov - waypoints[i1].fov) * easeT;
            return;
        }
        
        currentRealTime += transitReal;
        
        // 2. ARE WE HOLDING AT THIS WAYPOINT?
        if (t <= currentRealTime + waypoints[i].holdTime) {
            outPos    = waypoints[i].position;
            outTarget = waypoints[i].target;
            outFov    = waypoints[i].fov;
            return;
        }
        
        currentRealTime += waypoints[i].holdTime; 
    }

    // Fallback Bounds
    outPos    = waypoints.back().position;
    outTarget = waypoints.back().target;
    outFov    = waypoints.back().fov;
}