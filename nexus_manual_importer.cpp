#include "nexus_manual_importer.h"
#include "nexus_texture_extractor.h" 
#include "file_dialog.h"             
#include "imgui.h"
#include "rlImGui.h"
#include "raymath.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <sstream>
#include <filesystem>
#include <cstring> 

namespace fs = std::filesystem;

NexusManualImporter::NexusManualImporter() {
    currentToggleMode = 0; 
    isLoaded = false;
    needsRescan = false;
    renderTarget = LoadRenderTexture(1920, 1080);
    
    Image blank = GenImageColor(1, 1, WHITE);
    emptyTexture = LoadTextureFromImage(blank);
    UnloadImage(blank);
    
    previewCamera.position = { 0.0f, 2.0f, 5.0f };
    previewCamera.target = { 0.0f, 2.0f, 4.0f };
    previewCamera.up = { 0.0f, 1.0f, 0.0f };
    previewCamera.fovy = 60.0f;
    previewCamera.projection = CAMERA_PERSPECTIVE;
    
    currentBrowserDir = fs::current_path().string();
    feedbackMessage = "";
    feedbackTimer = 0.0f;
    
    isViewportHovered = false;
    isCameraDragging = false;
    
    triggerRaycast = false;
    targetMaterialID = -1;
    scrollToMaterialID = -1;

    memset(customNameInput, 0, sizeof(customNameInput)); 
}

NexusManualImporter::~NexusManualImporter() {
    if (isLoaded) UnloadModel(previewModel);
    UnloadRenderTexture(renderTarget);
    UnloadTexture(emptyTexture);
    for (auto& tex : loadedPreviewTextures) UnloadTexture(tex);
    for (auto& node : browserNodes) {
        if (node.thumbnail.id != 0) UnloadTexture(node.thumbnail);
    }
}

void NexusManualImporter::OpenDialogAndLoad() {
    std::string path = OpenGLTFFileDialog();
    if (path.empty()) return;

    std::replace(path.begin(), path.end(), '\\', '/');
    std::string targetDir = (currentToggleMode == 1) ? "Chars" : "Envs";
    std::string localPath = IngestAsset(path, targetDir);

    if (isLoaded) {
        UnloadModel(previewModel);
        for (auto& tex : loadedPreviewTextures) UnloadTexture(tex);
        loadedPreviewTextures.clear();
        materialAssignments.clear();
    }

    currentAssetPath = localPath;
    fs::path pathObj(localPath);
    currentAssetDir = pathObj.parent_path().string();
    rawAssetName = pathObj.stem().string();

    strncpy(customNameInput, rawAssetName.c_str(), sizeof(customNameInput) - 1);
    customNameInput[sizeof(customNameInput) - 1] = '\0';
    
    currentBrowserDir = fs::absolute(pathObj.parent_path()).string();

    previewModel = LoadModel(localPath.c_str());
    
    for (int i = 0; i < previewModel.materialCount; i++) {
        previewModel.materials[i].maps[MATERIAL_MAP_ALBEDO].color = WHITE;
    }
    
    isLoaded = true;

    std::string command = "py nexus_smart_extractor.py \"" + currentAssetPath + "\"";
    std::system(command.c_str());
    
    RefreshBrowser(); 
    
    feedbackMessage = "Loaded Mesh: " + rawAssetName;
    feedbackTimer = 5.0f;
}

void NexusManualImporter::RefreshBrowser() {
    for (auto& node : browserNodes) {
        if (node.thumbnail.id != 0) UnloadTexture(node.thumbnail);
    }
    browserNodes.clear();

    if (!fs::exists(currentBrowserDir)) return;

    fs::path p(currentBrowserDir);
    if (p.has_parent_path()) {
        BrowserNode upNode;
        upNode.name = ".. [Up]";
        upNode.path = p.parent_path().string();
        upNode.isDirectory = true;
        upNode.thumbnail = {0}; 
        browserNodes.push_back(upNode);
    }

    std::vector<BrowserNode> dirs;
    std::vector<BrowserNode> files;

    for (const auto& entry : fs::directory_iterator(currentBrowserDir)) {
        BrowserNode node;
        node.name = entry.path().filename().string();
        node.path = entry.path().string();
        node.isDirectory = entry.is_directory();
        node.thumbnail = {0};

        if (node.isDirectory) {
            dirs.push_back(node);
        } else {
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg") {
                node.thumbnail = LoadTexture(node.path.c_str());
                files.push_back(node);
            }
        }
    }

    auto sortFunc = [](const BrowserNode& a, const BrowserNode& b) { return a.name < b.name; };
    std::sort(dirs.begin(), dirs.end(), sortFunc);
    std::sort(files.begin(), files.end(), sortFunc);

    browserNodes.insert(browserNodes.end(), dirs.begin(), dirs.end());
    browserNodes.insert(browserNodes.end(), files.begin(), files.end());
}

