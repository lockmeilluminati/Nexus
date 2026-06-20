#pragma once
#include "nexus_core.h"
#include "raylib.h"
#include <string>

// NEW: The Perspective State Switch
enum class PlayerPerspective {
    PERSPECTIVE_THIRD_PERSON,
    PERSPECTIVE_FIRST_PERSON
};

enum class PlayerState {
    IDLE, WALK_FORWARD, WALK_BACK, STRAFE_LEFT, STRAFE_RIGHT,
    TURN_LEFT, TURN_RIGHT, RUN, JUMP, FALL, LAND, SHOOT, MELEE, DEAD
};

struct PlayerAnimSlot {
    std::string label;       
    std::string sourcePath;  
    int animIndex = 0;       
    bool loop = true;
};

struct NexusPlayer {
    SceneObject obj;         
    Vector3 rotation = { 0.0f, 0.0f, 0.0f };

    // NEW: Perspective Data Tracker
    PlayerPerspective perspective = PlayerPerspective::PERSPECTIVE_THIRD_PERSON;
    float fpEyeHeight  = 1.6f;
    float fpBehindDist = 0.0f;

    PlayerState state     = PlayerState::IDLE;
    PlayerState prevState = PlayerState::IDLE;

    PlayerAnimSlot animSlots[(int)PlayerState::DEAD + 1];

    float blendWeight        = 1.0f;   
    float blendSpeed         = 8.0f;   
    int   frameA             = 0;      
    int   frameB             = 0;      

    float moveSpeed          = 4.0f;
    float runMultiplier      = 2.0f;
    float turnBlendThreshold = 15.0f;  

    bool  isGrounded         = true;
    float jumpVelocity       = 0.0f;
    float gravity            = -20.0f;

    float landTimer          = 0.0f;
    float landDuration       = 0.3f;

    float oneShotTimer       = 0.0f;
    float oneShotDuration    = 0.0f;
    bool  inOneShot          = false;

    bool     isPlayMode      = false;
    float    playTimeLimit   = 10.0f;
    float    playTimer       = 0.0f;
    Camera3D playCamera      = { 0 };
    float    playCamYaw      = 0.0f;
    float    playCamPitch    = 20.0f;
    float    playCamDist     = 6.0f;
};

void InitNexusPlayer(NexusPlayer& player, SceneObject character);
void UpdateNexusPlayer(NexusPlayer& player, Camera3D& camera);
void DrawNexusPlayer(NexusPlayer& player);

void DrawPlayerInspector(NexusPlayer& player, bool& isOpen);
void EnterPlayMode(NexusPlayer& player, float timeLimitSeconds);
void DrawPlayModeHUD(NexusPlayer& player);