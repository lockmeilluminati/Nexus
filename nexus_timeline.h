#pragma once
#include <string>
#include <vector>

struct TimelineTrack {
    std::string assetName = "";
    int startFrame = 0;
    int endFrame = 0;
    
    // --- THE CRITICAL FIX ---
    // Forces the array index to 0 so it never reads out of bounds
    int currentAnimIndex = 0; 
    
    int sourceDuration = 0;
    bool isEnvironment = false;
    bool stayOnScene = false;
};

class NexusTimeline {
public:
    std::vector<TimelineTrack> tracks;
    bool isPlaying = false;
    int maxTimelineFrames = 1800; 

    void AddTrack(const std::string& name, int animFrames, bool isEnv);
    void Draw(int& globalFrame, float browserWidth, int& deleteIndex);
};