void NexusManualImporter::ExtractTextures() {
    if (!isLoaded || currentAssetPath.empty()) return;
    
    std::string command = "py nexus_smart_extractor.py \"" + currentAssetPath + "\"";
    std::system(command.c_str());
    
    RefreshBrowser();
    
    int texCount = 0;
    for (const auto& node : browserNodes) {
        if (!node.isDirectory) texCount++;
    }
    
    feedbackMessage = "Extraction complete. Found " + std::to_string(texCount) + " clean textures.";
    feedbackTimer = 4.0f;
}

void NexusManualImporter::ReloadAndApplyManual() {
    for (auto& tex : loadedPreviewTextures) UnloadTexture(tex);
    loadedPreviewTextures.clear();
    
    UnloadModel(previewModel);
    previewModel = LoadModel(currentAssetPath.c_str());
    
    for (int i = 0; i < previewModel.materialCount; i++) {
        previewModel.materials[i].maps[MATERIAL_MAP_ALBEDO].color = WHITE;
    }

    for (const auto& [matID, maps] : materialAssignments) {
        for (const auto& [mapType, mappedTex] : maps) {
            Texture2D newTex = LoadTexture(mappedTex.path.c_str());
            if (newTex.id != 0) {
                loadedPreviewTextures.push_back(newTex);
                previewModel.materials[matID].maps[mapType].texture = newTex;
            }
        }
    }
}

void NexusManualImporter::UndoAutoMap() {
    if (!isLoaded) return;
    
    bool removedAny = false;
    for (auto it = materialAssignments.begin(); it != materialAssignments.end();) {
        for (auto mapIt = it->second.begin(); mapIt != it->second.end();) {
            if (mapIt->second.isAutoMapped) {
                mapIt = it->second.erase(mapIt);
                removedAny = true;
            } else {
                ++mapIt;
            }
        }
        if (it->second.empty()) it = materialAssignments.erase(it);
        else ++it;
    }
    
    if (removedAny) {
        ReloadAndApplyManual();
        feedbackMessage = "Undid Auto-Map. Kept Manual Textures.";
    } else {
        feedbackMessage = "No Auto-Mapped textures to undo.";
    }
    feedbackTimer = 3.0f;
}

int NexusManualImporter::AutoMapTextures() {
    int mapCount = 0;
    for (const auto& node : browserNodes) {
        if (node.isDirectory) continue;
        std::string name = node.name;
        std::string lowerName = name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        
        int matID = -1;
        size_t firstUS = name.find('_');
        if (firstUS != std::string::npos && firstUS > 0) {
            bool isNum = true;
            for (size_t i = 0; i < firstUS; i++) {
                if (!isdigit(lowerName[i])) { isNum = false; break; }
            }
            if (isNum) {
                try { matID = std::stoi(lowerName.substr(0, firstUS)); } catch (...) {}
            }
        }

        if (matID >= 0 && matID < previewModel.materialCount) {
            auto tryApply = [&](int mapType) {
                if (!materialAssignments[matID].count(mapType) || materialAssignments[matID][mapType].isAutoMapped) {
                    ApplyTextureToMaterial(matID, mapType, node.path, true);
                    mapCount++;
                }
            };

            if (lowerName.find("albedo") != std::string::npos || lowerName.find("basecolor") != std::string::npos || lowerName.find("diffuse") != std::string::npos) {
                tryApply(MATERIAL_MAP_ALBEDO);
            } else if (lowerName.find("normal") != std::string::npos) {
                tryApply(MATERIAL_MAP_NORMAL);
            } else if (lowerName.find("metalrough") != std::string::npos || lowerName.find("metallic") != std::string::npos || lowerName.find("roughness") != std::string::npos) {
                tryApply(MATERIAL_MAP_METALNESS);
            }
        }
    }
    return mapCount;
}

