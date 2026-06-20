#include "nexus_mocap_studio.h"
#include "imgui.h"
#include "rlImGui.h"
#include <fstream>
#include <cmath>
#include <cstdlib> 
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOGDI   
#define NOUSER  
#include <windows.h>
#undef near
#undef far
static HANDLE hMapFile = NULL;
#endif

NexusMocapStudio::NexusMocapStudio() {
    renderTarget = LoadRenderTexture(1024, 1024);
    previewCamera.position = { 0.0f, 1.2f, 2.5f };
    previewCamera.target = { 0.0f, 1.0f, 0.0f };
    previewCamera.up = { 0.0f, 1.0f, 0.0f };
    previewCamera.fovy = 60.0f;
    previewCamera.projection = CAMERA_PERSPECTIVE;

    memset(customMathCodeBuffer, 0, sizeof(customMathCodeBuffer));
    memset(saveNameBuffer, 0, sizeof(saveNameBuffer));
    strcpy(saveNameBuffer, "Custom_Mocap_01");

    const char* defaultCode = 
        "// === ADVANCED PROCEDURAL MATH CHARACTER ===\n"
        "Color myColor = { 0, 255, 120, 255 };\n"
        "Color fillMode = { 10, 40, 60, 200 };\n\n"
        "// 1. Calculate Core Body Vectors\n"
        "Vector3 shoulderMid = Vector3Scale(Vector3Add(frame.joints[0], frame.joints[1]), 0.5f);\n"
        "Vector3 hipMid = Vector3Scale(Vector3Add(frame.joints[6], frame.joints[7]), 0.5f);\n"
        "Vector3 spineDir = Vector3Normalize(Vector3Subtract(shoulderMid, hipMid));\n"
        "Vector3 shoulderVec = Vector3Normalize(Vector3Subtract(frame.joints[1], frame.joints[0]));\n"
        "Vector3 forward = Vector3Normalize(Vector3CrossProduct(shoulderVec, spineDir));\n\n"
        "// 2. Draw Cranium\n"
        "Vector3 headCenter = Vector3Add(frame.joints[12], Vector3Scale(forward, -0.08f));\n"
        "DrawSphere(headCenter, 0.09f, fillMode);\n"
        "DrawSphereWires(headCenter, 0.09f, 8, 8, myColor);\n\n"
        "// 3. Draw Procedural Ribcage\n"
        "for (int i = 0; i <= 6; i++) {\n"
        "    float lerp = (float)i / 6.0f;\n"
        "    Vector3 spinePt = Vector3Lerp(shoulderMid, hipMid, lerp);\n"
        "    float w = sinf(lerp * PI) * 0.15f;\n"
        "    float d = sinf(lerp * PI) * 0.1f;\n"
        "    Vector3 lEdge = Vector3Add(spinePt, Vector3Scale(shoulderVec, -w));\n"
        "    Vector3 rEdge = Vector3Add(spinePt, Vector3Scale(shoulderVec, w));\n"
        "    Vector3 sternum = Vector3Add(spinePt, Vector3Scale(forward, d));\n"
        "    DrawLine3D(lEdge, sternum, myColor);\n"
        "    DrawLine3D(rEdge, sternum, myColor);\n"
        "}\n";
    
    strncpy(customMathCodeBuffer, defaultCode, sizeof(customMathCodeBuffer) - 1);
}

NexusMocapStudio::~NexusMocapStudio() { 
    UnloadRenderTexture(renderTarget); 
}

void NexusMocapStudio::Init() {
#ifdef _WIN32
    hMapFile = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 256, "NexusMocapMemory");
    if (hMapFile != NULL) {
        sharedMemoryPtr = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 256);
    }
#endif
}

void NexusMocapStudio::Shutdown() {
#ifdef _WIN32
    if (sharedMemoryPtr) UnmapViewOfFile(sharedMemoryPtr);
    if (hMapFile) CloseHandle(hMapFile);
#endif
}

