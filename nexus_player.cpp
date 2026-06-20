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

static ModelAnimation* ResolveAnim(NexusPlayer& player, const PlayerAnimSlot& slot) {
    if (slot.sourcePath.empty()) return nullptr;

    if (slot.sourcePath == player.obj.filePath) {
        if (player.obj.anims && slot.animIndex < player.obj.animCount) {
            return &player.obj.anims[slot.animIndex];
        }
        return nullptr;
    }

    for (size_t i = 0; i < player.obj.parasitePaths.size(); i++) {
        if (player.obj.parasitePaths[i] == slot.sourcePath) {
            if (slot.animIndex < player.obj.parasiteAnimCounts[i]) {
                return &player.obj.parasiteAnims[i][slot.animIndex];
            }
        }
    }
    return nullptr;
}

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

    int count = (int)PlayerState::DEAD + 1;
    for (int i = 0; i < count; i++) {
        player.animSlots[i].label = StateLabel((PlayerState)i);
        player.animSlots[i].sourcePath = "";
        player.animSlots[i].animIndex  = 0;
        player.animSlots[i].loop       = true;
    }
    player.animSlots[(int)PlayerState::JUMP].loop   = false;
    player.animSlots[(int)PlayerState::LAND].loop   = false;
    player.animSlots[(int)PlayerState::SHOOT].loop  = false;
    player.animSlots[(int)PlayerState::MELEE].loop  = false;
    player.animSlots[(int)PlayerState::DEAD].loop   = false;
}

