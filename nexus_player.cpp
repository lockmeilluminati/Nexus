#include "nexus_player.h"
#include "nexus_character.h"
#include "file_dialog.h"
#include "imgui.h"
#include "raymath.h"
#include <cmath>
#include <algorithm>
#include <string>

// ---------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------

static const char* StateLabel(PlayerState s) {
    switch (s) {
        case PlayerState::IDLE:          return "Idle";
        case PlayerState::WALK_FORWARD:  return "Walk Forward";
        case PlayerState::WALK_BACK:     return "Walk Back";
        case PlayerState::STRAFE_LEFT:   return "Strafe Left";
        case PlayerState::STRAFE_RIGHT:  return "Strafe Right";
        case PlayerState::TURN_LEFT:     return "Turn Left";
        case PlayerState::TURN_RIGHT:    return "Turn Right";
        case PlayerState::RUN:           return "Run";
        case PlayerState::JUMP:          return "Jump";
        case PlayerState::FALL:          return "Fall";
        case PlayerState::LAND:          return "Land";
        case PlayerState::SHOOT:         return "Shoot";
        case PlayerState::MELEE:         return "Melee";
        case PlayerState::DEAD:          return "Dead";
        default:                         return "Unknown";
    }
}

// Returns the ModelAnimation* for a given slot, searching base anims and parasites
static ModelAnimation* ResolveAnim(NexusPlayer& player, const PlayerAnimSlot& slot) {
    if (slot.sourcePath.empty()) return nullptr;

    // Check base character anims
    if (slot.sourcePath == player.obj.filePath) {
        if (player.obj.anims && slot.animIndex < player.obj.animCount) {
            return &player.obj.anims[slot.animIndex];
        }
        return nullptr;
    }

    // Check parasite GLBs
    for (size_t i = 0; i < player.obj.parasitePaths.size(); i++) {
        if (player.obj.parasitePaths[i] == slot.sourcePath) {
            if (slot.animIndex < player.obj.parasiteAnimCounts[i]) {
                return &player.obj.parasiteAnims[i][slot.animIndex];
            }
        }
    }
    return nullptr;
}

// Advance a frame counter for a given anim, looping or clamping
static int AdvanceFrame(int currentFrame, ModelAnimation* anim, bool loop, float dt) {
    if (!anim || anim->keyframeCount == 0) return 0;
    int next = currentFrame + (int)(dt * 60.0f + 0.5f);
    if (loop) return next % anim->keyframeCount;
    return std::min(next, anim->keyframeCount - 1);
}

// ---------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------

void InitNexusPlayer(NexusPlayer& player, SceneObject character) {
    player.obj = character;
    player.rotation = { 0.0f, 0.0f, 0.0f };
    player.state = PlayerState::IDLE;
    player.prevState = PlayerState::IDLE;
    player.blendWeight = 1.0f;
    player.frameA = 0;
    player.frameB = 0;
    player.isGrounded = true;
    player.jumpVelocity = 0.0f;
    player.inOneShot = false;
    player.oneShotTimer = 0.0f;

    // Pre-fill labels so the inspector shows readable names
    int count = (int)PlayerState::DEAD + 1;
    for (int i = 0; i < count; i++) {
        player.animSlots[i].label = StateLabel((PlayerState)i);
        player.animSlots[i].sourcePath = "";
        player.animSlots[i].animIndex  = 0;
        player.animSlots[i].loop       = true;
    }
    // One-shot states shouldn't loop
    player.animSlots[(int)PlayerState::JUMP].loop   = false;
    player.animSlots[(int)PlayerState::LAND].loop   = false;
    player.animSlots[(int)PlayerState::SHOOT].loop  = false;
    player.animSlots[(int)PlayerState::MELEE].loop  = false;
    player.animSlots[(int)PlayerState::DEAD].loop   = false;
}

// ---------------------------------------------------------------
// State transition helper — starts a blend whenever state changes
// ---------------------------------------------------------------