MocapFrame NexusMocapStudio::GetLiveFrame() {
    MocapFrame frame = {};
    if (sharedMemoryPtr) {
        float* data = (float*)sharedMemoryPtr;
        for (int i = 0; i < MOCAP_JOINT_COUNT; i++) {
            frame.joints[i].x = (data[(i * 3) + 0] * scale) + rootPosition.x;
            frame.joints[i].y = (data[(i * 3) + 1] * scale) + rootPosition.y;
            frame.joints[i].z = (data[(i * 3) + 2] * scale) + rootPosition.z;
        }
    }
    return frame;
}

void NexusMocapStudio::Update(float dt) {
    if (isRecording) {
        recordedSequence.push_back(GetLiveFrame());
        trimEnd = (int)recordedSequence.size() - 1; 
    } else if (isPlaying && !recordedSequence.empty()) {
        playheadTime += dt;
        float totalTime = (trimEnd - trimStart) / fps;
        if (totalTime <= 0) totalTime = 0.1f;
        if (playheadTime > totalTime) playheadTime = 0.0f; 
    }
    if (simulateAudioTalking) audioLevels = (sinf(GetTime() * 30.0f) * 0.5f + 0.5f) * ((float)(rand() % 100) / 100.0f);
    else audioLevels = 0.0f;
}