void NexusManualImporter::ApplyTextureToMaterial(int materialIndex, int mapType, const std::string& texturePath, bool isAuto) {
    if (materialIndex < 0 || materialIndex >= previewModel.materialCount) return;
    Texture2D newTex = LoadTexture(texturePath.c_str());
    if (newTex.id != 0) {
        loadedPreviewTextures.push_back(newTex);
        previewModel.materials[materialIndex].maps[mapType].texture = newTex;
        fs::path p(texturePath);
        materialAssignments[materialIndex][mapType] = { texturePath, isAuto }; 
    }
}

void NexusManualImporter::SaveNxMat() {
    if (!isLoaded) return; 
    
    fs::create_directories(currentAssetDir);
    fs::path assetDir(currentAssetDir);
    
    std::map<int, std::map<int, std::string>> localAssignments;

    for (auto const& [matID, maps] : materialAssignments) {
        for (auto const& [mapType, mappedTex] : maps) {
            fs::path src(mappedTex.path);
            fs::path dest = assetDir / src.filename();
            
            if (fs::absolute(src).parent_path() != fs::absolute(assetDir)) {
                try {
                    if (fs::exists(src)) {
                        fs::copy_file(src, dest, fs::copy_options::overwrite_existing);
                        TraceLog(LOG_INFO, "Auto-Copied texture to bundle: %s", dest.string().c_str());
                    }
                } catch(...) {
                    TraceLog(LOG_WARNING, "Failed to copy texture: %s", src.string().c_str());
                }
            }
            
            localAssignments[matID][mapType] = dest.filename().string();
        }
    }

    fs::path savePath = assetDir / (rawAssetName + ".nxmat");
    std::ofstream outFile(savePath);

    if (!outFile.is_open()) {
        TraceLog(LOG_ERROR, "Failed to write .nxmat at %s", savePath.string().c_str());
        feedbackMessage = "Error: Failed to save to asset folder!";
        feedbackTimer = 4.0f;
        return;
    }

    std::string ext = fs::path(currentAssetPath).extension().string();
    outFile << "{\n  \"asset\": \"" << rawAssetName << ext << "\",\n";
    outFile << "  \"type\": \"" << ((currentToggleMode == 1) ? "Character" : "Environment") << "\",\n";
    outFile << "  \"materials\": [\n";

    int matCount = 0;
    for (auto const& [matID, maps] : localAssignments) {
        outFile << "    {\n      \"id\": " << matID << ",\n";
        auto albedoIt = maps.find(MATERIAL_MAP_ALBEDO);
        auto normalIt = maps.find(MATERIAL_MAP_NORMAL);
        auto metalIt = maps.find(MATERIAL_MAP_METALNESS);
        
        auto formatPath = [](const std::string& pathStr) {
            std::string p = pathStr;
            std::replace(p.begin(), p.end(), '\\', '/');
            return p;
        };

        outFile << "      \"albedo\": \"" << (albedoIt != maps.end() ? formatPath(albedoIt->second) : "") << "\",\n";
        outFile << "      \"normal\": \"" << (normalIt != maps.end() ? formatPath(normalIt->second) : "") << "\",\n";
        outFile << "      \"metalness\": \"" << (metalIt != maps.end() ? formatPath(metalIt->second) : "") << "\"\n";
        outFile << "    }";
        matCount++;
        if (matCount < (int)localAssignments.size()) outFile << ",\n";
        else outFile << "\n";
    }
    outFile << "  ]\n}\n";
    outFile.close();
    
    TraceLog(LOG_INFO, "Saved material configuration to %s", savePath.string().c_str());
    feedbackMessage = "Success: Assets bundled & .nxmat Saved!";
    feedbackTimer = 3.0f;
    
    needsRescan = true; 
}

