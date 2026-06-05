#include "nexus_asset_scanner.h"
#include "raymath.h"
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <fstream> 

namespace fs = std::filesystem;

Texture2D GenerateThumbnail(const std::string& path, bool isEnvironment) {
    std::string ext = path.substr(path.find_last_of("."));
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    std::string modelToLoad = path;

    // --- PREFAB THUMBNAIL BYPASS ---
    // If it's a Prefab, we open the text file, find the BaseMesh, and load THAT to make the thumbnail.
    if (ext == ".nxchar") {
        std::ifstream file(path);
        std::string line;
        while(std::getline(file, line)) {
            if (line.find("BaseMesh=") == 0) {
                modelToLoad = line.substr(9);
                break;
            }
        }
    }

    Model model = LoadModel(modelToLoad.c_str());
    
    // Fallback if the model completely fails to load
    if (model.meshCount == 0) {
        Image img = GenImageColor(64, 64, MAGENTA);
        Texture2D tex = LoadTextureFromImage(img);
        UnloadImage(img);
        return tex;
    }

    // Reset native materials
    for (int i = 0; i < model.materialCount; i++) {
        model.materials[i].maps[MATERIAL_MAP_ALBEDO].color = WHITE;
    }

    // Apply texture rescue logic for .glb files
    std::string targetExt = modelToLoad.substr(modelToLoad.find_last_of("."));
    std::transform(targetExt.begin(), targetExt.end(), targetExt.begin(), ::tolower);
    if (targetExt == ".glb") {
        // Check if Raylib already loaded textures natively from the embedded GLB data
        bool nativeTexturesLoaded = false;
        for (int i = 0; i < model.materialCount; i++) {
            if (model.materials[i].maps[MATERIAL_MAP_ALBEDO].texture.id > 1) {
                nativeTexturesLoaded = true;
                break;
            }
        }

        // Only attempt the img-0000 fallback if native load failed
        if (!nativeTexturesLoaded) {
            std::string folder = modelToLoad.substr(0, modelToLoad.find_last_of("/\\"));
            std::string texPathPng = folder + "/img-0000.png";
            std::string texPathJpg = folder + "/img-0000.jpg";
            if (FileExists(texPathPng.c_str())) {
                Texture2D tex = LoadTexture(texPathPng.c_str());
                for (int i = 0; i < model.materialCount; i++) {
                    model.materials[i].maps[MATERIAL_MAP_ALBEDO].texture = tex;
                }
            } else if (FileExists(texPathJpg.c_str())) {
                Texture2D tex = LoadTexture(texPathJpg.c_str());
                for (int i = 0; i < model.materialCount; i++) {
                    model.materials[i].maps[MATERIAL_MAP_ALBEDO].texture = tex;
                }
            }
        }
    }

    BoundingBox bounds = GetModelBoundingBox(model);
    Vector3 center = {
        (bounds.max.x + bounds.min.x) / 2.0f,
        (bounds.max.y + bounds.min.y) / 2.0f,
        (bounds.max.z + bounds.min.z) / 2.0f
    };
    
    float maxSize = fmax(bounds.max.x - bounds.min.x, fmax(bounds.max.y - bounds.min.y, bounds.max.z - bounds.min.z));
    float camDist = maxSize * 1.2f;

    Camera3D thumbCam = { 0 };
    thumbCam.position = { center.x + camDist, center.y + (maxSize * 0.5f), center.z + camDist };
    thumbCam.target = center;
    thumbCam.up = { 0.0f, 1.0f, 0.0f };
    thumbCam.fovy = 45.0f;
    thumbCam.projection = CAMERA_PERSPECTIVE;

    RenderTexture2D target = LoadRenderTexture(64, 64);
    
    BeginTextureMode(target);
        ClearBackground(DARKGRAY);
        BeginMode3D(thumbCam);
            DrawModel(model, Vector3Zero(), 1.0f, WHITE);
        EndMode3D();
    EndTextureMode();

    Image img = LoadImageFromTexture(target.texture);
    ImageFlipVertical(&img); 
    Texture2D finalTex = LoadTextureFromImage(img);
    
    UnloadImage(img);
    UnloadRenderTexture(target);
    UnloadModel(model);

    return finalTex;
}

void ScanForAssets(std::vector<Asset>& envLibrary, std::vector<Asset>& charLibrary) {
    envLibrary.clear();
    charLibrary.clear();

    if (!DirectoryExists("Envs")) MakeDirectory("Envs");
    if (!DirectoryExists("Chars")) MakeDirectory("Chars");

    auto scanDir = [](const std::string& path, std::vector<Asset>& lib, bool isEnv) {
        if (!DirectoryExists(path.c_str())) return;
        for (const auto& entry : fs::recursive_directory_iterator(path)) {
            if (entry.is_regular_file()) {
                std::string filepath = entry.path().string();
                std::replace(filepath.begin(), filepath.end(), '\\', '/');
                std::string ext = filepath.substr(filepath.find_last_of("."));
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                
                // --- .NXCHAR ADDED TO DETECTED FILES ---
                if (ext == ".gltf" || ext == ".glb" || ext == ".nxchar") {
                    std::string filename = entry.path().filename().string();
                    std::string name = filename.substr(0, filename.find_last_of("."));
                    std::replace(name.begin(), name.end(), '_', ' ');

                    lib.push_back({name, filepath, GenerateThumbnail(filepath, isEnv)});
                }
            }
        }
    };

    scanDir("Envs", envLibrary, true);
    scanDir("Chars", charLibrary, false);
}