// THE FIX: Added overrideSkinID so main.cpp can borrow this function to draw on the timeline!
void NexusMocapStudio::RenderSkin(const MocapFrame& frame, int overrideSkinID) {
    float t = GetTime();
    int bones[14][2] = { {12, 0}, {12, 1}, {0, 1}, {6, 7}, {0, 6}, {1, 7}, {0, 2}, {2, 4}, {1, 3}, {3, 5}, {6, 8}, {8, 10}, {7, 9}, {9, 11} };
    DrawGrid(10, 1.0f);

    // Determines if we are drawing the studio's selected skin, or a skin forced by the timeline's track data
    MocapSkin skinToRender = (overrideSkinID != -1) ? (MocapSkin)overrideSkinID : activeSkin;

    if (skinToRender == MocapSkin::PROCEDURAL_HUMANOID) {
        Color out = { 40, 200, 255, 255 }; Color fill = { 10, 40, 60, 200 };
        Vector3 sMid = Vector3Scale(Vector3Add(frame.joints[0], frame.joints[1]), 0.5f);
        Vector3 hMid = Vector3Scale(Vector3Add(frame.joints[6], frame.joints[7]), 0.5f);
        Vector3 sDir = Vector3Normalize(Vector3Subtract(sMid, hMid));
        Vector3 sVec = Vector3Normalize(Vector3Subtract(frame.joints[1], frame.joints[0]));
        Vector3 fwd = Vector3Normalize(Vector3CrossProduct(sVec, sDir)); 

        for (int i = 0; i <= 6; i++) {
            float l = (float)i / 6;
            Vector3 pt = Vector3Lerp(sMid, hMid, l);
            float w = sinf(l * PI) * 0.15f; float d = sinf(l * PI) * 0.1f;
            DrawLine3D(Vector3Add(pt, Vector3Scale(sVec, -w)), Vector3Add(pt, Vector3Scale(fwd, d)), out);
            DrawLine3D(Vector3Add(pt, Vector3Scale(sVec, w)), Vector3Add(pt, Vector3Scale(fwd, d)), out);
        }

        auto DMusc = [&](Vector3 s, Vector3 e, float r1, float r2) { DrawCylinderEx(s, e, r1, r2, 8, fill); DrawCylinderWiresEx(s, e, r1, r2, 8, out); };
        DMusc(frame.joints[0], frame.joints[2], 0.04f, 0.03f); DMusc(frame.joints[2], frame.joints[4], 0.03f, 0.02f);
        DMusc(frame.joints[1], frame.joints[3], 0.04f, 0.03f); DMusc(frame.joints[3], frame.joints[5], 0.03f, 0.02f);
        DMusc(frame.joints[6], frame.joints[8], 0.06f, 0.04f); DMusc(frame.joints[8], frame.joints[10], 0.04f, 0.03f);
        DMusc(frame.joints[7], frame.joints[9], 0.06f, 0.04f); DMusc(frame.joints[9], frame.joints[11], 0.04f, 0.03f);

        Vector3 hc = Vector3Add(frame.joints[12], Vector3Scale(fwd, -0.08f));
        hc = Vector3Add(hc, Vector3Scale(sDir, 0.05f)); 
        DrawSphere(hc, 0.09f, fill); DrawSphereWires(hc, 0.09f, 8, 8, out); DrawLine3D(sMid, hc, out);

        float mOp = 0.005f + (audioLevels * 0.04f); 
        Vector3 uL = Vector3Add(frame.joints[12], Vector3Scale(sDir, -0.03f));
        Vector3 lL = Vector3Add(uL, Vector3Scale(sDir, -mOp));
        Vector3 mL = Vector3Add(uL, Vector3Add(Vector3Scale(sVec, -0.02f), Vector3Scale(sDir, -mOp * 0.5f)));
        Vector3 mR = Vector3Add(uL, Vector3Add(Vector3Scale(sVec, 0.02f), Vector3Scale(sDir, -mOp * 0.5f)));
        DrawTriangle3D(mL, lL, mR, RED); DrawLine3D(mL, uL, out); DrawLine3D(uL, mR, out); DrawLine3D(mR, lL, out); DrawLine3D(lL, mL, out);
    }
    else if (skinToRender == MocapSkin::CUSTOM_MATH_CODE) {
        Color ghost = { 0, 255, 120, 80 };
        for (int i = 0; i < MOCAP_JOINT_COUNT; i++) DrawSphereWires(frame.joints[i], 0.03f, 4, 4, ghost);
        for (int i = 0; i < 14; i++) DrawLine3D(frame.joints[bones[i][0]], frame.joints[bones[i][1]], ghost);
    } 
    else if (skinToRender == MocapSkin::TACTICAL_VOXEL) {
        Color coreColor = { 180, 20, 30, 255 }; 
        float pulse = 0.08f + (cosf(t * 8.0f) * 0.02f);
        for (int i = 0; i < MOCAP_JOINT_COUNT; i++) {
            DrawCube(frame.joints[i], pulse, pulse, pulse, DARKGRAY);
            DrawCubeWires(frame.joints[i], pulse + 0.01f, pulse + 0.01f, pulse + 0.01f, coreColor);
        }
        for (int i = 0; i < 14; i++) {
            Vector3 start = frame.joints[bones[i][0]];
            Vector3 end = frame.joints[bones[i][1]];
            DrawCube(Vector3Scale(Vector3Add(start, end), 0.5f), 0.04f, Vector3Distance(start, end), 0.04f, { 40, 40, 40, 255 });
        }
    } 
    else if (skinToRender == MocapSkin::GRAPHENE_CARBON) {
        Color carbonBase = { 15, 15, 18, 255 };
        Color neonWire = { 0, 255, 150, 150 }; 
        Color jointNode = { 30, 30, 30, 255 };

        for (int i = 0; i < 14; i++) {
            Vector3 start = frame.joints[bones[i][0]];
            Vector3 end = frame.joints[bones[i][1]];
            
            DrawCylinderEx(start, end, 0.02f, 0.02f, 6, carbonBase);
            DrawCylinderWiresEx(start, end, 0.025f, 0.025f, 5, neonWire);
        }
        
        for (int i = 0; i < MOCAP_JOINT_COUNT; i++) {
            DrawSphere(frame.joints[i], 0.035f, jointNode);
            DrawSphereWires(frame.joints[i], 0.04f, 6, 6, neonWire);
        }
    }
    else if (skinToRender == MocapSkin::TARTARIAN_RESONATOR) {
        Color copper = { 184, 115, 51, 255 };
        for (int i = 0; i < 14; i++) {
            Vector3 prev = frame.joints[bones[i][0]];
            for (int s = 1; s <= 5; s++) {
                float lerpT = (float)s / 5.0f;
                Vector3 basePt = Vector3Lerp(frame.joints[bones[i][0]], frame.joints[bones[i][1]], lerpT);
                float arc = tanf(t * 15.0f + (s * i)) * 0.02f;
                if (arc > 0.05f) { arc = 0.05f; }
                if (arc < -0.05f) { arc = -0.05f; }
                DrawLine3D(prev, { basePt.x + arc, basePt.y + arc, basePt.z }, copper);
                prev = { basePt.x + arc, basePt.y + arc, basePt.z };
            }
        }
        for (int i = 0; i < MOCAP_JOINT_COUNT; i++) DrawSphereWires(frame.joints[i], 0.03f, 4, 4, {255, 200, 100, 255});
    }
}