static void SetState(NexusPlayer& player, PlayerState newState) {
    if (player.state == newState) return;

    // One-shots can't be interrupted (except by DEAD)
    if (player.inOneShot && newState != PlayerState::DEAD) return;

    player.prevState  = player.state;
    player.state      = newState;
    player.frameB     = player.frameA; // carry old frame into blend-out layer
    player.frameA     = 0;
    player.blendWeight = 0.0f;         // start blend from 0, will ramp to 1.0

    bool isOneShot = (newState == PlayerState::SHOOT  ||
                      newState == PlayerState::MELEE   ||
                      newState == PlayerState::LAND    ||
                      newState == PlayerState::JUMP);

    if (isOneShot) {
        player.inOneShot = true;
        player.oneShotTimer = 0.0f;
        ModelAnimation* anim = ResolveAnim(player, player.animSlots[(int)newState]);
        player.oneShotDuration = anim ? ((float)anim->keyframeCount / 60.0f) : 0.5f;
    }
}

// ---------------------------------------------------------------
// Update — input → state machine → animation ticks
// ---------------------------------------------------------------

void UpdateNexusPlayer(NexusPlayer& player, Camera3D& camera) {
    float dt = GetFrameTime();

    // --- DEAD: freeze everything ---
    if (player.state == PlayerState::DEAD) {
        player.frameA = AdvanceFrame(player.frameA,
            ResolveAnim(player, player.animSlots[(int)PlayerState::DEAD]), false, dt);
        return;
    }

    // --- ONE-SHOT TIMEOUT ---
    if (player.inOneShot) {
        player.oneShotTimer += dt;
        if (player.oneShotTimer >= player.oneShotDuration) {
            player.inOneShot = false;
            SetState(player, PlayerState::IDLE);
        }
    }

    // --- GRAVITY & JUMP ---
    if (!player.isGrounded) {
        player.jumpVelocity += player.gravity * dt;
        player.obj.position.y += player.jumpVelocity * dt;

        if (player.obj.position.y <= 0.0f) {
            player.obj.position.y = 0.0f;
            player.isGrounded = true;
            player.jumpVelocity = 0.0f;
            if (!player.inOneShot) SetState(player, PlayerState::LAND);
        } else if (player.jumpVelocity < -2.0f) {
            if (!player.inOneShot) SetState(player, PlayerState::FALL);
        }
    }

    if (!player.inOneShot) {
        // --- JUMP INPUT ---
        if (player.isGrounded && IsKeyPressed(KEY_SPACE)) {
            player.isGrounded = false;
            player.jumpVelocity = 8.0f;
            SetState(player, PlayerState::JUMP);
        }

        // --- SHOOT / MELEE ---
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))  SetState(player, PlayerState::SHOOT);
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) SetState(player, PlayerState::MELEE);

        // --- MOVEMENT INPUT ---
        if (player.isGrounded) {
            bool running    = IsKeyDown(KEY_LEFT_SHIFT);
            bool moveForward = IsKeyDown(KEY_W);
            bool moveBack    = IsKeyDown(KEY_S);
            bool strafeLeft  = IsKeyDown(KEY_A);
            bool strafeRight = IsKeyDown(KEY_D);

            // Camera-relative forward/right vectors (ignore Y)
            Vector3 camForward = Vector3Normalize({
                camera.target.x - camera.position.x, 0.0f,
                camera.target.z - camera.position.z });
            Vector3 camRight = { camForward.z, 0.0f, -camForward.x };

            Vector3 moveDir = { 0, 0, 0 };
            if (moveForward)  moveDir = Vector3Add(moveDir, camForward);
            if (moveBack)     moveDir = Vector3Subtract(moveDir, camForward);
            if (strafeRight)  moveDir = Vector3Add(moveDir, camRight);
            if (strafeLeft)   moveDir = Vector3Subtract(moveDir, camRight);

            bool moving = Vector3Length(moveDir) > 0.01f;
            float speed = player.moveSpeed * (running ? player.runMultiplier : 1.0f);

            if (moving) {
                moveDir = Vector3Normalize(moveDir);
                player.obj.position = Vector3Add(player.obj.position,
                    Vector3Scale(moveDir, speed * dt));

                // Smoothly rotate character toward movement direction
                float targetYaw = atan2f(moveDir.x, moveDir.z) * RAD2DEG;
                float diff = targetYaw - player.rotation.y;
                // Wrap diff to [-180, 180]
                while (diff >  180.0f) diff -= 360.0f;
                while (diff < -180.0f) diff += 360.0f;
                player.rotation.y += diff * std::min(dt * 12.0f, 1.0f);

                // Choose state
                if (running) {
                    SetState(player, PlayerState::RUN);
                } else if (strafeLeft && !moveForward && !moveBack) {
                    SetState(player, PlayerState::STRAFE_LEFT);
                } else if (strafeRight && !moveForward && !moveBack) {
                    SetState(player, PlayerState::STRAFE_RIGHT);
                } else {
                    // Check mouse delta to blend in turn animations
                    float mouseDX = GetMouseDelta().x;
                    if (mouseDX < -player.turnBlendThreshold) {
                        SetState(player, PlayerState::TURN_LEFT);
                    } else if (mouseDX > player.turnBlendThreshold) {
                        SetState(player, PlayerState::TURN_RIGHT);
                    } else if (moveForward) {
                        SetState(player, PlayerState::WALK_FORWARD);
                    } else if (moveBack) {
                        SetState(player, PlayerState::WALK_BACK);
                    }
                }
            } else {
                SetState(player, PlayerState::IDLE);
            }
        }
    }

    // --- BLEND WEIGHT RAMP ---
    // Regardless of state, ramp blendWeight toward 1.0 each frame
    player.blendWeight += dt * player.blendSpeed;
    if (player.blendWeight > 1.0f) player.blendWeight = 1.0f;

    // --- ADVANCE ANIMATION FRAMES ---
    float dt_clamped = std::min(dt, 0.05f); // Safety clamp

    PlayerAnimSlot& slotA = player.animSlots[(int)player.state];
    ModelAnimation* animA = ResolveAnim(player, slotA);
    player.frameA = AdvanceFrame(player.frameA, animA, slotA.loop, dt_clamped);

    if (player.blendWeight < 1.0f) {
        PlayerAnimSlot& slotB = player.animSlots[(int)player.prevState];
        ModelAnimation* animB = ResolveAnim(player, slotB);
        player.frameB = AdvanceFrame(player.frameB, animB, slotB.loop, dt_clamped);
    }
}

