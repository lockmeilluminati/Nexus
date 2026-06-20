#include "nexus_animator.h"
#include "nexus_character.h"
#include "nexus_prefab.h"
#include "file_dialog.h"
#include "imgui.h"
#include "raymath.h" 
#include <cmath> 
#include <algorithm>

NexusAnimator::NexusAnimator() {}

NexusAnimator::~NexusAnimator() {
    if (previewTarget.id != 0) UnloadRenderTexture(previewTarget);
    ResetStudio(); 
}

void NexusAnimator::ResetStudio() {
    if (isBaseLoaded) {
        UnloadModel(previewObj.model);
        if (previewObj.anims) UnloadModelAnimations(previewObj.anims, previewObj.animCount);
        for(size_t i=0; i<previewObj.parasiteAnims.size(); i++) {
            UnloadModelAnimations(previewObj.parasiteAnims[i], previewObj.parasiteAnimCounts[i]);
        }
    }
    isBaseLoaded = false;
    availableAnims.clear();
    previewObj.montage.clear();
    previewObj.parasitePaths.clear();
    previewObj.parasiteAnims.clear();
    previewObj.parasiteAnimCounts.clear();
    playheadTime = 0.0f;
    isPlaying = false;
    isScrubbing = false;
    previewCamOffset = {0.0f, 0.0f, 0.0f};
    previewCamAngle = {45.0f, 30.0f};
    selectedBlockIndex = -1; 
}