void NexusMocapStudio::UpdateViewport() {
    if (isViewportHovered && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) { isCameraDragging = true; DisableCursor(); }
    if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) { isCameraDragging = false; EnableCursor(); }

    if (isCameraDragging) {
        Vector2 delta = GetMouseDelta();
        Vector3 movement = { 0.0f, 0.0f, 0.0f };
        if (IsKeyDown(KEY_W)) movement.x += 0.05f; 
        if (IsKeyDown(KEY_S)) movement.x -= 0.05f;
        if (IsKeyDown(KEY_D)) movement.y += 0.05f; 
        if (IsKeyDown(KEY_A)) movement.y -= 0.05f;
        Vector3 rotation = { delta.x * 0.15f, delta.y * 0.15f, 0.0f };
        UpdateCameraPro(&previewCamera, movement, rotation, 0.0f);
    }

    BeginTextureMode(renderTarget);
    ClearBackground({25, 25, 30, 255});
    BeginMode3D(previewCamera);
    
    if (isPlaying && !recordedSequence.empty()) {
        int frameIndex = trimStart + (int)(playheadTime * fps);
        if (frameIndex > trimEnd) frameIndex = trimEnd;
        if (frameIndex >= (int)recordedSequence.size()) frameIndex = (int)recordedSequence.size() - 1;
        
        // Pass -1 so it uses the Studio's activeSkin variable
        RenderSkin(recordedSequence[frameIndex], -1);
    } else {
        // Pass -1 so it uses the Studio's activeSkin variable
        RenderSkin(GetLiveFrame(), -1);
    }
    
    EndMode3D();
    EndTextureMode();
}