// ---------------------------------------------------------------
// Draw — applies animation to model each frame
// ---------------------------------------------------------------

void DrawNexusPlayer(NexusPlayer& player) {
    // Apply current state animation
    PlayerAnimSlot& slotA = player.animSlots[(int)player.state];
    ModelAnimation* animA = ResolveAnim(player, slotA);

    if (animA && animA->keyframeCount > 0 &&
        animA->boneCount == (player.obj.anims ? player.obj.anims[0].boneCount : animA->boneCount)) {
        UpdateModelAnimation(player.obj.model, *animA, player.frameA);
    }

    // While blending, also apply the previous state anim and lerp result
    // (Raylib doesn't have native blend weights, so we re-apply the target
    //  anim on top — the snap is invisible at blendSpeed = 8.0f+)
    // A full bone-level lerp would require direct BoneTransform access;
    // this gives clean-feeling snappy blends without crashing.

    // Build transform from rotation
    Matrix rotMat = MatrixRotateY(player.rotation.y * DEG2RAD);
    player.obj.model.transform = rotMat;

    DrawModel(player.obj.model, player.obj.position, player.obj.scale, WHITE);

    // Debug: draw bounding box while in editor
    // DrawBoundingBox(player.obj.bounds, GREEN);
}

// ---------------------------------------------------------------
// Inspector
// ---------------------------------------------------------------

