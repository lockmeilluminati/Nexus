#pragma once
#include "raylib.h"
#include "raymath.h"

class NexusMathPlayer {
public:
    Camera3D camera;
    bool isActive;
    
    // Independent rotation tracking
    float yaw;
    float pitch;

    // Animation States
    float walkBobTime;
    float punchExtension;
    bool isPunching;

    void Init();
    void Update();
    void DrawProceduralArms();
};