void NexusManualImporter::UpdateViewport(bool isOpen) {
    if (!isOpen || !isLoaded) return;
    
    if (isViewportHovered && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        isCameraDragging = true;
        DisableCursor(); 
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) {
        isCameraDragging = false;
        EnableCursor();
    }

    if (isCameraDragging) {
        Vector2 delta = GetMouseDelta();
        Vector3 movement = { 0.0f, 0.0f, 0.0f };
        float moveSpeed = IsKeyDown(KEY_LEFT_SHIFT) ? 0.6f : 0.2f;

        if (IsKeyDown(KEY_W)) movement.x += moveSpeed;
        if (IsKeyDown(KEY_S)) movement.x -= moveSpeed;
        if (IsKeyDown(KEY_D)) movement.y += moveSpeed;
        if (IsKeyDown(KEY_A)) movement.y -= moveSpeed;
        if (IsKeyDown(KEY_E)) movement.z += moveSpeed;
        if (IsKeyDown(KEY_Q)) movement.z -= moveSpeed;

        Vector3 rotation = { delta.x * 0.15f, delta.y * 0.15f, 0.0f };
        UpdateCameraPro(&previewCamera, movement, rotation, 0.0f);
    }
    
    BeginTextureMode(renderTarget);
        ClearBackground(DARKGRAY);
        BeginMode3D(previewCamera);
            DrawGrid(10, 1.0f);
            DrawModel(previewModel, Vector3Zero(), 1.0f, WHITE);
            
            if (triggerRaycast) {
                triggerRaycast = false;
                
                Vector2 virtualMouse = { 
                    viewportMouseU * (float)GetScreenWidth(), 
                    viewportMouseV * (float)GetScreenHeight() 
                };
                
                Ray ray = GetMouseRay(virtualMouse, previewCamera);
                
                int hitMeshIndex = -1;
                float closestDist = 999999.0f;
                Matrix transform = previewModel.transform; 
                
                for (int i = 0; i < previewModel.meshCount; i++) {
                    RayCollision col = GetRayCollisionMesh(ray, previewModel.meshes[i], transform);
                    if (col.hit && col.distance < closestDist) {
                        closestDist = col.distance;
                        hitMeshIndex = i;
                    }
                }
                
                if (hitMeshIndex != -1) {
                    targetMaterialID = previewModel.meshMaterial[hitMeshIndex];
                    scrollToMaterialID = targetMaterialID; 
                    feedbackMessage = "Selected Material ID: " + std::to_string(targetMaterialID);
                    feedbackTimer = 3.0f;
                }
            }
            
        EndMode3D();
    EndTextureMode();
}