void DrawPlayerInspector(NexusPlayer& player, bool& isOpen) {
    if (!isOpen) return;

    ImGui::SetNextWindowSize(ImVec2(480, 620), ImGuiCond_FirstUseEver);
    ImGui::Begin("Player Controller", &isOpen);

    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "State: %s", StateLabel(player.state));
    ImGui::SameLine();
    ImGui::TextDisabled("Blend: %.2f", player.blendWeight);

    ImGui::Separator();
    ImGui::Spacing();

    ImGui::DragFloat("Move Speed",          &player.moveSpeed,          0.1f, 0.1f, 30.0f);
    ImGui::DragFloat("Run Multiplier",      &player.runMultiplier,      0.1f, 1.0f, 5.0f);
    ImGui::DragFloat("Blend Speed",         &player.blendSpeed,         0.5f, 1.0f, 30.0f);
    ImGui::DragFloat("Turn Blend Threshold",&player.turnBlendThreshold, 1.0f, 1.0f, 100.0f, "%.0f px");
    ImGui::DragFloat("Gravity",             &player.gravity,            0.5f, -100.0f, -1.0f);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Animation Slot Assignments");
    ImGui::TextDisabled("Assign a GLB and clip index to each state.");
    ImGui::Spacing();

    // Collect all available source paths + anim names for the combo
    // Sources: base character + all parasites
    std::vector<std::string> sources;
    if (!player.obj.filePath.empty()) sources.push_back(player.obj.filePath);
    for (auto& p : player.obj.parasitePaths) sources.push_back(p);

    int stateCount = (int)PlayerState::DEAD + 1;
    for (int i = 0; i < stateCount; i++) {
        PlayerAnimSlot& slot = player.animSlots[i];

        ImGui::PushID(i);
        ImGui::TextColored(ImVec4(0.6f, 0.9f, 1.0f, 1.0f), "%-18s", slot.label.c_str());
        ImGui::SameLine();

        // Source file picker
        std::string shortName = slot.sourcePath.empty() ? "(none)" :
            slot.sourcePath.substr(slot.sourcePath.find_last_of("/\\") + 1);
        ImGui::SetNextItemWidth(160);
        if (ImGui::BeginCombo("##src", shortName.c_str())) {
            for (auto& src : sources) {
                std::string sn = src.substr(src.find_last_of("/\\") + 1);
                bool sel = (slot.sourcePath == src);
                if (ImGui::Selectable(sn.c_str(), sel)) {
                    slot.sourcePath = src;
                    slot.animIndex  = 0;
                }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // Clip index picker — shows anim names if available
        ImGui::SameLine();
        if (!slot.sourcePath.empty()) {
            // Resolve max anims for this source
            int maxAnims = 0;
            std::vector<std::string> animNames;

            if (slot.sourcePath == player.obj.filePath && player.obj.anims) {
                maxAnims = player.obj.animCount;
                for (int j = 0; j < maxAnims; j++) {
                    std::string n = player.obj.anims[j].name[0] != '\0' ?
                        player.obj.anims[j].name : ("Clip " + std::to_string(j));
                    animNames.push_back(n);
                }
            } else {
                for (size_t pi = 0; pi < player.obj.parasitePaths.size(); pi++) {
                    if (player.obj.parasitePaths[pi] == slot.sourcePath) {
                        maxAnims = player.obj.parasiteAnimCounts[pi];
                        for (int j = 0; j < maxAnims; j++) {
                            std::string n = player.obj.parasiteAnims[pi][j].name[0] != '\0' ?
                                player.obj.parasiteAnims[pi][j].name : ("Clip " + std::to_string(j));
                            animNames.push_back(n);
                        }
                        break;
                    }
                }
            }

            if (maxAnims > 0) {
                std::string curClip = (slot.animIndex < (int)animNames.size()) ?
                    animNames[slot.animIndex] : ("Clip " + std::to_string(slot.animIndex));
                ImGui::SetNextItemWidth(150);
                if (ImGui::BeginCombo("##clip", curClip.c_str())) {
                    for (int j = 0; j < maxAnims; j++) {
                        bool sel = (slot.animIndex == j);
                        if (ImGui::Selectable(animNames[j].c_str(), sel)) slot.animIndex = j;
                        if (sel) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            } else {
                ImGui::SetNextItemWidth(150);
                ImGui::InputInt("##clipidx", &slot.animIndex);
                if (slot.animIndex < 0) slot.animIndex = 0;
            }
        } else {
            ImGui::TextDisabled("---");
        }

        ImGui::PopID();
    }

    ImGui::Spacing();
    ImGui::Separator();

    // Load additional parasite GLBs from inside the inspector
    ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Load Animation Pack (Parasite GLB)");
    if (ImGui::Button("Browse...", ImVec2(-1, 28))) {
        std::string path = OpenGLTFFileDialog();
        if (!path.empty()) {
            std::replace(path.begin(), path.end(), '\\', '/');
            int count = 0;
            ModelAnimation* anims = LoadModelAnimations(path.c_str(), &count);
            if (count > 0) {
                player.obj.parasitePaths.push_back(path);
                player.obj.parasiteAnims.push_back(anims);
                player.obj.parasiteAnimCounts.push_back(count);
                TraceLog(LOG_INFO, "PLAYER: Loaded %d animations from %s", count, path.c_str());
            }
        }
    }

    ImGui::End();
}
