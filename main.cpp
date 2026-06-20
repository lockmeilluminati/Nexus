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
#include "nexus_manual_importer.h"
#include "nexus_camera_track.h"
#include "nexus_publisher.h"
#include "nexus_player.h"

// NEW OVERLAYS AND LOGIC
#include "nexus_text_fx.h"
#include "nexus_creator_menu.h"
#include "nexus_math_player.h" 
#include "nexus_mocap_studio.h"
#include "nexus_splash.h"

#include "rlImGui.h"        
#include "imgui.h"          
#include <string>
#include <vector>
#include <algorithm>
#include <cstring> 
#include <future>
#include <chrono>

extern std::string OpenAudioFileDialog();
extern std::string OpenNexusFileDialog();
extern std::string SaveNexusFileDialog();
extern std::string OpenGLTFFileDialog();

std::vector<Asset> envLibrary;
std::vector<Asset> charLibrary;
std::vector<SceneObject> scene;
std::vector<Vector3> sceneRotations; 

// GLOBAL MOCAP STORAGE
std::vector<NexusMocapTrack> activeMocapTracks;

// GLOBAL SELECTION STATES
int selectedObjectIndex = -1;
int selectedTextIndex = -1; 

enum TransformMode { MODE_MOVE, MODE_ROTATE, MODE_SCALE };
TransformMode currentMode = MODE_MOVE;
bool isPlottingWaypoints = false; 

