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
#include "nexus_texture_extractor.h"
#include "nexus_anim_selector.h" 
#include "nexus_waypoint.h" 
#include "nexus_prefab.h" 
#include "nexus_animator.h" 
#include "nexus_character_creator.h"
#include "nexus_scene_manager.h"
#include "nexus_manual_importer.h" // <-- New master importer header
#include "rlImGui.h"        
#include "imgui.h"          
#include <string>
#include <vector>
#include <algorithm>
#include <cstring> 

std::vector<Asset> envLibrary;
std::vector<Asset> charLibrary;
std::vector<SceneObject> scene;
std::vector<Vector3> sceneRotations; 

int selectedObjectIndex = -1;
enum TransformMode { MODE_MOVE, MODE_ROTATE, MODE_SCALE };
TransformMode currentMode = MODE_MOVE;
bool isPlottingWaypoints = false; 

std::string CleanAssetName(const std::string& path) {
    std::string p = path;
    std::replace(p.begin(), p.end(), '\\', '/');
    size_t pos = p.find_last_of('/');
    std::string filename = (pos != std::string::npos) ? p.substr(pos + 1) : p;
    std::string name = filename;
    size_t extPos = name.find_last_of('.');
    if (extPos != std::string::npos) name = name.substr(0, extPos);
    if (name == "scene" && pos != std::string::npos) {
        std::string parentPath = p.substr(0, pos);
        size_t parentPos = parentPath.find_last_of('/');
        name = (parentPos != std::string::npos) ? parentPath.substr(parentPos + 1) : parentPath;
    }
    std::replace(name.begin(), name.end(), '_', ' '); 
    return name;
}