void NexusAnimator::Draw(bool& isOpen) {
    if (!isOpen) return;

    if (previewTarget.id == 0) {
        previewTarget = LoadRenderTexture(400, 300);
        previewCam = { { 0.0f, 1.5f, 5.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, 45.0f, CAMERA_PERSPECTIVE };
    }

    ImGui::SetNextWindowSize(ImVec2(1000, 600), ImGuiCond_FirstUseEver);
    ImGui::Begin("Animation State Machine (Montage Studio)", &isOpen);

    // --- LEFT PANEL ---
    ImGui::BeginChild("LeftPanel", ImVec2(250, 0), true);
    ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "1. Setup Skeleton");
    
    if (ImGui::Button("Load Base Character", ImVec2(-1, 30))) {
        std::string path = OpenGLTFFileDialog();
        if (!path.empty()) {
            std::replace(path.begin(), path.end(), '\\', '/');
            ResetStudio(); 
            previewObj = SpawnCharacter(path);
            isBaseLoaded = true;

            BoundingBox bounds = previewObj.bounds;
            previewCenter = {
                (bounds.max.x + bounds.min.x) / 2.0f,
                (bounds.max.y + bounds.min.y) / 2.0f,
                (bounds.max.z + bounds.min.z) / 2.0f
            };
            float maxSize = fmax(bounds.max.x - bounds.min.x, fmax(bounds.max.y - bounds.min.y, bounds.max.z - bounds.min.z));
            previewCamDist = maxSize * 1.5f;
            if (previewCamDist < 2.0f) previewCamDist = 5.0f;

            for(int i=0; i<previewObj.animCount; i++) {
                std::string aName = previewObj.anims[i].name[0] != '\0' ? previewObj.anims[i].name : "Base Anim " + std::to_string(i);
                availableAnims.push_back({previewObj.filePath, i, aName, &previewObj.anims[i]});
            }
        }
    }
    
    if (ImGui::Button("CLEAR ENTIRE STUDIO", ImVec2(-1, 20))) ResetStudio();

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "2. Inject Parasites");
    if (ImGui::Button("Load Animation GLB", ImVec2(-1, 30))) {
        if (!isBaseLoaded) {
            TraceLog(LOG_WARNING, "NEXUS: Must load a base character first!");
        } else {
            std::string path = OpenGLTFFileDialog();
            if (!path.empty()) {
                std::replace(path.begin(), path.end(), '\\', '/');
                int count = 0;
                ModelAnimation* anims = LoadModelAnimations(path.c_str(), &count);
                if (count > 0) {
                    previewObj.parasitePaths.push_back(path);
                    previewObj.parasiteAnims.push_back(anims);
                    previewObj.parasiteAnimCounts.push_back(count);
                    for(int i=0; i<count; i++) {
                        std::string aName = anims[i].name[0] != '\0' ? anims[i].name : "Injected Anim " + std::to_string(i);
                        availableAnims.push_back({path, i, aName, &anims[i]});
                    }
                }
            }
        }
    }

    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "3. Available Tracks");
    
    ImGui::BeginChild("AnimList", ImVec2(0, 0), true);
    if (!isBaseLoaded) ImGui::TextDisabled("Awaiting base mesh...");
    for (auto& a : availableAnims) {
        if (ImGui::Button(("Add " + a.displayName).c_str(), ImVec2(-1, 0))) {
            float lengthSec = (float)a.animPtr->keyframeCount / 60.0f;
            previewObj.montage.push_back({a.sourcePath, a.originalIndex, a.displayName, 0.0f, lengthSec, 0.5f});
        }
    }
    ImGui::EndChild();
    ImGui::EndChild();

    ImGui::SameLine();

    // --- MIDDLE PANEL: CANVAS ---
    ImGui::BeginChild("MiddlePanel", ImVec2(ImGui::GetContentRegionAvail().x - 420, 0), true);
    ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "4. Timeline Canvas");
    
    ImGui::SetNextItemWidth(100);
    ImGui::SliderFloat("Zoom", &zoomScale, 1.0f, 5.0f, "%.1fx");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    ImGui::SliderFloat("Pan Scroll", &scrollOffset, 0.0f, 30.0f, "%.1fs");

    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    
    // Increased gap slightly to accommodate the new Delete Button
    canvasSize.y -= 200.0f; 
    
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(30, 30, 30, 255));
    drawList->PushClipRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), true);

    float maxTime = 30.0f; 
    float basePixelsPerSec = canvasSize.x / maxTime;
    float pixelsPerSecond = basePixelsPerSec * zoomScale;
    float viewOffsetX = scrollOffset * basePixelsPerSec;

    for (int i = 0; i <= 30; i++) {
        float x = canvasPos.x + (i * pixelsPerSecond) - viewOffsetX;
        if (x > canvasPos.x && x < canvasPos.x + canvasSize.x) {
            drawList->AddLine(ImVec2(x, canvasPos.y), ImVec2(x, canvasPos.y + canvasSize.y), IM_COL32(100, 100, 100, 100));
            if (i % 5 == 0) drawList->AddText(ImVec2(x + 2, canvasPos.y + 2), IM_COL32(200, 200, 200, 255), (std::to_string(i) + "s").c_str());
        }
    }

    ImVec2 mousePos = ImGui::GetMousePos();
    bool mouseClicked = ImGui::IsMouseClicked(0);
    bool mouseReleased = ImGui::IsMouseReleased(0);
    bool mouseDown = ImGui::IsMouseDown(0);

    bool inRuler = (mousePos.x >= canvasPos.x && mousePos.x <= canvasPos.x + canvasSize.x &&
                    mousePos.y >= canvasPos.y && mousePos.y <= canvasPos.y + 25.0f);
    
    if (inRuler && mouseClicked) {
        isScrubbing = true;
        isPlaying = false; 
    }
    
    if (isScrubbing) {
        if (mouseReleased) isScrubbing = false;
        
        float newTime = (mousePos.x - canvasPos.x + viewOffsetX) / pixelsPerSecond;
        playheadTime = newTime;
        
        if (playheadTime < 0.0f) playheadTime = 0.0f;
        if (playheadTime > maxTime) playheadTime = maxTime;
    }

    float px = canvasPos.x + (playheadTime * pixelsPerSecond) - viewOffsetX;
    
    if (mouseReleased) draggingBlockIndex = -1;

    for (size_t i = 0; i < previewObj.montage.size(); i++) {
        auto& block = previewObj.montage[i];
        
        float x1 = canvasPos.x + (block.startTime * pixelsPerSecond) - viewOffsetX;
        float x2 = canvasPos.x + ((block.startTime + block.duration) * pixelsPerSecond) - viewOffsetX;
        float y1 = canvasPos.y + 25.0f + (i * 35.0f); 
        float y2 = y1 + 30.0f;

        float handleWidth = 10.0f;
        bool inClip = (mousePos.x >= canvasPos.x && mousePos.x <= canvasPos.x + canvasSize.x);
        bool overRightEdge = (mousePos.x >= x2 - handleWidth && mousePos.x <= x2 && mousePos.y >= y1 && mousePos.y <= y2 && inClip);
        bool overBody = (mousePos.x >= x1 && mousePos.x < x2 - handleWidth && mousePos.y >= y1 && mousePos.y <= y2 && inClip);

        if (overRightEdge) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

        if (mouseClicked) {
            if (overRightEdge) {
                draggingBlockIndex = i; blockDragMode = 1; 
                selectedBlockIndex = i; 
            } else if (overBody) {
                draggingBlockIndex = i; blockDragMode = 0; 
                selectedBlockIndex = i; 
            }
        }

        if (draggingBlockIndex == (int)i && mouseDown && !isScrubbing) {
            float dragDeltaSecs = ImGui::GetIO().MouseDelta.x / pixelsPerSecond;
            if (blockDragMode == 0) {
                block.startTime += dragDeltaSecs;
                if (block.startTime < 0.0f) block.startTime = 0.0f; 
            } else if (blockDragMode == 1) {
                block.duration += dragDeltaSecs;
                if (block.duration < 0.1f) block.duration = 0.1f; 
            }
        }

        drawList->AddRectFilled(ImVec2(x1, y1), ImVec2(x2, y2), IM_COL32(70, 130, 180, 255), 4.0f);
        drawList->AddRectFilled(ImVec2(x2 - 8, y1), ImVec2(x2, y2), IM_COL32(255, 255, 255, 50), 4.0f, ImDrawFlags_RoundCornersRight);
        
        if (selectedBlockIndex == (int)i) {
            drawList->AddRect(ImVec2(x1, y1), ImVec2(x2, y2), IM_COL32(255, 255, 0, 255), 4.0f, 0, 2.0f);
        } else {
            drawList->AddRect(ImVec2(x1, y1), ImVec2(x2, y2), IM_COL32(255, 255, 255, 100), 4.0f);
        }
        
        drawList->AddText(ImVec2(x1 + 5, y1 + 7), IM_COL32(255, 255, 255, 255), block.animName.c_str());

        if (i > 0) {
            auto& prevBlock = previewObj.montage[i-1];
            float prevEnd = prevBlock.startTime + prevBlock.duration;
            
            if (block.startTime < prevEnd && block.startTime >= prevBlock.startTime) {
                float overlapStart = block.startTime;
                float overlapEnd = fminf(block.startTime + block.duration, prevEnd);
                
                block.blendInTime = overlapEnd - overlapStart; 
                
                float ox1 = canvasPos.x + (overlapStart * pixelsPerSecond) - viewOffsetX;
                float ox2 = canvasPos.x + (overlapEnd * pixelsPerSecond) - viewOffsetX;
                
                drawList->AddRectFilled(ImVec2(ox1, canvasPos.y), ImVec2(ox2, canvasPos.y + canvasSize.y), IM_COL32(255, 200, 0, 100));
                drawList->AddText(ImVec2(ox1 + 2, canvasPos.y + canvasSize.y - 15), IM_COL32(255, 200, 0, 255), "BLEND");
            }
        }
    }
    
    if (px >= canvasPos.x && px <= canvasPos.x + canvasSize.x) {
        drawList->AddLine(ImVec2(px, canvasPos.y), ImVec2(px, canvasPos.y + canvasSize.y), IM_COL32(0, 255, 0, 255), 2.0f);
        drawList->AddTriangleFilled(
            ImVec2(px - 8, canvasPos.y), 
            ImVec2(px + 8, canvasPos.y), 
            ImVec2(px, canvasPos.y + 12), 
            IM_COL32(0, 255, 0, 255)
        );
    }

    drawList->PopClipRect();

    ImGui::SetCursorScreenPos(canvasPos);
    ImGui::InvisibleButton("TimelineCanvasInteraction", canvasSize);

    ImGui::Spacing();
    ImGui::BeginChild("PropertyEditor", ImVec2(0, 0), true);
    
    // =========================================================
    // THE FIX: Strict type-casting (int) added to size()
    // =========================================================
    if (selectedBlockIndex >= 0 && selectedBlockIndex < (int)previewObj.montage.size()) {
        auto& b = previewObj.montage[selectedBlockIndex];
        ImGui::TextColored(ImVec4(1, 0.8f, 0.2f, 1), "Editing Root Offsets: %s", b.animName.c_str());
        ImGui::Separator();
        ImGui::Spacing();
        
        float inputWidth = ImGui::GetContentRegionAvail().x - 100.0f;
        ImGui::PushItemWidth(inputWidth > 100.0f ? inputWidth : 100.0f);
        
        ImGui::DragFloat3("Position", &b.positionOffset.x, 0.05f);
        ImGui::Spacing();
        ImGui::DragFloat3("Rotation", &b.rotationOffset.x, 1.0f);
        ImGui::Spacing();
        ImGui::DragFloat("Scale", &b.scaleOffset, 0.01f);
        
        ImGui::PopItemWidth();
        
        // --- NEW: DELETE SINGLE TRACK BUTTON ---
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("DELETE SELECTED TRACK", ImVec2(-1, 24))) {
            previewObj.montage.erase(previewObj.montage.begin() + selectedBlockIndex);
            selectedBlockIndex = -1; // Deselect after deleting
        }
        ImGui::PopStyleColor();
        // ---------------------------------------
        
    } else {
        ImGui::TextDisabled("Click a track on the timeline to edit its offsets...");
    }
    
    ImGui::EndChild();

    ImGui::EndChild();

    ImGui::SameLine();

    // --- RIGHT PANEL: PREVIEW & BAKE ---
    ImGui::BeginChild("RightPanel", ImVec2(0, 0), true);
    ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "5. Preview & Bake");
    ImGui::TextDisabled("WASD to Move | L-Click to Turn | Mid-Click to Reset");
    
    ImGui::SetNextItemWidth(250);
    ImGui::SliderFloat("Camera Zoom", &previewCamDist, 0.1f, 200.0f, "%.1fm Distance");
    
    float yaw = previewCamAngle.x * DEG2RAD;
    float pitch = previewCamAngle.y * DEG2RAD;

    previewCam.position = {
        previewCenter.x + previewCamOffset.x + previewCamDist * sinf(yaw) * cosf(pitch),
        previewCenter.y + previewCamOffset.y + previewCamDist * sinf(pitch),
        previewCenter.z + previewCamOffset.z + previewCamDist * cosf(yaw) * cosf(pitch)
    };
    previewCam.target = { previewCenter.x + previewCamOffset.x, previewCenter.y + previewCamOffset.y, previewCenter.z + previewCamOffset.z };

    bool boneMismatch = false;

    BeginTextureMode(previewTarget);
        ClearBackground(DARKGRAY);
        BeginMode3D(previewCam);
        DrawGrid(10, 1.0f);
        
        if (isBaseLoaded) {
            if (isPlaying && !isScrubbing) {
                playheadTime += GetFrameTime();
                if (playheadTime > 30.0f) playheadTime = 0.0f;
            }

            MontageBlock* activeBlock = nullptr;
            for (auto& b : previewObj.montage) {
                if (playheadTime >= b.startTime && playheadTime <= b.startTime + b.duration) {
                    activeBlock = &b; break;
                }
            }

            if (activeBlock) {
                ModelAnimation* targetAnim = nullptr;
                for (auto& aa : availableAnims) {
                    if (aa.sourcePath == activeBlock->sourcePath && aa.originalIndex == activeBlock->animIndex) {
                        targetAnim = aa.animPtr; break;
                    }
                }
                
                if (targetAnim && targetAnim->keyframeCount > 0) {
                    // Use Raylib's official skeleton compatibility check instead of
                    // manual boneCount comparison — IsModelAnimationValid is the
                    // documented way to guard UpdateModelAnimation in Raylib 4.5.
                    if (IsModelAnimationValid(previewObj.model, *targetAnim)) {
                        int localFrame = (int)((playheadTime - activeBlock->startTime) * 60.0f) % targetAnim->keyframeCount;
                        // Reset transform to identity BEFORE UpdateModelAnimation.
                        // Raylib bakes model.transform into bone world positions during
                        // the update — any dirty transform here causes bone explosion.
                        previewObj.model.transform = MatrixIdentity();
                        UpdateModelAnimation(previewObj.model, *targetAnim, localFrame);
                    } else {
                        boneMismatch = true;
                    }
                }

                // Apply block offsets via DrawModelEx — never via model.transform.
                // Putting translation/scale into model.transform and then calling
                // UpdateModelAnimation next frame is what causes spaghetti bones.
                Quaternion qRot = QuaternionFromEuler(
                    activeBlock->rotationOffset.x * DEG2RAD,
                    activeBlock->rotationOffset.y * DEG2RAD,
                    activeBlock->rotationOffset.z * DEG2RAD);
                Vector3 rotAxis; float rotAngle;
                QuaternionToAxisAngle(qRot, &rotAxis, &rotAngle);
                rotAngle *= RAD2DEG;
                float s = activeBlock->scaleOffset > 0.0f ? activeBlock->scaleOffset : 1.0f;
                DrawModelEx(previewObj.model, activeBlock->positionOffset, rotAxis, rotAngle, {s, s, s}, WHITE);
            } else {
                previewObj.model.transform = MatrixIdentity();
                DrawModel(previewObj.model, Vector3Zero(), 1.0f, WHITE);
            }
        }
        EndMode3D();
    EndTextureMode();

    ImVec2 previewRenderPos = ImGui::GetCursorScreenPos();
    ImGui::Image((ImTextureID)(intptr_t)previewTarget.texture.id, ImVec2(400, 300), ImVec2(0, 1), ImVec2(1, 0));
    
    ImGui::SetCursorScreenPos(previewRenderPos);
    ImGui::InvisibleButton("3DViewInteraction", ImVec2(400, 300));
    
    if (ImGui::IsItemActive()) {
        if (ImGui::IsMouseDragging(0)) {
            previewCamAngle.x -= ImGui::GetIO().MouseDelta.x * 0.5f;
            previewCamAngle.y += ImGui::GetIO().MouseDelta.y * 0.5f;
            if (previewCamAngle.y > 89.0f) previewCamAngle.y = 89.0f;
            if (previewCamAngle.y < -89.0f) previewCamAngle.y = -89.0f;
        }
    }

    if (ImGui::IsItemHovered()) {
        if (ImGui::IsMouseClicked(2)) { 
            previewCamOffset = {0.0f, 0.0f, 0.0f}; 
            previewCamAngle = {45.0f, 30.0f};
        }
        if (!ImGui::GetIO().WantTextInput) {
            float moveSpeed = 0.05f * (previewCamDist * 0.1f);
            if (moveSpeed < 0.05f) moveSpeed = 0.05f;

            if (IsKeyDown(KEY_W)) { previewCamOffset.x -= sinf(yaw) * moveSpeed; previewCamOffset.z -= cosf(yaw) * moveSpeed; }
            if (IsKeyDown(KEY_S)) { previewCamOffset.x += sinf(yaw) * moveSpeed; previewCamOffset.z += cosf(yaw) * moveSpeed; }
            if (IsKeyDown(KEY_A)) { previewCamOffset.x -= cosf(yaw) * moveSpeed; previewCamOffset.z += sinf(yaw) * moveSpeed; }
            if (IsKeyDown(KEY_D)) { previewCamOffset.x += cosf(yaw) * moveSpeed; previewCamOffset.z -= sinf(yaw) * moveSpeed; }
        }
    }

    if (boneMismatch) {
        ImGui::SetCursorScreenPos(ImVec2(previewRenderPos.x + 10, previewRenderPos.y + 10));
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "ERROR: BONE MISMATCH!");
        ImGui::SetCursorScreenPos(ImVec2(previewRenderPos.x + 10, previewRenderPos.y + 25));
        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "Parasite incompatible with base skeleton.");
    }

    ImGui::Spacing();
    ImGui::SetCursorScreenPos(ImVec2(previewRenderPos.x, previewRenderPos.y + 310));

    if (ImGui::Button(isPlaying ? "PAUSE PREVIEW" : "PLAY PREVIEW", ImVec2(-1, 30))) isPlaying = !isPlaying;
    
    // --- RE-ADDED: The Reset Time Button! ---
    if (ImGui::Button("RESET TIME TO 0s", ImVec2(-1, 24))) playheadTime = 0.0f;
    // ----------------------------------------
    
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::InputText("Prefab Name", saveNameBuf, 128);
    
    if (ImGui::Button("BAKE .NXCHAR PREFAB", ImVec2(-1, 40))) {
        if (isBaseLoaded && strlen(saveNameBuf) > 0) {
            std::string savePath = "Chars/" + std::string(saveNameBuf) + ".nxchar";
            SaveNexusCharacterPrefab(previewObj, savePath);
            needsRescan = true; 
            saveNameBuf[0] = '\0'; 
            
            // --- NEW: Auto-Clear Studio on Bake! ---
            ResetStudio();
            // ---------------------------------------
        }
    }

    ImGui::EndChild();
    ImGui::End();
}