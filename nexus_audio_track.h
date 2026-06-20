#pragma once
#include <string>
#include <vector>
#include "raylib.h"

// The data structure for an individual audio stream
struct TimelineAudioTrack {
    std::string filePath;
    std::string assetName;
    int startFrame = 0;
    int durationFrames = 0;
    
    // Records how much of the intro has been shaved off
    float trimStartSecs = 0.0f; 
    
    // THE FIX: New Volume Variable
    float volume = 1.0f; 
    
    Music stream;
    bool isLoaded = false;
};

class NexusAudioSystem {
public:
    std::vector<TimelineAudioTrack> tracks;

    void AddAudioTrack(const std::string& path);
    void UpdateAudioSync(int globalFrame, bool isPlaying);
    void RemoveTrack(int index);
    void UnloadAll();
};