int main() {
    InitWindow(1280, 720, "Nexus 3D - Native glTF Importer");
    Camera camera = { { 15.0f, 25.0f, 15.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, 45.0f, 0 };
    SetTargetFPS(60);

    ScanForAssets(envLibrary, charLibrary);
    rlImGuiSetup(true);
    NexusTimeline timeline;
    NexusAssetBrowser assetBrowser; 
    
    NexusAnimator animator;
    bool showAnimator = false;

    NexusCharacterCreator charCreator;
    bool showCharCreator = false;

    NexusSceneManager sceneManager;
    bool showSceneManager = false;

    // --> NEW INSTANCE
    NexusManualImporter manualImporter;
    bool showManualImporter = false;
    
    int globalFrame = 0;
    bool isPreviewing = false;
    SceneObject previewObject;
    bool showAssetBrowser = true;
    bool showTimeline = true; 
    bool shouldExit = false;

    while (!WindowShouldClose() && !shouldExit) {
        
        if (timeline.isPlaying) {
            globalFrame++;
            if (globalFrame >= timeline.maxTimelineFrames) {
                globalFrame = 0; 
            }
        }

        bool isMouseInViewport = !ImGui::GetIO().WantCaptureMouse;
        bool suppressCamera = false; 

        if (selectedObjectIndex != -1 && !isPlottingWaypoints) {
            if (currentMode == MODE_MOVE) {
                bool arrowPressed = IsKeyDown(KEY_UP) || IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_RIGHT);
                if (arrowPressed) {
                    float moveSpeed = 0.2f;
                    Vector3 delta = {0.0f, 0.0f, 0.0f};
                    
                    if (IsKeyDown(KEY_UP)) delta.z -= moveSpeed;
                    if (IsKeyDown(KEY_DOWN)) delta.z += moveSpeed;
                    if (IsKeyDown(KEY_LEFT)) delta.x -= moveSpeed;
                    if (IsKeyDown(KEY_RIGHT)) delta.x += moveSpeed;

                    if (delta.x != 0.0f || delta.z != 0.0f) {
                        scene[selectedObjectIndex].position.x += delta.x;
                        scene[selectedObjectIndex].position.z += delta.z;
                        for (auto& wp : scene[selectedObjectIndex].waypoints) {
                            wp.x += delta.x; wp.z += delta.z;
                        }
                        suppressCamera = true; 
                    }
                }
            }
            
            if (isMouseInViewport && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                Vector2 delta = GetMouseDelta();
                if (currentMode == MODE_ROTATE) {
                    sceneRotations[selectedObjectIndex].y += delta.x * 0.5f; 
                    suppressCamera = true; 
                } else if (currentMode == MODE_SCALE) {
                    scene[selectedObjectIndex].scale += delta.x * 0.01f;
                    if (scene[selectedObjectIndex].scale < 0.01f) scene[selectedObjectIndex].scale = 0.01f;
                    suppressCamera = true; 
                }
            }
        }

        if (isMouseInViewport && !suppressCamera) UpdateViewportCamera(camera);

        Vector3 mouseGroundPos = { 0.0f, 0.0f, 0.0f };
        Ray mouseRay = GetMouseRay(GetMousePosition(), camera);
        if (mouseRay.direction.y != 0.0f) {
            float t = -mouseRay.position.y / mouseRay.direction.y;
            if (t > 0.0f) mouseGroundPos = { mouseRay.position.x + mouseRay.direction.x * t, 0.0f, mouseRay.position.z + mouseRay.direction.z * t };
        }

        bool isSnapping = false;
        if (isPlottingWaypoints && selectedObjectIndex != -1 && !scene[selectedObjectIndex].waypoints.empty()) {
            Vector3 startNode = scene[selectedObjectIndex].waypoints[0];
            Vector3 testDir = Vector3Subtract(mouseGroundPos, startNode);
            testDir.y = 0;
            if (Vector3Length(testDir) < 1.5f) { 
                mouseGroundPos = startNode;
                isSnapping = true;
            }
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
                
                if (!previewObject.montage.empty()) {
                    float maxTime = 0.0f;
                    for (auto& b : previewObject.montage) {
                        if (b.startTime + b.duration > maxTime) maxTime = b.startTime + b.duration;
                    }
                    frames = (int)(maxTime * 60.0f); 
                    if (frames < 1) frames = 60; 
                } 
                else if (previewObject.isAnimated && previewObject.anims != nullptr) {
                    frames = previewObject.anims[0].keyframeCount;
                }
                
                std::string trackName = CleanAssetName(previewObject.filePath);
                timeline.AddTrack(trackName, frames, isEnv);
                timeline.tracks.back().startFrame = globalFrame;
                timeline.tracks.back().endFrame = globalFrame + (frames > 0 ? frames : 300);
            }
            else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                UnloadModel(previewObject.model);
                if (previewObject.isAnimated && previewObject.anims != nullptr) UnloadModelAnimations(previewObject.anims, previewObject.animCount);
                for(size_t i=0; i<previewObject.parasiteAnims.size(); i++) UnloadModelAnimations(previewObject.parasiteAnims[i], previewObject.parasiteAnimCounts[i]);
                isPreviewing = false;
            }
        } 
        else if (isPlottingWaypoints && selectedObjectIndex != -1) {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && isMouseInViewport) {
                scene[selectedObjectIndex].waypoints.push_back(mouseGroundPos);
                if (isSnapping) {
                    scene[selectedObjectIndex].loopWaypoints = true;
                    isPlottingWaypoints = false; 
                }
            }
            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && isMouseInViewport) isPlottingWaypoints = false; 
        } 
        else {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && isMouseInViewport) {
                float closestDist = 99999.0f;
                int hitIndex = -1;
                for (size_t i = 0; i < scene.size(); i++) {
                    Vector3 extents = { ((scene[i].bounds.max.x - scene[i].bounds.min.x) / 2.0f) * scene[i].scale, ((scene[i].bounds.max.y - scene[i].bounds.min.y) / 2.0f) * scene[i].scale, ((scene[i].bounds.max.z - scene[i].bounds.min.z) / 2.0f) * scene[i].scale };
                    BoundingBox worldBox = { { scene[i].position.x - extents.x, scene[i].position.y, scene[i].position.z - extents.z }, { scene[i].position.x + extents.x, scene[i].position.y + extents.y * 2.0f, scene[i].position.z + extents.z } };
                    RayCollision collision = GetRayCollisionBox(mouseRay, worldBox);
                    if (collision.hit && collision.distance < closestDist) {
                        closestDist = collision.distance; hitIndex = i;
                    }
                }
                if (hitIndex != -1) selectedObjectIndex = hitIndex;
            }
            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && isMouseInViewport) selectedObjectIndex = -1; 
        }

        // --> RENDER FBO PREVIEW OUTSIDE OF IMGUI
        manualImporter.UpdateViewport(showManualImporter);

        BeginDrawing();
        ClearBackground(DARKGRAY);

        BeginMode3D(camera);
            DrawGrid(20, 1.0f);
            
            if (selectedObjectIndex != -1) {
                DrawWaypointPath(scene[selectedObjectIndex]);
                if (isPlottingWaypoints) {
                    if (!scene[selectedObjectIndex].waypoints.empty()) {
                        DrawLine3D(scene[selectedObjectIndex].waypoints.back(), mouseGroundPos, Fade(WHITE, 0.5f));
                    }
                    if (isSnapping) {
                        DrawSphere(mouseGroundPos, 0.3f, Fade(YELLOW, 0.8f));
                        DrawSphereWires(mouseGroundPos, 0.35f, 8, 8, ORANGE); 
                    } else {
                        DrawSphere(mouseGroundPos, 0.15f, Fade(WHITE, 0.5f));
                    }
                }
            }
            
            for (size_t i = 0; i < scene.size(); i++) {
                auto& obj = scene[i];
                auto& track = timeline.tracks[i]; 

                if (globalFrame >= track.startFrame && (globalFrame <= track.endFrame || track.stayOnScene)) {
                    
                    UpdateCharacterWaypoints(obj, sceneRotations[i], globalFrame, track.startFrame);

                    Matrix blockTransform = MatrixIdentity();

                    if (!obj.montage.empty()) {
                        float maxTime = 0.0f;
                        for (auto& b : obj.montage) {
                            if (b.startTime + b.duration > maxTime) maxTime = b.startTime + b.duration;
                        }
                        int montageLenFrames = (int)(maxTime * 60.0f);
                        if (montageLenFrames < 1) montageLenFrames = 1;

                        float localTime = (float)((globalFrame - track.startFrame) % montageLenFrames) / 60.0f; 
                        
                        MontageBlock* activeBlock = nullptr;
                        for (auto& b : obj.montage) {
                            if (localTime >= b.startTime && localTime <= b.startTime + b.duration) {
                                activeBlock = &b; break;
                            }
                        }
                        
                        if (activeBlock) {
                            ModelAnimation* targetAnim = nullptr;
                            for (size_t p = 0; p < obj.parasitePaths.size(); p++) {
                                if (obj.parasitePaths[p] == activeBlock->sourcePath) {
                                    if (obj.parasiteAnims.size() > p && activeBlock->animIndex < obj.parasiteAnimCounts[p]) {
                                        targetAnim = &obj.parasiteAnims[p][activeBlock->animIndex]; 
                                    }
                                    break;
                                }
                            }
                            if (!targetAnim && obj.filePath == activeBlock->sourcePath && obj.anims != nullptr) {
                                if (activeBlock->animIndex < obj.animCount) {
                                    targetAnim = &obj.anims[activeBlock->animIndex];
                                }
                            }
                            
                            if (targetAnim && targetAnim->keyframeCount > 0 && obj.animCount > 0 && obj.anims != nullptr) {
                                if (targetAnim->boneCount == obj.anims[0].boneCount) {
                                    int localFrame = (int)((localTime - activeBlock->startTime) * 60.0f) % targetAnim->keyframeCount;
                                    UpdateModelAnimation(obj.model, *targetAnim, localFrame);
                                    
                                    Matrix bScale = MatrixScale(activeBlock->scaleOffset, activeBlock->scaleOffset, activeBlock->scaleOffset);
                                    Matrix bRot = MatrixRotateXYZ({activeBlock->rotationOffset.x*DEG2RAD, activeBlock->rotationOffset.y*DEG2RAD, activeBlock->rotationOffset.z*DEG2RAD});
                                    Matrix bTrans = MatrixTranslate(activeBlock->positionOffset.x, activeBlock->positionOffset.y, activeBlock->positionOffset.z);
                                    blockTransform = MatrixMultiply(MatrixMultiply(bScale, bRot), bTrans);
                                }
                            }
                        }
                    } 
                    else if (obj.isAnimated && obj.anims != nullptr) {
                        if (track.currentAnimIndex < obj.animCount && obj.anims != nullptr && obj.animCount > 0) {
                            int animFrames = obj.anims[track.currentAnimIndex].keyframeCount; 
                            
                            if (animFrames > 0 && obj.anims[track.currentAnimIndex].boneCount == obj.anims[0].boneCount) {
                                int tPoseSkip = (animFrames > 60) ? 60 : 0;
                                int usableFrames = animFrames - tPoseSkip;
                                if (usableFrames <= 0) usableFrames = 1; 
                                
                                int playheadOffset = (globalFrame - track.startFrame) % usableFrames;
                                int localFrame = tPoseSkip + playheadOffset;
                                UpdateModelAnimation(obj.model, obj.anims[track.currentAnimIndex], localFrame);
                            }
                        }
                    }
                    
                    Vector3 pivot = { (obj.bounds.max.x + obj.bounds.min.x) / 2.0f, obj.bounds.min.y, (obj.bounds.max.z + obj.bounds.min.z) / 2.0f };
                    Matrix tPivot = MatrixTranslate(-pivot.x, -pivot.y, -pivot.z);
                    Matrix tScale = MatrixScale(obj.scale, obj.scale, obj.scale);
                    Matrix tRot = MatrixRotateXYZ({ DEG2RAD * sceneRotations[i].x, DEG2RAD * sceneRotations[i].y, DEG2RAD * sceneRotations[i].z });
                    Matrix tTrans = MatrixTranslate(obj.position.x, obj.position.y, obj.position.z);

                    Matrix mainTransform = MatrixMultiply(MatrixMultiply(MatrixMultiply(tPivot, tScale), tRot), tTrans);
                    
                    obj.model.transform = MatrixMultiply(blockTransform, mainTransform);

                    DrawModel(obj.model, Vector3Zero(), 1.0f, WHITE);

                    if ((int)i == selectedObjectIndex) {
                        Vector3 extents = { ((obj.bounds.max.x - obj.bounds.min.x) / 2.0f) * obj.scale, ((obj.bounds.max.y - obj.bounds.min.y) / 2.0f) * obj.scale, ((obj.bounds.max.z - obj.bounds.min.z) / 2.0f) * obj.scale };
                        BoundingBox worldBox = { { obj.position.x - extents.x, obj.position.y, obj.position.z - extents.z }, { obj.position.x + extents.x, obj.position.y + extents.y * 2.0f, obj.position.z + extents.z } };
                        DrawBoundingBox(worldBox, GREEN);
                    }
                }
            }

            if (isPreviewing) {
                Vector3 pivot = { (previewObject.bounds.max.x + previewObject.bounds.min.x) / 2.0f, previewObject.bounds.min.y, (previewObject.bounds.max.z + previewObject.bounds.min.z) / 2.0f };
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
                    if (!openPath.empty()) { selectedObjectIndex = -1; LoadNexusProject(openPath, scene, sceneRotations, timeline); }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Save Project As...")) {
                    std::string savePath = SaveNexusFileDialog();
                    if (!savePath.empty()) SaveNexusProject(savePath, scene, sceneRotations, timeline);
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit")) shouldExit = true;
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Import")) {
                if (ImGui::MenuItem("Master Importer & Mapper")) {
                    showManualImporter = true;
                }
                ImGui::EndMenu();
            }
            
            if (selectedObjectIndex != -1) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.55f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
                if (ImGui::SmallButton("  Character Creator  ")) {
                    showCharCreator = true;
                    charCreator.InitFromObject(scene[selectedObjectIndex]);
                }
                ImGui::PopStyleColor(2);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
                ImGui::SmallButton("  Character Creator  ");
                ImGui::PopStyleColor();
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Select a character first.");
            }
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.65f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.55f, 0.85f, 1.0f));
            if (ImGui::SmallButton("  Scene Manager  ")) showSceneManager = true;
            ImGui::PopStyleColor(2);

            if (ImGui::BeginMenu("Window")) {
                ImGui::MenuItem("Asset Browser", NULL, &showAssetBrowser);
                ImGui::MenuItem("Sequencer Timeline", NULL, &showTimeline);
                ImGui::MenuItem("Scene Manager", NULL, &showSceneManager);
                ImGui::MenuItem("Animation State Machine", NULL, &showAnimator);
                ImGui::MenuItem("Character Creator", NULL, &showCharCreator);
                ImGui::Separator();
                if (ImGui::MenuItem("Rescan Asset Folders")) ScanForAssets(envLibrary, charLibrary);
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        assetBrowser.Draw(showAssetBrowser, envLibrary, charLibrary, previewObject, isPreviewing);

        animator.Draw(showAnimator);
        if (animator.needsRescan) {
            ScanForAssets(envLibrary, charLibrary);
            animator.needsRescan = false;
        }

        static int lastCreatorSelection = -2;
        if (selectedObjectIndex != lastCreatorSelection) {
            if (selectedObjectIndex != -1)
                charCreator.InitFromObject(scene[selectedObjectIndex]);
            lastCreatorSelection = selectedObjectIndex;
        }
        if (showCharCreator && selectedObjectIndex != -1) {
            charCreator.Draw(scene[selectedObjectIndex], showCharCreator);
            charCreator.ApplyToObject(scene[selectedObjectIndex]);
        } else if (showCharCreator) {
            SceneObject empty;
            charCreator.Draw(empty, showCharCreator);
        }

        if (showTimeline) {
            float browserWidth = showAssetBrowser ? 300.0f : 0.0f;
            int timelineDeleteIndex = -1;
            timeline.Draw(globalFrame, browserWidth, timelineDeleteIndex);
            if (timelineDeleteIndex >= 0 && timelineDeleteIndex < (int)scene.size()) {
                SceneObject& obj = scene[timelineDeleteIndex];
                UnloadModel(obj.model);
                if (obj.isAnimated && obj.anims != nullptr)
                    UnloadModelAnimations(obj.anims, obj.animCount);
                for (size_t pi = 0; pi < obj.parasiteAnims.size(); pi++)
                    UnloadModelAnimations(obj.parasiteAnims[pi], obj.parasiteAnimCounts[pi]);
                scene.erase(scene.begin() + timelineDeleteIndex);
                sceneRotations.erase(sceneRotations.begin() + timelineDeleteIndex);
                timeline.tracks.erase(timeline.tracks.begin() + timelineDeleteIndex);
                if (selectedObjectIndex >= (int)scene.size())
                    selectedObjectIndex = (int)scene.size() - 1;
                if (scene.empty()) selectedObjectIndex = -1;
            }
        }

        sceneManager.Draw(showSceneManager, scene, sceneRotations, timeline, selectedObjectIndex);

        // --> DRAW THE NEW UI INSIDE THE IMGUI BLOCK
        manualImporter.DrawUI(showManualImporter);

        // ADD THIS TO AUTO-RELOAD THE BROWSER
        if (manualImporter.needsRescan) {
            ScanForAssets(envLibrary, charLibrary);
            manualImporter.needsRescan = false;
        }

        if (selectedObjectIndex != -1) {
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            float inspectorWidth = 320.0f;
            ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - inspectorWidth, viewport->WorkPos.y), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(inspectorWidth, viewport->WorkSize.y), ImGuiCond_Always);
            
            ImGui::Begin("Transform Inspector", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Selected: %s", timeline.tracks[selectedObjectIndex].assetName.c_str());
            ImGui::Separator();

            int mode = (int)currentMode;
            ImGui::RadioButton("Move", &mode, 0); ImGui::SameLine();
            ImGui::RadioButton("Rotate", &mode, 1); ImGui::SameLine();
            ImGui::RadioButton("Scale", &mode, 2);
            currentMode = (TransformMode)mode;

            ImGui::Separator();
            if (currentMode == MODE_MOVE && !isPlottingWaypoints) ImGui::TextDisabled("Use Arrow Keys to move manually.");
            else if (isPlottingWaypoints) ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "PLOTTING ACTIVE: Click on Ground.");
            else ImGui::TextDisabled("Click & Drag mouse in viewport to modify.");

            Vector3 inspectPos = scene[selectedObjectIndex].position;
            if (ImGui::DragFloat3("Position##obj", &inspectPos.x, 0.1f)) {
                Vector3 delta = { inspectPos.x - scene[selectedObjectIndex].position.x, inspectPos.y - scene[selectedObjectIndex].position.y, inspectPos.z - scene[selectedObjectIndex].position.z };
                scene[selectedObjectIndex].position = inspectPos;
                for (auto& wp : scene[selectedObjectIndex].waypoints) { wp.x += delta.x; wp.y += delta.y; wp.z += delta.z; }
            }

            ImGui::DragFloat3("Rotation##obj", &sceneRotations[selectedObjectIndex].x, 1.0f);
            ImGui::DragFloat("Scale##obj", &scene[selectedObjectIndex].scale, 0.01f);
            
            ImGui::Spacing();
            ImGui::Checkbox("Stay on Scene", &timeline.tracks[selectedObjectIndex].stayOnScene);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "AI Pathfinding Nodes");
            ImGui::DragFloat("Walk Speed", &scene[selectedObjectIndex].walkSpeed, 0.1f, 0.1f, 20.0f, "%.1f m/s");
            ImGui::Checkbox("Loop Waypoint Path", &scene[selectedObjectIndex].loopWaypoints);
            
            ImGui::Spacing();
            if (isPlottingWaypoints) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                if (ImGui::Button("FINISH PLOTTING (Or Right-Click)", ImVec2(-1, 26))) isPlottingWaypoints = false;
                ImGui::PopStyleColor();
            } else {
                if (ImGui::Button("PLOT WAYPOINTS", ImVec2(-1, 26))) isPlottingWaypoints = true;
            }

            ImGui::SameLine();
            if (ImGui::Button("CLEAR PATH", ImVec2(-1, 26))) {
                scene[selectedObjectIndex].waypoints.clear();
                scene[selectedObjectIndex].loopWaypoints = false;
            }
            ImGui::Separator();
            
            if (!scene[selectedObjectIndex].montage.empty()) {
                float maxTime = 0.0f;
                for (auto& b : scene[selectedObjectIndex].montage) {
                    if (b.startTime + b.duration > maxTime) maxTime = b.startTime + b.duration;
                }
                int montageLenFrames = (int)(maxTime * 60.0f);
                if (montageLenFrames < 1) montageLenFrames = 1;

                float localTime = (float)((globalFrame - timeline.tracks[selectedObjectIndex].startFrame) % montageLenFrames) / 60.0f;
                
                std::string currentPlayingName = "None";
                for (auto& b : scene[selectedObjectIndex].montage) {
                    if (localTime >= b.startTime && localTime <= b.startTime + b.duration) {
                        currentPlayingName = b.animName; break;
                    }
                }
                
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Animation Target (Montage)");
                if (ImGui::BeginCombo("##MontageAnim", currentPlayingName.c_str())) {
                    for (auto& b : scene[selectedObjectIndex].montage) {
                        bool isSelected = (currentPlayingName == b.animName);
                        ImGui::Selectable(b.animName.c_str(), isSelected);
                    }
                    ImGui::EndCombo();
                }
                ImGui::TextDisabled("Driven dynamically by State Machine");
            } else {
                DrawAnimationSelector(scene[selectedObjectIndex], timeline.tracks[selectedObjectIndex]);
            }
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::TextColored(ImVec4(0.8f, 0.6f, 1.0f, 1.0f), "Save as Prefab");
            static char prefabNameBuf[128] = "";
            ImGui::InputText("Name", prefabNameBuf, IM_ARRAYSIZE(prefabNameBuf));
            
            if (ImGui::Button("SAVE AS NEW ASSET", ImVec2(-1, 30))) {
                if (strlen(prefabNameBuf) > 0) {
                    std::string savePath = "Chars/" + std::string(prefabNameBuf) + ".nxchar";
                    SaveNexusCharacterPrefab(scene[selectedObjectIndex], savePath);
                    ScanForAssets(envLibrary, charLibrary); 
                    prefabNameBuf[0] = '\0'; 
                }
            }
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button("SAVE TRANSFORM", ImVec2(-1, 35))) selectedObjectIndex = -1;

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 0.3f, 1.0f));
            if (ImGui::Button("OPEN CHARACTER CREATOR", ImVec2(-1, 30))) {
                showCharCreator = true;
                charCreator.InitFromObject(scene[selectedObjectIndex]);
            }
            ImGui::PopStyleColor();
            
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
        
        for(size_t i=0; i<obj.parasiteAnims.size(); i++){
            UnloadModelAnimations(obj.parasiteAnims[i], obj.parasiteAnimCounts[i]);
        }
    }
    
    rlImGuiShutdown();
    CloseWindow();
    return 0;
}