void NexusMocapStudio::DrawUI(bool& isOpen) {
    if (!isOpen) return;
    ImGui::SetNextWindowSize(ImVec2(1100, 750), ImGuiCond_FirstUseEver);
    ImGui::Begin("Mo Cap Plugin Studio", &isOpen);

    float leftColumnWidth = ImGui::GetWindowWidth() * 0.45f; 
    ImGui::Columns(2, "MocapCols", false);
    ImGui::SetColumnWidth(0, leftColumnWidth);

    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.4f, 1.0f), "Private 3D Viewport");
    float viewWidth = ImGui::GetColumnWidth() - 15.0f; 
    float viewHeight = viewWidth; 
    Rectangle viewRect = { 0, 0, (float)renderTarget.texture.width, -(float)renderTarget.texture.height };
    
    ImVec2 startPos = ImGui::GetCursorScreenPos();
    rlImGuiImageRect(&renderTarget.texture, (int)viewWidth, (int)viewHeight, viewRect);
    ImGui::SetCursorScreenPos(startPos);
    ImGui::InvisibleButton("##MocapViewportHitbox", ImVec2(viewWidth, viewHeight));
    isViewportHovered = ImGui::IsItemHovered();

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Studio Recorder & Trimmer");
    if (isRecording) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("STOP RECORDING", ImVec2(-1, 40))) { isRecording = false; isPlaying = true; }
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("START RECORDING", ImVec2(-1, 40))) { recordedSequence.clear(); isRecording = true; isPlaying = false; }
        ImGui::PopStyleColor();
    }

    if (!recordedSequence.empty()) {
        ImGui::Spacing();
        ImGui::SliderInt("Trim Start", &trimStart, 0, recordedSequence.size() - 1);
        ImGui::SliderInt("Trim End", &trimEnd, 0, recordedSequence.size() - 1);
        if (trimStart > trimEnd) trimStart = trimEnd;
        if (ImGui::Button(isPlaying ? "Pause Playback" : "Play Trimmed Recording", ImVec2(-1, 30))) isPlaying = !isPlaying;
    }

    ImGui::NextColumn();

    if (ImGui::Button("LAUNCH PYTHON WEBCAM", ImVec2(-1, 30))) system("start cmd /k python mocap_shared.py"); 
    
    if (!sharedMemoryPtr) {
        if (ImGui::Button("Connect to Tracker", ImVec2(-1, 30))) Init();
    } else {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "STATUS: CONNECTED & RECEIVING");
    }
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "MoCap Object Target");
    int s = (int)activeSkin;
    ImGui::RadioButton("Procedural Humanoid (AAA)", &s, 4);
    ImGui::RadioButton("Custom Raylib Math Code", &s, 3);
    ImGui::RadioButton("Tartarian Resonator", &s, 2);
    ImGui::RadioButton("Graphene Carbon", &s, 1);
    ImGui::RadioButton("Tactical Voxel", &s, 0);
    activeSkin = (MocapSkin)s;

    if (activeSkin == MocapSkin::PROCEDURAL_HUMANOID) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
        ImGui::Checkbox("Simulate Audio Talking", &simulateAudioTalking);
        ImGui::PopStyleColor();
    } else if (activeSkin == MocapSkin::CUSTOM_MATH_CODE) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.5f, 1.0f), "Paste C++ Raylib Math Character Below:");
        ImGui::InputTextMultiline("##CustomMath", customMathCodeBuffer, IM_ARRAYSIZE(customMathCodeBuffer), ImVec2(-1, 200));
        ImGui::Checkbox("Snap motion to Interactive Object", &snapToInteractiveObj);
    }

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    ImGui::InputText("Asset Name", saveNameBuffer, IM_ARRAYSIZE(saveNameBuffer));
    
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.6f, 0.2f, 1.0f));
    if (ImGui::Button("SAVE AS .NXMOCAP ASSET", ImVec2(-1, 35))) {
        if (!recordedSequence.empty()) {
            outputTrack.trackName = saveNameBuffer;
            outputTrack.frames = recordedSequence;
            outputTrack.trimStartFrame = trimStart;
            outputTrack.trimEndFrame = trimEnd;
            outputTrack.customMathCode = customMathCodeBuffer;
            outputTrack.snapToInteractiveObject = snapToInteractiveObj;
            outputTrack.skinID = (int)activeSkin; 

            std::string path = "Chars/" + std::string(saveNameBuffer) + ".nxmocap";
            outputTrack.SaveToFile(path);
            
            requestAssetRescan = true;
            
            studioFeedback = "Successfully saved " + std::string(saveNameBuffer) + ".nxmocap!";
            studioFeedbackTimer = 3.0f;
        } else {
            studioFeedback = "Error: Nothing recorded yet!";
            studioFeedbackTimer = 3.0f;
        }
    }
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.6f, 1.0f));
    if (ImGui::Button("SEND TO MAIN TIMELINE", ImVec2(-1, 35))) {
        if (!recordedSequence.empty()) {
            outputTrack.frames = recordedSequence;
            outputTrack.trimStartFrame = trimStart;
            outputTrack.trimEndFrame = trimEnd;
            outputTrack.customMathCode = customMathCodeBuffer;
            outputTrack.snapToInteractiveObject = snapToInteractiveObj;
            outputTrack.skinID = (int)activeSkin; 
            
            // This flag is caught in main.cpp, triggering the array push and rendering on the timeline!
            requestSaveToTimeline = true;
            
            studioFeedback = "Track sent to Main Timeline viewport!";
            studioFeedbackTimer = 3.0f;
        } else {
            studioFeedback = "Error: Record a sequence first!";
            studioFeedbackTimer = 3.0f;
        }
    }
    ImGui::PopStyleColor();

    if (studioFeedbackTimer > 0.0f) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.3f, 1.0f), ">> %s", studioFeedback.c_str());
        studioFeedbackTimer -= GetFrameTime();
    }

    ImGui::Columns(1);
    ImGui::End();
}