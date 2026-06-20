#pragma once
#include "raylib.h"
#include "raymath.h"
#include <vector>
#include <string>

#define MOCAP_JOINT_COUNT 13

struct MocapFrame {
    Vector3 joints[MOCAP_JOINT_COUNT];
};

class NexusMocapTrack {
public:
    std::string trackName = "Custom MoCap";
    std::vector<MocapFrame> frames;
    float fps = 30.0f;

    int timelineStartFrame = 0;
    int trimStartFrame = 0;
    int trimEndFrame = -1; 

    Vector3 position = {0.0f, 0.0f, 0.0f};
    Vector3 rotation = {0.0f, 0.0f, 0.0f};
    float scale = 1.0f;
    int skinID = 3; 

    bool snapToInteractiveObject = true;
    std::string customMathCode = ""; 

    void Evaluate(float localTimeSeconds, MocapFrame& outFrame) const;
    void SaveToFile(const std::string& path) const;
    void LoadFromFile(const std::string& path);
};