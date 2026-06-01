#include "nexus_asset_scanner.h"
#include "nexus_environment.h" // NEW: Required for the pipeline
#include "nexus_character.h"   // NEW: Required for the pipeline
#include "raylib.h"
#include "raymath.h" 
#include <filesystem>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

std::string FormatAssetName(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    std::string name = (pos != std::string::npos) ? path.substr(pos + 1) : path;
    size_t ext = name.find_last_of(".");
    if (ext != std::string::npos) name = name.substr(0, ext);
    std::replace(name.begin(), name.end(), '_', ' '); 
    return name;
}

// FIX: Now accepts `isEnv` so it can route through your custom texture pipeline
Texture2D GenerateThumbnail(const std::string& path, bool isEnv) {
    
    // 1. Run the asset through your official Python/glbdump pipeline!
    SceneObject obj = isEnv ? SpawnEnvironment(path) : SpawnCharacter(path);
    
    BoundingBox box = GetModelBoundingBox(obj.model);
    Vector3 center = {
        (box.max.x + box.min.x) / 2.0f,
        (box.max.y + box.min.y) / 2.0f,
        (box.max.z + box.min.z) / 2.0f
    };
    
    float maxDim = fmax(box.max.x - box.min.x, fmax(box.max.y - box.min.y, box.max.z - box.min.z));
    if (maxDim < 0.1f) maxDim = 1.0f; 

    Camera3D thumbCam = {0};
    thumbCam.target = center;
    thumbCam.position = { center.x + maxDim * 1.5f, center.y + maxDim * 1.2f, center.z + maxDim * 1.5f };
    thumbCam.up = { 0.0f, 1.0f, 0.0f };
    thumbCam.fovy = 45.0f;
    thumbCam.projection = CAMERA_PERSPECTIVE;

    RenderTexture2D target = LoadRenderTexture(64, 64);
    BeginTextureMode(target);
        ClearBackground(Fade(DARKGRAY, 0.8f));
        BeginMode3D(thumbCam);
            DrawModel(obj.model, Vector3Zero(), 1.0f, WHITE);
        EndMode3D();
    EndTextureMode();

    // 2. Clean up the heavy 3D data so we don't leak RAM
    UnloadModel(obj.model); 
    if (obj.isAnimated && obj.anims != nullptr) {
        UnloadModelAnimations(obj.anims, obj.animCount);
    }

    Image img = LoadImageFromTexture(target.texture);
    ImageFlipVertical(&img); 
    Texture2D thumb = LoadTextureFromImage(img);
    UnloadImage(img);
    UnloadRenderTexture(target);

    return thumb;
}

void ScanForAssets(std::vector<Asset>& envLibrary, std::vector<Asset>& charLibrary) {
    for(auto& asset : envLibrary) UnloadTexture(asset.thumbnail);
    for(auto& asset : charLibrary) UnloadTexture(asset.thumbnail);

    fs::create_directories("Envs"); 
    fs::create_directories("Chars");
    envLibrary.clear();
    charLibrary.clear();

    for (const auto& entry : fs::recursive_directory_iterator("Envs")) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::tolower(c); });
            if (ext == ".glb" || ext == ".gltf") {
                std::string cleanPath = entry.path().string();
                std::replace(cleanPath.begin(), cleanPath.end(), '\\', '/');
                envLibrary.push_back({FormatAssetName(cleanPath), cleanPath, GenerateThumbnail(cleanPath, true)});
            }
        }
    }

    for (const auto& entry : fs::recursive_directory_iterator("Chars")) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::tolower(c); });
            if (ext == ".glb" || ext == ".gltf") {
                std::string cleanPath = entry.path().string();
                std::replace(cleanPath.begin(), cleanPath.end(), '\\', '/');
                charLibrary.push_back({FormatAssetName(cleanPath), cleanPath, GenerateThumbnail(cleanPath, false)});
            }
        }
    }
}