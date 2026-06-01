#include "raylib.h"
#include "raymath.h" 
#include "nexus_core.h"
#include "nexus_3dviewportcontrols.h"
#include "nexus_environment.h"
#include "nexus_character.h"
#include "file_dialog.h"
#include "nexus_timeline.h" 
#include "nexus_project.h"  
#include "nexus_asset_browser.h" 
#include "nexus_win32.h"         
#include "nexus_asset_scanner.h" 
#include "rlImGui.h"        
#include "imgui.h"          
#include <string>
#include <vector>
#include <algorithm>

std::vector<Asset> envLibrary;
std::vector<Asset> charLibrary;
std::vector<SceneObject> scene;
std::vector<Vector3> sceneRotations; 

int selectedObjectIndex = -1;
enum TransformMode { MODE_MOVE, MODE_ROTATE, MODE_SCALE };
TransformMode currentMode = MODE_MOVE;

std::string CleanAssetName(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    std::string name = (pos != std::string::npos) ? path.substr(pos + 1) : path;
    size_t ext = name.find_last_of(".");
    if (ext != std::string::npos) name = name.substr(0, ext);
    std::replace(name.begin(), name.end(), '_', ' '); 
    return name;
}

int main() {
    InitWindow(1280, 720, "Nexus 3D - NLE Engine (Final Layout)");
    Camera camera = { { 15.0f, 25.0f, 15.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, 45.0f, 0 };
    SetTargetFPS(60);

    ScanForAssets(envLibrary, charLibrary);

    rlImGuiSetup(true);
    NexusTimeline timeline;
    NexusAssetBrowser assetBrowser; 
    
    int globalFrame = 0;
    bool isPreviewing = false;
    SceneObject previewObject;

    bool showAssetBrowser = true;
    bool showTimeline = true; 
    bool shouldExit = false;

    while (!WindowShouldClose() && !shouldExit) {
        
        if (timeline.isPlaying) {
            globalFrame++;
            if (globalFrame >= timeline.maxTimelineFrames) globalFrame = 0; 
        }

        bool isMouseInViewport = !ImGui::GetIO().WantCaptureMouse;
        bool suppressCamera = false; 

        if (selectedObjectIndex != -1) {
            if (currentMode == MODE_MOVE) {
                bool arrowPressed = IsKeyDown(KEY_UP) || IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_RIGHT);
                if (arrowPressed) {
                    float moveSpeed = 0.2f;
                    if (IsKeyDown(KEY_UP)) scene[selectedObjectIndex].position.z -= moveSpeed;
                    if (IsKeyDown(KEY_DOWN)) scene[selectedObjectIndex].position.z += moveSpeed;
                    if (IsKeyDown(KEY_LEFT)) scene[selectedObjectIndex].position.x -= moveSpeed;
                    if (IsKeyDown(KEY_RIGHT)) scene[selectedObjectIndex].position.x += moveSpeed;
                    suppressCamera = true; 
                }
            }
            
            if (isMouseInViewport && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                Vector2 delta = GetMouseDelta();
                if (currentMode == MODE_ROTATE) {
                    sceneRotations[selectedObjectIndex].y += delta.x * 0.5f; 
                    suppressCamera = true; 
                } 
                else if (currentMode == MODE_SCALE) {
                    scene[selectedObjectIndex].scale += delta.x * 0.01f;
                    if (scene[selectedObjectIndex].scale < 0.01f) scene[selectedObjectIndex].scale = 0.01f;
                    suppressCamera = true; 
                }
            }
        }

        if (isMouseInViewport && !suppressCamera) {
            UpdateViewportCamera(camera);
        }

        Vector3 mouseGroundPos = { 0.0f, 0.0f, 0.0f };
        Ray mouseRay = GetMouseRay(GetMousePosition(), camera);
        if (mouseRay.direction.y != 0.0f) {
            float t = -mouseRay.position.y / mouseRay.direction.y;
            if (t > 0.0f) mouseGroundPos = { mouseRay.position.x + mouseRay.direction.x * t, 0.0f, mouseRay.position.z + mouseRay.direction.z * t };
        }

        if (isPreviewing) {
            previewObject.position = mouseGroundPos;

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && isMouseInViewport) {
                scene.push_back(previewObject);
                sceneRotations.push_back({0.0f, 0.0f, 0.0f}); 
                
                selectedObjectIndex = scene.size() - 1; 
                isPreviewing = false; 

                int frames = 0;
                bool isEnv = (previewObject.name == "Environment"); 
                if (previewObject.isAnimated && previewObject.anims != nullptr) frames = previewObject.anims[0].keyframeCount;
                
                std::string trackName = CleanAssetName(previewObject.filePath);
                timeline.AddTrack(trackName, frames, isEnv);
                timeline.tracks.back().startFrame = globalFrame;
                timeline.tracks.back().endFrame = globalFrame + (frames > 0 ? frames : 300);
            }
            else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                UnloadModel(previewObject.model);
                if (previewObject.isAnimated && previewObject.anims != nullptr) UnloadModelAnimations(previewObject.anims, previewObject.animCount);
                isPreviewing = false;
            }
        } else {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && isMouseInViewport) {
                float closestDist = 99999.0f;
                int hitIndex = -1;

                for (size_t i = 0; i < scene.size(); i++) {
                    Vector3 extents = {
                        ((scene[i].bounds.max.x - scene[i].bounds.min.x) / 2.0f) * scene[i].scale,
                        ((scene[i].bounds.max.y - scene[i].bounds.min.y) / 2.0f) * scene[i].scale,
                        ((scene[i].bounds.max.z - scene[i].bounds.min.z) / 2.0f) * scene[i].scale
                    };
                    BoundingBox worldBox = {
                        { scene[i].position.x - extents.x, scene[i].position.y, scene[i].position.z - extents.z },
                        { scene[i].position.x + extents.x, scene[i].position.y + extents.y * 2.0f, scene[i].position.z + extents.z }
                    };

                    RayCollision collision = GetRayCollisionBox(mouseRay, worldBox);
                    if (collision.hit && collision.distance < closestDist) {
                        closestDist = collision.distance;
                        hitIndex = i;
                    }
                }
                
                if (hitIndex != -1) selectedObjectIndex = hitIndex;
            }

            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && isMouseInViewport) {
                selectedObjectIndex = -1; 
            }
        }

        BeginDrawing();
        ClearBackground(DARKGRAY);

        BeginMode3D(camera);
            DrawGrid(20, 1.0f);
            
            for (size_t i = 0; i < scene.size(); i++) {
                auto& obj = scene[i];
                auto& track = timeline.tracks[i]; 

                if (globalFrame >= track.startFrame && globalFrame <= track.endFrame) {
                    
                    if (obj.isAnimated && obj.anims != nullptr && track.sourceDuration > 0) {
                        int tPoseSkip = (track.sourceDuration > 60) ? 60 : 0;
                        int usableFrames = track.sourceDuration - tPoseSkip;
                        int playheadOffset = (globalFrame - track.startFrame) % usableFrames;
                        int localFrame = tPoseSkip + playheadOffset;
                        UpdateModelAnimation(obj.model, obj.anims[0], localFrame);
                    }
                    
                    Vector3 pivot = {
                        (obj.bounds.max.x + obj.bounds.min.x) / 2.0f,
                        obj.bounds.min.y, 
                        (obj.bounds.max.z + obj.bounds.min.z) / 2.0f
                    };

                    Matrix tPivot = MatrixTranslate(-pivot.x, -pivot.y, -pivot.z);
                    Matrix tScale = MatrixScale(obj.scale, obj.scale, obj.scale);
                    Matrix tRot = MatrixRotateXYZ({ DEG2RAD * sceneRotations[i].x, DEG2RAD * sceneRotations[i].y, DEG2RAD * sceneRotations[i].z });
                    Matrix tTrans = MatrixTranslate(obj.position.x, obj.position.y, obj.position.z);

                    Matrix m1 = MatrixMultiply(tPivot, tScale);
                    Matrix m2 = MatrixMultiply(m1, tRot);
                    obj.model.transform = MatrixMultiply(m2, tTrans);

                    DrawModel(obj.model, Vector3Zero(), 1.0f, WHITE);

                    if ((int)i == selectedObjectIndex) {
                        Vector3 extents = {
                            ((obj.bounds.max.x - obj.bounds.min.x) / 2.0f) * obj.scale,
                            ((obj.bounds.max.y - obj.bounds.min.y) / 2.0f) * obj.scale,
                            ((obj.bounds.max.z - obj.bounds.min.z) / 2.0f) * obj.scale
                        };
                        BoundingBox worldBox = {
                            { obj.position.x - extents.x, obj.position.y, obj.position.z - extents.z },
                            { obj.position.x + extents.x, obj.position.y + extents.y * 2.0f, obj.position.z + extents.z }
                        };
                        DrawBoundingBox(worldBox, GREEN);
                    }
                }
            }

            if (isPreviewing) {
                Vector3 pivot = {
                    (previewObject.bounds.max.x + previewObject.bounds.min.x) / 2.0f,
                    previewObject.bounds.min.y,
                    (previewObject.bounds.max.z + previewObject.bounds.min.z) / 2.0f
                };

                Matrix tPivot = MatrixTranslate(-pivot.x, -pivot.y, -pivot.z);
                Matrix tScale = MatrixScale(previewObject.scale, previewObject.scale, previewObject.scale);
                Matrix tTrans = MatrixTranslate(previewObject.position.x, previewObject.position.y, previewObject.position.z);
                previewObject.model.transform = MatrixMultiply(MatrixMultiply(tPivot, tScale), tTrans);

                DrawModel(previewObject.model, Vector3Zero(), 1.0f, Fade(WHITE, 0.5f));
            }
        EndMode3D();

        rlImGuiBegin();

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Open Project...")) {
                    std::string openPath = OpenNexusFileDialog();
                    if (!openPath.empty()) {
                        selectedObjectIndex = -1; 
                        LoadNexusProject(openPath, scene, sceneRotations, timeline);
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Save Project As...")) {
                    std::string savePath = SaveNexusFileDialog();
                    if (!savePath.empty()) {
                        SaveNexusProject(savePath, scene, sceneRotations, timeline);
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit")) {
                    shouldExit = true;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Import")) {
                if (ImGui::MenuItem("Environment Importer")) {
                    std::string path = OpenGLTFFileDialog();
                    if (!path.empty()) {
                        std::replace(path.begin(), path.end(), '\\', '/');
                        envLibrary.push_back({CleanAssetName(path), path, GenerateThumbnail(path, true)});
                    }
                }
                if (ImGui::MenuItem("Character Importer")) {
                    std::string path = OpenGLTFFileDialog();
                    if (!path.empty()) {
                        std::replace(path.begin(), path.end(), '\\', '/');
                        charLibrary.push_back({CleanAssetName(path), path, GenerateThumbnail(path, false)});
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Window")) {
                ImGui::MenuItem("Asset Browser", NULL, &showAssetBrowser);
                ImGui::MenuItem("Sequencer Timeline", NULL, &showTimeline);
                ImGui::Separator();
                if (ImGui::MenuItem("Rescan Asset Folders")) {
                    ScanForAssets(envLibrary, charLibrary);
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        assetBrowser.Draw(showAssetBrowser, envLibrary, charLibrary, previewObject, isPreviewing);

        if (showTimeline) {
            // FIX: Match the exact width of the new Asset Browser so they never overlap
            float browserWidth = showAssetBrowser ? 300.0f : 0.0f;
            timeline.Draw(globalFrame, browserWidth);
        }

        if (selectedObjectIndex != -1) {
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            float inspectorWidth = 320.0f;
            
            // FIX: Dock cleanly to WorkPos.y so it stays just under the top menu
            ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - inspectorWidth, viewport->WorkPos.y), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(inspectorWidth, 240), ImGuiCond_Always);
            
            ImGui::Begin("Transform Inspector", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
            
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Selected: %s", timeline.tracks[selectedObjectIndex].assetName.c_str());
            ImGui::Separator();

            int mode = (int)currentMode;
            ImGui::RadioButton("Move", &mode, 0); ImGui::SameLine();
            ImGui::RadioButton("Rotate", &mode, 1); ImGui::SameLine();
            ImGui::RadioButton("Scale", &mode, 2);
            currentMode = (TransformMode)mode;

            ImGui::Separator();
            if (currentMode == MODE_MOVE) ImGui::TextDisabled("Use Arrow Keys to move.");
            else ImGui::TextDisabled("Click & Drag mouse in viewport to modify.");

            ImGui::DragFloat3("Position##obj", &scene[selectedObjectIndex].position.x, 0.1f);
            ImGui::DragFloat3("Rotation##obj", &sceneRotations[selectedObjectIndex].x, 1.0f);
            ImGui::DragFloat("Scale##obj", &scene[selectedObjectIndex].scale, 0.01f);
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button("SAVE TRANSFORM", ImVec2(-1, 35))) {
                selectedObjectIndex = -1; 
            }
            
            ImGui::End();
        }

        rlImGuiEnd();
        EndDrawing();
    }

    for (auto& asset : envLibrary) UnloadTexture(asset.thumbnail);
    for (auto& asset : charLibrary) UnloadTexture(asset.thumbnail);
    for (auto& obj : scene) {
        UnloadModel(obj.model);
        if (obj.isAnimated && obj.anims != nullptr) UnloadModelAnimations(obj.anims, obj.animCount);
    }
    
    rlImGuiShutdown();
    CloseWindow();
    return 0;
}