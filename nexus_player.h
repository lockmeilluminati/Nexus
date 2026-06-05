#pragma once
#include "nexus_core.h"
#include "raylib.h"
#include <string>

// ---------------------------------------------------------------
// Player Animation State Machine
// Each state maps to a named animation clip from the Quaternius
// Universal Animation Library (or any compatible GLB source).
// ---------------------------------------------------------------

enum class PlayerState {
    IDLE,
    WALK_FORWARD,
    WALK_BACK,
    STRAFE_LEFT,
    STRAFE_RIGHT,
    TURN_LEFT,       // Blended: walk_forward + strafe_left weight
    TURN_RIGHT,      // Blended: walk_forward + strafe_right weight
    RUN,
    JUMP,
    FALL,
    LAND,
    SHOOT,
    MELEE,
    DEAD
};

struct PlayerAnimSlot {
    std::string label;       // Display name in inspector ("Walk Forward")
    std::string sourcePath;  // GLB path the clip lives in
    int animIndex = 0;       // Index inside that GLB
    bool loop = true;
};

struct NexusPlayer {
    SceneObject obj;         // The actual character — shares SceneObject with engine
    Vector3 rotation = { 0.0f, 0.0f, 0.0f };

    PlayerState state        = PlayerState::IDLE;
    PlayerState prevState    = PlayerState::IDLE;

    // One slot per state enum value — indexed by (int)PlayerState
    PlayerAnimSlot animSlots[(int)PlayerState::DEAD + 1];

    // Blend system: two animation layers crossfading
    float blendWeight        = 1.0f;   // 0.0 = fully prevState anim, 1.0 = fully state anim
    float blendSpeed         = 8.0f;   // How fast blends snap (units/sec)
    int   frameA             = 0;      // Frame counter for current state anim
    int   frameB             = 0;      // Frame counter for previous state anim (blend-out)

    // Movement config
    float moveSpeed          = 4.0f;
    float runMultiplier      = 2.0f;
    float turnBlendThreshold = 15.0f;  // Mouse delta X above this triggers turn blend

    bool  isGrounded         = true;
    float jumpVelocity       = 0.0f;
    float gravity            = -20.0f;

    // Land state timer (plays LAND anim once then returns to idle)
    float landTimer          = 0.0f;
    float landDuration       = 0.3f;

    // One-shot state timer (SHOOT, MELEE, LAND play once then snap back)
    float oneShotTimer       = 0.0f;
    float oneShotDuration    = 0.0f;
    bool  inOneShot          = false;
};

// ---------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------

// Initializes the player from a spawned SceneObject.
// Call this after SpawnCharacter(), passing the result in.
void InitNexusPlayer(NexusPlayer& player, SceneObject character);

// Called every frame inside the main 3D update loop
void UpdateNexusPlayer(NexusPlayer& player, Camera3D& camera);

// Called every frame inside BeginMode3D / EndMode3D
void DrawNexusPlayer(NexusPlayer& player);

// ---------------------------------------------------------------
// Inspector panel (ImGui) — call from your right-panel inspector
// ---------------------------------------------------------------
void DrawPlayerInspector(NexusPlayer& player, bool& isOpen);