// ========================================================
// UI WARNING STATE VARIABLES
// ========================================================
bool showControllerConflictWarning = false;
float controllerConflictTimer = 0.0f;

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
    SetTraceLogCallback(NexusLogCallback);

    SetConfigFlags(FLAG_WINDOW_RESIZABLE); 
    InitWindow(1280, 720, "Nexus 3D Engine");
    MaximizeWindow(); 
    SetTargetFPS(60);

    Camera camera = { { 15.0f, 25.0f, 15.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, 45.0f, 0 };

    NexusSplash splash;
    splash.Init();
    InitAudioDevice();

    BeginDrawing();
    ClearBackground(BLACK); 
    splash.DrawSplashFrame();
    EndDrawing();
    
    BeginDrawing();
    ClearBackground(BLACK); 
    splash.DrawSplashFrame();
    EndDrawing();

    ScanForAssets(envLibrary, charLibrary);

    rlImGuiSetup(true);
    
    NexusTimeline timeline;
    NexusAssetBrowser assetBrowser; 
    
    NexusAnimator animator;
    bool showAnimator = false;
    
    NexusTextManager textManager; 

    NexusCreatorMenu creatorMenu;
    NexusCharacterCreator charCreator;
    bool showCharCreator = false;

    NexusMathPlayer mathPlayer;
    mathPlayer.Init();

    NexusSceneManager sceneManager;
    bool showSceneManager = false;

    NexusManualImporter manualImporter;
    bool showManualImporter = false;

    NexusCameraTrack cameraTrack;
    bool showCameraTrack = false;
    
    NexusPublisher publisher;
    bool showPublisher = false;
    
    NexusMocapStudio mocapStudio; 
    bool showMocapStudio = false; 
    
    NexusPlayer player;
    bool playerInitialized = false;
    int activePlayerIndex = -1;  
    bool showPlayerInspector = false;
    bool wasInGameZone = false;

    bool isMathPlayMode = false; 
    bool useMathController = false; 

    int globalFrame = 0;
    bool isPreviewing = false;
    SceneObject previewObject;
    bool showAssetBrowser = true;
    bool showTimeline = true; 
    bool shouldExit = false;

    while (!WindowShouldClose() && !shouldExit) {
        
        if (isMathPlayMode) {
            mathPlayer.Update();
            
            if (!IsCursorHidden()) DisableCursor();
            Vector2 mouseDelta = GetMouseDelta();
            
            static float engineCamPitch = 0.0f;
            static float engineCamYaw = 0.0f;
            static bool engineCamInit = false;
            
            if (!engineCamInit) {
                Vector3 dir = Vector3Normalize(Vector3Subtract(mathPlayer.camera.target, mathPlayer.camera.position));
                engineCamPitch = asinf(dir.y);
                engineCamYaw = atan2f(dir.x, dir.z);
                engineCamInit = true;
            }
            
            engineCamYaw -= mouseDelta.x * 0.003f;
            engineCamPitch -= mouseDelta.y * 0.003f;
            if (engineCamPitch > 1.5f) engineCamPitch = 1.5f;
            if (engineCamPitch < -1.5f) engineCamPitch = -1.5f;
            
            Vector3 forward = { cosf(engineCamPitch) * sinf(engineCamYaw), sinf(engineCamPitch), cosf(engineCamPitch) * cosf(engineCamYaw) };
            mathPlayer.camera.target = Vector3Add(mathPlayer.camera.position, forward);

            if (!player.isPlayMode) {
                mathPlayer.isActive = false;
                isMathPlayMode = false;
                engineCamInit = false; 
                EnableCursor();
            }
        }

        if (timeline.isPlaying) {
            globalFrame++;
            if (globalFrame >= timeline.maxTimelineFrames) {
                globalFrame = 0;
            }
        }

        timeline.audioManager.UpdateAudioSync(globalFrame, timeline.isPlaying);

        if (timeline.isPlaying && playerInitialized) {
            const GameZone* gz = timeline.GetActiveGameZone(globalFrame);
            if (gz && !wasInGameZone && !player.isPlayMode) {
                float limit = gz->infiniteTime ? 999999.0f : gz->timeLimitSecs;
                
                if (useMathController) {
                    isMathPlayMode = true;
                    mathPlayer.isActive = true;
                    player.isPlayMode = true;
                    player.playTimeLimit = limit;
                    player.playTimer = 0.0f;
                } else {
                    EnterPlayMode(player, limit);
                }
                wasInGameZone = true;
                
            } else if (!gz && wasInGameZone && player.isPlayMode) {
                player.isPlayMode = false;
                player.playTimer  = 0.0f;
                wasInGameZone     = false;
                EnableCursor();
                
                if (useMathController) {
                    isMathPlayMode = false;
                    mathPlayer.isActive = false;
                }
            } else if (gz) {
                wasInGameZone = true;
            } else {
                wasInGameZone = false;
            }
        } else if (!timeline.isPlaying) {
            wasInGameZone = false;
        }

        bool isMouseInViewport = !ImGui::GetIO().WantCaptureMouse;
        bool suppressCamera = false;
        if (playerInitialized && player.isPlayMode) { 
            isMouseInViewport = false; 
            suppressCamera = true; 
            
            for (auto& obj : scene) {
                obj.audioEmitter.Update(camera.position, obj.position, GetFrameTime());
            }
        }

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

        bool timelineOverridesCam = timeline.SyncCameraToTimeline(globalFrame, camera);

        if (cameraTrack.isActive) {
            cameraTrack.Update(GetFrameTime());
            cameraTrack.ApplyToCamera(camera);
        } else if (!timelineOverridesCam && isMouseInViewport && !suppressCamera && !mathPlayer.isActive) {
            UpdateViewportCamera(camera);
        }

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
                selectedTextIndex = -1; 
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

                for (auto& line : previewObject.audioEmitter.playlist) {
                    timeline.audioManager.AddAudioTrack(line.filePath);
                    if (!timeline.audioManager.tracks.empty()) {
                        std::string cleanName = scene[selectedObjectIndex].name;
                        if (cleanName.empty()) cleanName = "Character";
                        timeline.audioManager.tracks.back().assetName = cleanName + " Voice";
                        timeline.audioManager.tracks.back().startFrame = globalFrame;
                    }
                }
            }
            else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                if (previewObject.model.meshCount > 0) UnloadModel(previewObject.model);
                if (previewObject.isAnimated && previewObject.anims != nullptr) UnloadModelAnimations(previewObject.anims, previewObject.animCount);
                for(size_t i=0; i<previewObject.parasiteAnims.size(); i++) {
                    if (previewObject.parasiteAnims[i] != nullptr) UnloadModelAnimations(previewObject.parasiteAnims[i], previewObject.parasiteAnimCounts[i]);
                }
                
                previewObject.audioEmitter.UnloadAll();
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
                if (hitIndex != -1) {
                    selectedObjectIndex = hitIndex;
                    selectedTextIndex = -1; 
                }
            }
            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && isMouseInViewport) {
                selectedObjectIndex = -1; 
                selectedTextIndex = -1;
            }
        }

        manualImporter.UpdateViewport(showManualImporter);

        if (manualImporter.needsRescan) {
            SetTraceLogLevel(LOG_ERROR);
            ScanForAssets(envLibrary, charLibrary);
            SetTraceLogLevel(LOG_INFO);
            manualImporter.needsRescan = false;
        }

        if (showMocapStudio) {
            mocapStudio.Update(GetFrameTime());
            mocapStudio.UpdateViewport();
        }

        if (mocapStudio.requestAssetRescan) {
            SetTraceLogLevel(LOG_ERROR); 
            ScanForAssets(envLibrary, charLibrary);
            SetTraceLogLevel(LOG_INFO);  
            mocapStudio.requestAssetRescan = false;
        }

        if (mocapStudio.requestSaveToTimeline) {
            mocapStudio.requestSaveToTimeline = false;
            
            activeMocapTracks.push_back(mocapStudio.outputTrack);
            
            SceneObject mocapObj;
            mocapObj.name = mocapStudio.outputTrack.trackName;
            mocapObj.filePath = "PROCEDURAL_MOCAP"; 
            mocapObj.position = {0.0f, 0.0f, 0.0f}; 
            mocapObj.scale = 1.0f;
            
            scene.push_back(mocapObj);
            sceneRotations.push_back({0.0f, 0.0f, 0.0f});

            int framesCount = mocapStudio.outputTrack.frames.size();
            timeline.AddTrack("MoCap: " + mocapObj.name, framesCount, false);
            
            timeline.tracks.back().startFrame = globalFrame;
            timeline.tracks.back().endFrame = globalFrame + (framesCount > 0 ? framesCount : 300);
            
            TraceLog(LOG_INFO, "MOCAP: Track registered and sent to Timeline UI at frame %d!", globalFrame);
        }

        BeginDrawing();
        ClearBackground(DARKGRAY);

        BeginMode3D(mathPlayer.isActive ? mathPlayer.camera : camera);
            DrawGrid(20, 1.0f);
            
            if (selectedObjectIndex != -1) {
                scene[selectedObjectIndex].audioEmitter.DrawDebugZone(scene[selectedObjectIndex].position);
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

                if (obj.filePath == "PROCEDURAL_MOCAP") {
                    for (auto& mTrack : activeMocapTracks) {
                        if (mTrack.trackName == obj.name) {
                            if (globalFrame >= track.startFrame && (globalFrame <= track.endFrame || track.stayOnScene)) {
                                float localTime = (float)(globalFrame - track.startFrame) / 60.0f;
                                MocapFrame currentFrame;
                                mTrack.Evaluate(localTime, currentFrame);

                                for (int j = 0; j < MOCAP_JOINT_COUNT; j++) {
                                    currentFrame.joints[j] = Vector3Add(currentFrame.joints[j], obj.position);
                                }

                                if (mTrack.snapToInteractiveObject && selectedObjectIndex != -1 && selectedObjectIndex != (int)i) {
                                    Vector3 riflePos = scene[selectedObjectIndex].position;
                                    currentFrame.joints[3] = riflePos;
                                    currentFrame.joints[2] = { riflePos.x + 0.4f, riflePos.y, riflePos.z + 0.1f };
                                }

                                mocapStudio.RenderSkin(currentFrame, mTrack.skinID);
                            }
                            break;
                        }
                    }
                    continue; 
                }
                
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

            cameraTrack.DrawPath();
            
            if (playerInitialized && player.isPlayMode && !isMathPlayMode) {
                UpdateNexusPlayer(player, camera);
                DrawNexusPlayer(player);
            }
            if (isPreviewing) {
                Vector3 pivot = { (previewObject.bounds.max.x + previewObject.bounds.min.x) / 2.0f, previewObject.bounds.min.y, (previewObject.bounds.max.z + previewObject.bounds.min.z) / 2.0f };
                Matrix tPivot = MatrixTranslate(-pivot.x, -pivot.y, -pivot.z);
                Matrix tScale = MatrixScale(previewObject.scale, previewObject.scale, previewObject.scale);
                Matrix tTrans = MatrixTranslate(previewObject.position.x, previewObject.position.y, previewObject.position.z);
                previewObject.model.transform = MatrixMultiply(MatrixMultiply(tPivot, tScale), tTrans);
                DrawModel(previewObject.model, Vector3Zero(), 1.0f, Fade(WHITE, 0.5f));
            }

            if (mathPlayer.isActive) {
                mathPlayer.DrawProceduralArms();
            }

        EndMode3D();// =========================================================================
        // 2D OVERLAYS & UI LAYER
        // =========================================================================

        // DRAW CINEMATIC TEXT FX
        for (auto& txt : timeline.textTracks) {
            textManager.DrawCinematicText(txt, globalFrame);
        }

        if (playerInitialized && player.isPlayMode && !isMathPlayMode) {
            for (auto& obj : scene) {
                obj.audioEmitter.DrawGameplayPrompt(GetScreenWidth(), GetScreenHeight());
            }
        }

        rlImGuiBegin();

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Open Project...")) {
                    std::string openPath = OpenNexusFileDialog();
                    if (!openPath.empty()) { 
                        selectedObjectIndex = -1; 
                        selectedTextIndex = -1;
                        
                        LoadNexusProject(openPath, scene, sceneRotations, timeline, cameraTrack, playerInitialized, useMathController, activePlayerIndex); 
                        
                        // INSTANT PLAYER RE-ALLOCATION
                        if (playerInitialized) {
                            if (useMathController) {
                                mathPlayer.Init();
                                TraceLog(LOG_INFO, "PROJECT LOAD: Math Player Controller restored.");
                            } else if (activePlayerIndex >= 0 && activePlayerIndex < (int)scene.size()) {
                                InitNexusPlayer(player, scene[activePlayerIndex]);
                                player.perspective = PlayerPerspective::PERSPECTIVE_THIRD_PERSON;
                                showPlayerInspector = true;
                                TraceLog(LOG_INFO, "PROJECT LOAD: Third Person Controller restored.");
                            } else {
                                playerInitialized = false; 
                            }
                        }
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Save Project As...")) {
                    std::string savePath = SaveNexusFileDialog();
                    if (!savePath.empty()) {
                        SaveNexusProject(savePath, scene, sceneRotations, timeline, cameraTrack, playerInitialized, useMathController, activePlayerIndex);
                    }
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
            
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.55f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
            if (ImGui::SmallButton("  Character Creator  ")) {
                selectedObjectIndex = -1; 
                showCharCreator = true;
                creatorMenu.currentMode = CreatorMode::MODE_HUB_SELECT;
            }
            ImGui::PopStyleColor(2);

            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.65f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.55f, 0.85f, 1.0f));
            if (ImGui::SmallButton("  Scene Manager  ")) showSceneManager = true;
            ImGui::PopStyleColor(2);

            if (playerInitialized) {
                ImGui::SameLine();
                ImGui::Spacing(); ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
                if (ImGui::SmallButton("  [X] UNASSIGN PLAYER CONTROLLER  ")) {
                    playerInitialized = false;
                    showPlayerInspector = false;
                    useMathController = false;
                    isMathPlayMode = false;
                    activePlayerIndex = -1;
                }
                ImGui::PopStyleColor(2);
            }

            if (ImGui::BeginMenu("Window")) {
                ImGui::MenuItem("Asset Browser", NULL, &showAssetBrowser);
                ImGui::MenuItem("Sequencer Timeline", NULL, &showTimeline);
                ImGui::MenuItem("Scene Manager", NULL, &showSceneManager);
                ImGui::MenuItem("Animation State Machine", NULL, &showAnimator);
                if (ImGui::MenuItem("Character Creator", NULL, &showCharCreator)) {
                    if (showCharCreator) {
                        selectedObjectIndex = -1; 
                        creatorMenu.currentMode = CreatorMode::MODE_HUB_SELECT;
                    }
                }
                ImGui::MenuItem("Camera Track Editor", NULL, &showCameraTrack);
                ImGui::MenuItem("MoCap Plugin Studio", NULL, &showMocapStudio);
                ImGui::MenuItem("Publish Project", NULL, &showPublisher);
                if (playerInitialized) ImGui::MenuItem("Player Inspector", NULL, &showPlayerInspector);
                ImGui::Separator();
                if (ImGui::MenuItem("Rescan Asset Folders")) {
                    SetTraceLogLevel(LOG_ERROR); 
                    ScanForAssets(envLibrary, charLibrary);
                    SetTraceLogLevel(LOG_INFO);  
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        assetBrowser.Draw(showAssetBrowser, envLibrary, charLibrary, previewObject, isPreviewing);

        animator.Draw(showAnimator);
        if (animator.needsRescan) {
            SetTraceLogLevel(LOG_ERROR);
            ScanForAssets(envLibrary, charLibrary);
            SetTraceLogLevel(LOG_INFO);
            animator.needsRescan = false;
        }

        static int lastCreatorSelection = -2;
        if (selectedObjectIndex != lastCreatorSelection) {
            if (selectedObjectIndex != -1)
                charCreator.InitFromObject(scene[selectedObjectIndex]);
            lastCreatorSelection = selectedObjectIndex;
        }
        
        if (showCharCreator) {
            SceneObject* targetObj = &previewObject; 
            if (selectedObjectIndex >= 0 && selectedObjectIndex < (int)scene.size()) {
                targetObj = &scene[selectedObjectIndex];
            }

            if (creatorMenu.currentMode == CreatorMode::MODE_HUB_SELECT) {
                creatorMenu.DrawMainMenu(showCharCreator);
            }
            else if (creatorMenu.currentMode == CreatorMode::MODE_THIRD_PERSON) {
                charCreator.Draw(*targetObj, showCharCreator);
                if (charCreator.requestClear) {
                    charCreator.requestClear = false;
                    creatorMenu.TriggerGlobalReset();
                }
                if (selectedObjectIndex != -1) {
                    charCreator.ApplyToObject(*targetObj);
                }
                
                if (charCreator.sendToPlayer) {
                    if (playerInitialized) {
                        charCreator.sendToPlayer = false;
                        showControllerConflictWarning = true;
                        controllerConflictTimer = 4.0f;
                    } else {
                        useMathController = false; 
                        InitNexusPlayer(player, *targetObj);
                        player.perspective = PlayerPerspective::PERSPECTIVE_THIRD_PERSON;
                        playerInitialized = true;
                        activePlayerIndex = selectedObjectIndex; 
                        showPlayerInspector = true;
                        charCreator.sendToPlayer = false; 
                    }
                }
                
                if (showControllerConflictWarning && controllerConflictTimer > 0.0f) {
                    ImGui::Begin("Character Creator Pipeline");
                    ImGui::Spacing(); ImGui::Separator();
                    ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "[!] SYSTEM LOCKOUT: Player Controller is already active.");
                    ImGui::TextDisabled("Open 'Window -> Player Inspector' and delete the active controller first.");
                    ImGui::End();
                    controllerConflictTimer -= GetFrameTime();
                }
            }
            else if (creatorMenu.currentMode == CreatorMode::MODE_FIRST_PERSON) {
                ImGui::SetNextWindowSize(ImVec2(350, 260), ImGuiCond_FirstUseEver);
                ImGui::Begin("First Person Setup", nullptr, ImGuiWindowFlags_NoCollapse);
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.8f, 1.0f), "Procedural Math Controller");
                ImGui::Separator();
                ImGui::TextDisabled("Physics-based idle bobbing");
                ImGui::TextDisabled("and punching mechanics ready.");
                
                ImGui::Spacing(); ImGui::Spacing();
                
                if (playerInitialized) {
                    ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "[!] SYSTEM LOCKOUT");
                    ImGui::TextDisabled("A controller is already running.\nDelete it in the Player Inspector\nbefore allocating a new one.");
                    ImGui::Spacing(); ImGui::Spacing();
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
                    if (ImGui::Button("ALLOCATE MATH CONTROLLER", ImVec2(-1, 50))) {
                        useMathController = true;  
                        playerInitialized = true;  
                        activePlayerIndex = -1; 
                        mathPlayer.Init();
                        
                        showCharCreator = false; 
                        creatorMenu.TriggerGlobalReset(); 
                        TraceLog(LOG_INFO, "Math Controller Allocated. Ready for Timeline Play!");
                    }
                    ImGui::PopStyleColor(2);
                }
                
                ImGui::Spacing();
                if (ImGui::Button("CANCEL / RETURN TO HUB", ImVec2(-1, 30))) {
                    creatorMenu.TriggerGlobalReset();
                }
                ImGui::End();
            }

            if (creatorMenu.globalClearRequested) {
                creatorMenu.globalClearRequested = false;
                
                // THE FIX: Memory Dereference Protection
                if (targetObj == &previewObject && isPreviewing) {
                    if (targetObj->model.meshCount > 0) UnloadModel(targetObj->model);
                    if (targetObj->isAnimated && targetObj->anims != nullptr) {
                        UnloadModelAnimations(targetObj->anims, targetObj->animCount);
                    }
                    for (size_t pi = 0; pi < targetObj->parasiteAnims.size(); pi++) {
                        if (targetObj->parasiteAnims[pi] != nullptr) UnloadModelAnimations(targetObj->parasiteAnims[pi], targetObj->parasiteAnimCounts[pi]);
                    }
                    targetObj->audioEmitter.UnloadAll();
                    previewObject = SceneObject(); 
                    isPreviewing = false; 
                } 
            }
        }

        if (showTimeline) {
            float browserWidth = showAssetBrowser ? 300.0f : 0.0f;
            int timelineDeleteIndex = -1;
            bool playModeRequested = false; float playTimeLimit = 10.0f;
            
            timeline.Draw(globalFrame, browserWidth, timelineDeleteIndex, playModeRequested, playTimeLimit, selectedObjectIndex, selectedTextIndex);
            
            if (playModeRequested && playerInitialized) {
                if (useMathController) {
                    isMathPlayMode = true;
                    mathPlayer.isActive = true;
                    player.isPlayMode = true;
                    player.playTimeLimit = playTimeLimit;
                    player.playTimer = 0.0f;
                } else {
                    if (!scene.empty()) {
                        int si = activePlayerIndex >= 0 ? activePlayerIndex : (selectedObjectIndex >= 0 ? selectedObjectIndex : 0);
                        if (si < (int)scene.size()) {
                            InitNexusPlayer(player, scene[si]);
                            EnterPlayMode(player, playTimeLimit);
                            showPlayerInspector = true;
                        }
                    }
                }
            }
            
            if (timelineDeleteIndex >= 0 && timelineDeleteIndex < (int)scene.size()) {
                SceneObject& obj = scene[timelineDeleteIndex];
                
                if (obj.filePath == "PROCEDURAL_MOCAP") {
                    for (auto it = activeMocapTracks.begin(); it != activeMocapTracks.end(); ++it) {
                        if (it->trackName == obj.name) {
                            activeMocapTracks.erase(it);
                            break;
                        }
                    }
                } else {
                    if (obj.model.meshCount > 0) UnloadModel(obj.model);
                    if (obj.isAnimated && obj.anims != nullptr)
                        UnloadModelAnimations(obj.anims, obj.animCount);
                    for (size_t pi = 0; pi < obj.parasiteAnims.size(); pi++) {
                        if (obj.parasiteAnims[pi] != nullptr) UnloadModelAnimations(obj.parasiteAnims[pi], obj.parasiteAnimCounts[pi]);
                    }
                }
                
                obj.audioEmitter.UnloadAll();
                
                scene.erase(scene.begin() + timelineDeleteIndex);
                sceneRotations.erase(sceneRotations.begin() + timelineDeleteIndex);
                timeline.tracks.erase(timeline.tracks.begin() + timelineDeleteIndex);
                
                if (timelineDeleteIndex < activePlayerIndex) activePlayerIndex--;
                else if (timelineDeleteIndex == activePlayerIndex) {
                    playerInitialized = false; activePlayerIndex = -1; showPlayerInspector = false;
                }

                if (selectedObjectIndex >= (int)scene.size())
                    selectedObjectIndex = (int)scene.size() - 1;
                if (scene.empty()) selectedObjectIndex = -1;
            }
        }

        sceneManager.Draw(showSceneManager, scene, sceneRotations, timeline, selectedObjectIndex, selectedTextIndex, player, playerInitialized);
        manualImporter.DrawUI(showManualImporter);

        if (manualImporter.needsRescan) {
            SetTraceLogLevel(LOG_ERROR);
            ScanForAssets(envLibrary, charLibrary);
            SetTraceLogLevel(LOG_INFO);
            manualImporter.needsRescan = false;
        }

        cameraTrack.DrawUI(showCameraTrack, camera, (float)globalFrame / 60.0f);
        
        if (showMocapStudio) {
            mocapStudio.DrawUI(showMocapStudio);
        }

        if (cameraTrack.saveToTimeline) {
            cameraTrack.saveToTimeline = false;
            if (!cameraTrack.waypoints.empty()) {
                int endFrame = (int)(cameraTrack.totalDuration * 60.0f);
                if (endFrame < 60) endFrame = 300;
                
                cameraTrack.timelineStartFrame = globalFrame;
                cameraTrack.timelineEndFrame   = globalFrame + endFrame;
                cameraTrack.trimStart          = 0.0f;
                timeline.cameraTracks.push_back(cameraTrack);
                
                TraceLog(LOG_INFO, "CAMERA: '%s' added to timeline at frame %d", cameraTrack.savedTrackName.c_str(), globalFrame);
                
                cameraTrack.waypoints.clear();
                cameraTrack.selectedWaypoint = -1;
                cameraTrack.totalDuration = 0.0f;
                cameraTrack.playheadTime = 0.0f;
                cameraTrack.isActive = false;
                cameraTrack.quickPlaceTime = 0.0f;
            }
        }

        // =========================================================================
        // THE FIX: 7 ARGUMENTS! sceneRotations IS NOW PASSED TO PUBLISHER
        // =========================================================================
        publisher.Draw(showPublisher, cameraTrack, scene, sceneRotations, timeline, player, useMathController);
        
        if (playerInitialized) {
            if (player.isPlayMode && !useMathController) {
                DrawPlayModeHUD(player);
            }
            
            if (!useMathController) {
                DrawPlayerInspector(player, showPlayerInspector);
                
                if (showPlayerInspector) {
                    ImGui::Begin("Player Controller");
                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                    
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Controller Memory Management");
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.15f, 0.15f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.25f, 0.25f, 1.0f));
                    if (ImGui::Button("DELETE ACTIVE CONTROLLER", ImVec2(-1, 40))) {
                        playerInitialized = false;
                        showPlayerInspector = false;
                        useMathController = false;
                        isMathPlayMode = false;
                        activePlayerIndex = -1;
                        TraceLog(LOG_INFO, "PLAYER CONTROLLER: Memory successfully wiped.");
                    }
                    ImGui::PopStyleColor(2);
                    ImGui::End();
                }
            } else {
                if (showPlayerInspector) {
                    ImGui::Begin("Math Player Controller", &showPlayerInspector);
                    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.8f, 1.0f), "Procedural Engine Active");
                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                    
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.15f, 0.15f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.25f, 0.25f, 1.0f));
                    if (ImGui::Button("DELETE ACTIVE CONTROLLER", ImVec2(-1, 40))) {
                        playerInitialized = false;
                        showPlayerInspector = false;
                        useMathController = false;
                        isMathPlayMode = false;
                        activePlayerIndex = -1;
                    }
                    ImGui::PopStyleColor(2);
                    ImGui::End();
                }
            }
        }

        // =========================================================================
        // DYNAMIC RIGHT-HAND INSPECTOR PANEL
        // =========================================================================
        if (selectedObjectIndex != -1 || selectedTextIndex != -1) {
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            float inspectorWidth = 320.0f;
            ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - inspectorWidth, viewport->WorkPos.y), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(inspectorWidth, viewport->WorkSize.y), ImGuiCond_Always);
            
            ImGui::Begin("Inspector Panel", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

            if (selectedObjectIndex != -1) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Selected 3D Object: %s", timeline.tracks[selectedObjectIndex].assetName.c_str());
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
                
                if (scene[selectedObjectIndex].filePath != "PROCEDURAL_MOCAP") {
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
                } else {
                    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.5f, 1.0f), "Math Character Proxy");
                    ImGui::TextDisabled("Driven dynamically by MoCap Track");
                }
                
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                scene[selectedObjectIndex].audioEmitter.DrawUI();
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::TextColored(ImVec4(0.8f, 0.4f, 1.0f, 1.0f), "Cinematic Cutscene Audio");
                ImGui::TextDisabled("Audio tracks triggered by the Sequencer Timeline.");
                
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.2f, 0.8f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.3f, 0.9f, 1.0f));
                if (ImGui::Button("+ ADD CUTSCENE DIALOGUE", ImVec2(-1, 26))) {
                    std::string path = OpenAudioFileDialog();
                    if (!path.empty()) {
                        timeline.audioManager.AddAudioTrack(path);
                        if (!timeline.audioManager.tracks.empty()) {
                            std::string cleanName = scene[selectedObjectIndex].name;
                            if (cleanName.empty()) cleanName = "Character";
                            timeline.audioManager.tracks.back().assetName = cleanName + " Voice";
                            timeline.audioManager.tracks.back().startFrame = timeline.tracks[selectedObjectIndex].startFrame;
                        }
                    }
                }
                ImGui::PopStyleColor(2);
                ImGui::TextDisabled("Adds a Teal Track to the timeline. Drag it to adjust delay.");
                
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // YOUR SAVED CUSTOM INSPECTOR LOGIC
                ImGui::TextColored(ImVec4(0.8f, 0.6f, 1.0f, 1.0f), "Save as Prefab");
                static char prefabNameBuf[128] = "";
                ImGui::InputText("Name", prefabNameBuf, IM_ARRAYSIZE(prefabNameBuf));
                
                static int saveType = 0; // 0 = Character, 1 = Environment
                ImGui::RadioButton("Character (.nxchar)", &saveType, 0); ImGui::SameLine();
                ImGui::RadioButton("Environment (.nxenv)", &saveType, 1);
                
                if (ImGui::Button("SAVE AS NEW ASSET", ImVec2(-1, 30))) {
                    if (strlen(prefabNameBuf) > 0) {
                        std::string folder = (saveType == 0) ? "Chars/" : "Envs/";
                        std::string ext = (saveType == 0) ? ".nxchar" : ".nxenv";
                        std::string savePath = folder + std::string(prefabNameBuf) + ext;
                        
                        SaveNexusCharacterPrefab(scene[selectedObjectIndex], savePath);
                        
                        SetTraceLogLevel(LOG_ERROR); 
                        ScanForAssets(envLibrary, charLibrary); 
                        SetTraceLogLevel(LOG_INFO);
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
                    selectedObjectIndex = -1; 
                    showCharCreator = true;
                    creatorMenu.currentMode = CreatorMode::MODE_HUB_SELECT;
                }
                ImGui::PopStyleColor();
            } 
            else if (selectedTextIndex != -1 && selectedTextIndex < (int)timeline.textTracks.size()) {
                textManager.DrawTextEditorLayout(timeline.textTracks[selectedTextIndex], selectedTextIndex);
            }
            
            ImGui::End();
        }

        rlImGuiEnd();
        EndDrawing();
    }

    // =========================================================================
    // GRACEFUL ENGINE SHUTDOWN
    // =========================================================================
    timeline.audioManager.UnloadAll();
    
    for (auto& txt : timeline.textTracks) {
        textManager.UnloadTextFont(txt);
    }

    for (auto& asset : envLibrary) UnloadTexture(asset.thumbnail);
    for (auto& asset : charLibrary) UnloadTexture(asset.thumbnail);
    
    for (auto& obj : scene) {
        if (obj.filePath != "PROCEDURAL_MOCAP") {
            if (obj.model.meshCount > 0) UnloadModel(obj.model);
            if (obj.isAnimated && obj.anims != nullptr) UnloadModelAnimations(obj.anims, obj.animCount);
            
            for(size_t i=0; i<obj.parasiteAnims.size(); i++){
                if (obj.parasiteAnims[i] != nullptr) UnloadModelAnimations(obj.parasiteAnims[i], obj.parasiteAnimCounts[i]);
            }
        }
        obj.audioEmitter.UnloadAll();
    }
    
    rlImGuiShutdown();
    CloseAudioDevice();
    CloseWindow();
    return 0;
}