void NexusManualImporter::DrawUI(bool& isOpen) {
    if (!isOpen) return;

    ImGui::SetNextWindowSize(ImVec2(1300, 800), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Nexus Master Importer", &isOpen)) {
        
        ImGui::RadioButton("Environment", &currentToggleMode, 0); ImGui::SameLine();
        ImGui::RadioButton("Character", &currentToggleMode, 1); ImGui::SameLine();
        ImGui::Spacing(); ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();
        
        if (ImGui::Button("BROWSE & LOAD GLB", ImVec2(200, 30))) OpenDialogAndLoad();
        
        if (isLoaded) {
            ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();
            
            ImGui::SetNextItemWidth(200.0f);
            ImGui::InputText("##AssetName", customNameInput, IM_ARRAYSIZE(customNameInput));
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Rename the asset before saving");
            
            ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();
            
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.3f, 1.0f));
            if (ImGui::Button("SAVE MAP & RENAME", ImVec2(180, 30))) {
                std::string newName(customNameInput);
                
                if (!newName.empty() && newName != rawAssetName) {
                    
                    // CRITICAL FIX: Unload the model to release Windows file locks before renaming!
                    UnloadModel(previewModel);
                    for (auto& tex : loadedPreviewTextures) UnloadTexture(tex);
                    loadedPreviewTextures.clear();
                    
                    fs::path oldDir = fs::path(currentAssetDir);
                    fs::path newDir = oldDir.parent_path() / newName;
                    
                    try {
                        // Move the entire folder so we don't leave duplicates in the browser
                        if (fs::exists(oldDir) && oldDir != newDir) {
                            fs::rename(oldDir, newDir); 
                            currentAssetDir = newDir.string();
                            
                            // Now rename the actual 3D file inside the new folder
                            fs::path oldFile = newDir / (rawAssetName + fs::path(currentAssetPath).extension().string());
                            fs::path newFile = newDir / (newName + fs::path(currentAssetPath).extension().string());
                            
                            if (fs::exists(oldFile)) {
                                fs::rename(oldFile, newFile);
                                currentAssetPath = newFile.string();
                            }
                            
                            // Update the memory references for our mapped textures to point to the new folder
                            for (auto& [matID, maps] : materialAssignments) {
                                for (auto& [mapType, mappedTex] : maps) {
                                    fs::path texPath(mappedTex.path);
                                    if (texPath.parent_path() == oldDir) {
                                        mappedTex.path = (newDir / texPath.filename()).string();
                                    }
                                }
                            }
                            rawAssetName = newName;
                        }
                    } catch(const std::exception& e) {
                        TraceLog(LOG_WARNING, "Rename failed: %s", e.what());
                    }
                }
                
                SaveNxMat();
                isLoaded = false; // Mark unloaded so the destructor doesn't crash
                isOpen = false; 
            }
            ImGui::PopStyleColor();
        }
        
        ImGui::Separator();

        if (isLoaded) {
            ImGui::Columns(3, "ImporterColumns", false);
            ImGui::SetColumnWidth(0, 360.0f);
            ImGui::SetColumnWidth(1, 600.0f);

            // --- LEFT COLUMN ---
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Texture Browser");
            std::string shortPath = currentBrowserDir;
            if(shortPath.length() > 40) shortPath = "..." + shortPath.substr(shortPath.length() - 40);
            ImGui::TextDisabled("%s", shortPath.c_str());
            
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.8f, 1.0f));
            if (ImGui::Button("EXTRACT TEXTURES", ImVec2(-1, 26))) {
                ExtractTextures();
            }
            ImGui::PopStyleColor();
            
            if (ImGui::Button("Refresh Folder", ImVec2(-1, 26))) RefreshBrowser();
            ImGui::Separator();
            
            ImGui::BeginChild("FileExplorer", ImVec2(0, 0), true);
            
            float cellSize = 96.0f;
            float panelWidth = ImGui::GetContentRegionAvail().x;
            int columns = std::max(1, (int)(panelWidth / cellSize));
            
            if (ImGui::BeginTable("BrowserGrid", columns)) {
                for (size_t i = 0; i < browserNodes.size(); i++) {
                    ImGui::TableNextColumn();
                    auto& node = browserNodes[i];
                    
                    ImGui::PushID(i);
                    ImGui::BeginGroup();
                    
                    if (node.isDirectory) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f)); 
                        if (ImGui::Button("[ DIR ]", ImVec2(64, 64))) {
                            currentBrowserDir = node.path;
                            RefreshBrowser();
                        }
                        ImGui::TextWrapped("%s", node.name.c_str());
                        ImGui::PopStyleColor();
                    } else {
                        Rectangle sourceRect = { 0, 0, (float)node.thumbnail.width, (float)node.thumbnail.height };
                        rlImGuiImageRect(&node.thumbnail, 64, 64, sourceRect);
                        
                        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                            std::string pathStr = node.path;
                            ImGui::SetDragDropPayload("TEXTURE_PATH", pathStr.c_str(), pathStr.size() + 1);
                            ImGui::Text("Mapping: %s", node.name.c_str());
                            rlImGuiImageRect(&node.thumbnail, 128, 128, sourceRect); 
                            ImGui::EndDragDropSource();
                        }

                        std::string dispName = node.name;
                        if(dispName.length() > 12) dispName = dispName.substr(0, 10) + "..";
                        ImGui::Text("%s", dispName.c_str());
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", node.name.c_str());
                    }
                    
                    ImGui::EndGroup();
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::EndChild();
            ImGui::NextColumn();

            // --- CENTER COLUMN ---
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.4f, 1.0f), "Live Preview");
            ImGui::SameLine();
            ImGui::TextDisabled(" (Right-Click: Fly | Left-Click: Pick Material)");
            ImGui::Separator();
            
            float viewWidth = ImGui::GetColumnWidth();
            float viewHeight = viewWidth * (1080.0f / 1920.0f); 
            Rectangle viewRect = { 0, 0, (float)renderTarget.texture.width, -(float)renderTarget.texture.height };
            
            ImVec2 startPos = ImGui::GetCursorScreenPos();
            rlImGuiImageRect(&renderTarget.texture, (int)viewWidth, (int)viewHeight, viewRect);
            
            ImGui::SetCursorScreenPos(startPos);
            ImGui::InvisibleButton("##ViewportHitbox", ImVec2(viewWidth, viewHeight));
            
            isViewportHovered = ImGui::IsItemHovered();
            
            if (isViewportHovered) {
                ImVec2 mousePos = ImGui::GetMousePos();
                viewportMouseU = (mousePos.x - startPos.x) / viewWidth;
                viewportMouseV = (mousePos.y - startPos.y) / viewHeight;
                
                if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                    triggerRaycast = true;
                }
            }
            
            ImGui::NextColumn();

            // --- RIGHT COLUMN ---
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Material Override");
            
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.8f, 1.0f));
            if (ImGui::Button("AUTO MAP TEXTURES", ImVec2(-1, 30))) {
                int mapped = AutoMapTextures();
                feedbackMessage = "Auto-Map Assigned " + std::to_string(mapped) + " textures.";
                feedbackTimer = 3.0f;
            }
            ImGui::PopStyleColor();
            
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("UNDO AUTO MAP", ImVec2(-1, 30))) {
                UndoAutoMap();
            }
            ImGui::PopStyleColor();
            
            if (feedbackTimer > 0.0f) {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), ">> %s", feedbackMessage.c_str());
                feedbackTimer -= GetFrameTime();
            } else {
                ImGui::Spacing();
            }
            
            ImGui::Separator();
            
            ImGui::BeginChild("MaterialEditor", ImVec2(0, 0), true);

            for (int i = 0; i < previewModel.materialCount; i++) {
                
                if (scrollToMaterialID == i) {
                    ImGui::SetNextItemOpen(true, ImGuiCond_Always);
                }
                
                bool isTarget = (targetMaterialID == i);
                if (isTarget) ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.6f, 0.8f, 1.0f));
                
                bool headerOpen = ImGui::CollapsingHeader(("Material ID: " + std::to_string(i)).c_str(), ImGuiTreeNodeFlags_DefaultOpen);
                
                if (isTarget) ImGui::PopStyleColor();
                
                if (scrollToMaterialID == i) {
                    ImGui::SetScrollHereY(0.5f);
                    scrollToMaterialID = -1; 
                }
                
                if (headerOpen) {
                    auto drawMapSlot = [&](const char* slotName, int mapType) {
                        std::string label = std::string(slotName) + " (Drop Here)";
                        
                        bool isManuallyMapped = materialAssignments[i].count(mapType);
                        
                        bool hasNativeTexture = (!isManuallyMapped && 
                                                 previewModel.materials[i].maps[mapType].texture.id > 1 && 
                                                 previewModel.materials[i].maps[mapType].texture.id != emptyTexture.id);
                        
                        bool isMapped = isManuallyMapped || hasNativeTexture;
                        
                        if (isMapped) {
                            if (isManuallyMapped) {
                                fs::path p(materialAssignments[i][mapType].path);
                                std::string fName = p.filename().string();
                                if (fName.length() > 18) fName = fName.substr(0, 15) + "...";
                                label = std::string(slotName) + ": " + fName;
                            } else {
                                label = std::string(slotName) + ": [Native Found]";
                            }
                            
                            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.3f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.4f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.5f, 0.2f, 1.0f));
                        }

                        if (ImGui::Button((label + "##" + std::to_string(i) + "_" + std::to_string(mapType)).c_str(), ImVec2(-1, 30))) {
                            if (isMapped) {
                                materialAssignments[i].erase(mapType);
                                ReloadAndApplyManual(); 
                                feedbackMessage = "Cleared slot: " + std::string(slotName);
                                feedbackTimer = 2.0f;
                            }
                        }
                        
                        if (isMapped) {
                            ImGui::PopStyleColor(3);
                            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click to clear this texture mapping");
                        }

                        if (ImGui::BeginDragDropTarget()) {
                            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TEXTURE_PATH")) {
                                std::string droppedPath = (const char*)payload->Data;
                                ApplyTextureToMaterial(i, mapType, droppedPath, false);
                                targetMaterialID = i; 
                            }
                            ImGui::EndDragDropTarget();
                        }
                    };

                    drawMapSlot("Albedo", MATERIAL_MAP_ALBEDO);
                    drawMapSlot("Normal Map", MATERIAL_MAP_NORMAL);
                    drawMapSlot("Metal/Rough", MATERIAL_MAP_METALNESS);

                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                }
            }
            
            ImGui::EndChild();
            ImGui::Columns(1);
        } else {
            ImGui::TextDisabled("Select an asset to begin manual mapping.");
        }
    }
    ImGui::End();
}