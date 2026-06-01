#ifndef NEXUS_TIMELINE_H
#define NEXUS_TIMELINE_H

#include "raylib.h"
#include <string>
#include <vector>

struct TimelineTrack {
    std::string assetName;
    int sourceDuration; 
    int startFrame;     
    int endFrame;       
    bool isEnvironment;
};

class NexusTimeline {
public:
    void AddTrack(const std::string& name, int frames, bool isEnv);
    void Draw(int& globalFrame, float leftOffset); // NEW: Added leftOffset parameter

    std::vector<TimelineTrack> tracks;
    bool isPlaying = false; 
    int maxTimelineFrames = 1800; 
};

#endif