static void SetState(NexusPlayer& player, PlayerState newState) {
    if (player.state == newState) return;
    if (player.inOneShot && newState != PlayerState::DEAD) return;

    player.prevState  = player.state;
    player.state      = newState;
    player.frameB     = player.frameA; 
    player.frameA     = 0;
    player.blendWeight = 0.0f;         

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

    if (player.isPlayMode) {
        player.playTimer += dt;
        if (player.playTimer >= player.playTimeLimit) {
            player.isPlayMode = false; player.playTimer = 0.f; EnableCursor(); return;
        }

        // Strictly Third Person Camera logic now
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            DisableCursor();
            Vector2 md = GetMouseDelta();
            player.playCamYaw   += md.x * 0.25f;
            player.playCamPitch -= md.y * 0.25f;
            if (player.playCamPitch >  80.f) player.playCamPitch =  80.f;
            if (player.playCamPitch < -10.f) player.playCamPitch = -10.f;
        } else { EnableCursor(); }
        
        float yr = player.playCamYaw*DEG2RAD, pr = player.playCamPitch*DEG2RAD;
        Vector3 o = {player.obj.position.x, player.obj.position.y+1.2f, player.obj.position.z};
        player.playCamera.target   = o;
        player.playCamera.position = {
            o.x + player.playCamDist*cosf(pr)*sinf(yr),
            o.y + player.playCamDist*sinf(pr),
            o.z + player.playCamDist*cosf(pr)*cosf(yr)};
        player.playCamera.up={0,1,0}; player.playCamera.fovy=60.f;
        player.playCamera.projection=CAMERA_PERSPECTIVE;
        camera = player.playCamera;
    }

    if (player.state == PlayerState::DEAD) {
        player.frameA = AdvanceFrame(player.frameA,
            ResolveAnim(player, player.animSlots[(int)PlayerState::DEAD]), false, dt);
        return;
    }

    if (player.inOneShot) {
        player.oneShotTimer += dt;
        if (player.oneShotTimer >= player.oneShotDuration) {
            player.inOneShot = false;
            SetState(player, PlayerState::IDLE);
        }
    }

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
        if (player.isGrounded && IsKeyPressed(KEY_SPACE)) {
            player.isGrounded = false;
            player.jumpVelocity = 8.0f;
            SetState(player, PlayerState::JUMP);
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))  SetState(player, PlayerState::SHOOT);
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) SetState(player, PlayerState::MELEE);

        if (player.isGrounded) {
            bool running    = IsKeyDown(KEY_LEFT_SHIFT);
            bool moveForward = IsKeyDown(KEY_W);
            bool moveBack    = IsKeyDown(KEY_S);
            bool strafeLeft  = IsKeyDown(KEY_A);
            bool strafeRight = IsKeyDown(KEY_D);

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

                float targetYaw = atan2f(moveDir.x, moveDir.z) * RAD2DEG;
                float diff = targetYaw - player.rotation.y;
                while (diff >  180.0f) diff -= 360.0f;
                while (diff < -180.0f) diff += 360.0f;
                player.rotation.y += diff * std::min(dt * 12.0f, 1.0f);

                if (running) {
                    SetState(player, PlayerState::RUN);
                } else if (strafeLeft && !moveForward && !moveBack) {
                    SetState(player, PlayerState::STRAFE_LEFT);
                } else if (strafeRight && !moveForward && !moveBack) {
                    SetState(player, PlayerState::STRAFE_RIGHT);
                } else {
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

    player.blendWeight += dt * player.blendSpeed;
    if (player.blendWeight > 1.0f) player.blendWeight = 1.0f;

    float dt_clamped = std::min(dt, 0.05f); 

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
    PlayerAnimSlot& slotA = player.animSlots[(int)player.state];
    ModelAnimation* animA = ResolveAnim(player, slotA);

    // Third Person: identity before update, rotation only via DrawModelEx.
    player.obj.model.transform = MatrixIdentity();
    if (animA && animA->keyframeCount > 0 &&
        IsModelAnimationValid(player.obj.model, *animA)) {
        UpdateModelAnimation(player.obj.model, *animA, player.frameA);
    }
    Vector3 rotAxis = { 0.0f, 1.0f, 0.0f };
    Vector3 scaleVec = { player.obj.scale, player.obj.scale, player.obj.scale };
    DrawModelEx(player.obj.model, player.obj.position, rotAxis, player.rotation.y, scaleVec, WHITE);
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

    std::vector<std::string> sources;
    if (!player.obj.filePath.empty()) sources.push_back(player.obj.filePath);
    for (auto& p : player.obj.parasitePaths) sources.push_back(p);

    int stateCount = (int)PlayerState::DEAD + 1;
    for (int i = 0; i < stateCount; i++) {
        PlayerAnimSlot& slot = player.animSlots[i];

        ImGui::PushID(i);
        ImGui::TextColored(ImVec4(0.6f, 0.9f, 1.0f, 1.0f), "%-18s", slot.label.c_str());
        ImGui::SameLine();

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

        ImGui::SameLine();
        if (!slot.sourcePath.empty()) {
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

void EnterPlayMode(NexusPlayer& player, float limit) {
    player.isPlayMode=true; player.playTimer=0.f; player.playTimeLimit=limit;
    player.state=PlayerState::IDLE; player.prevState=PlayerState::IDLE;
    player.blendWeight=1.f; player.frameA=player.frameB=0; player.inOneShot=false;
    player.playCamYaw=player.rotation.y; player.playCamPitch=20.f; player.playCamDist=6.f;
    player.playCamera.up={0,1,0}; player.playCamera.fovy=60.f;
    player.playCamera.projection=CAMERA_PERSPECTIVE;
    TraceLog(LOG_INFO,"PLAY MODE: %.1fs",limit);
}

void DrawPlayModeHUD(NexusPlayer& player) {
    if (!player.isPlayMode) return;
    bool inf = player.playTimeLimit>=999990.f;
    float rem = std::max(0.f,player.playTimeLimit-player.playTimer);
    ImGuiIO& io=ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x*.5f,20),ImGuiCond_Always,ImVec2(.5f,0));
    ImGui::SetNextWindowBgAlpha(.65f);
    ImGui::Begin("##phud",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoNav);
    ImVec4 col=inf?ImVec4(.4f,1,.5f,1):rem>3?ImVec4(.2f,1,.3f,1):ImVec4(1,.3f,.1f,1);
    if(inf) ImGui::TextColored(col,"  PLAY MODE  |  INFINITE  ");
    else    ImGui::TextColored(col,"  PLAY MODE  |  %.1fs remaining  ",rem);
    ImGui::End();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x*.5f,52),ImGuiCond_Always,ImVec2(.5f,0));
    ImGui::SetNextWindowBgAlpha(.65f);
    ImGui::Begin("##pstop",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoNav);
    ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(.7f,.1f,.1f,1));
    if(ImGui::Button(" STOP GAME ")) { player.isPlayMode=false; player.playTimer=0; EnableCursor(); }
    ImGui::PopStyleColor();
    ImGui::End();
}