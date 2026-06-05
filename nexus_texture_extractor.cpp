#include "nexus_texture_extractor.h"
#include "raylib.h"
#include <filesystem>
#include <cstdlib>
#include <algorithm>
#include <string>

namespace fs = std::filesystem;

std::string IngestAsset(const std::string& sourceFilePath, const std::string& category) {
    fs::create_directories(category);

    fs::path sourcePath(sourceFilePath);
    std::string fileName = sourcePath.filename().string();
    std::string ext = sourcePath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    std::string rawName;
    if (ext == ".gltf") {
        rawName = sourcePath.parent_path().filename().string();
    } else {
        rawName = sourcePath.stem().string();
    }

    std::string localDir = category + "/" + rawName;
    fs::create_directories(localDir);
    
    fs::path localAssetPath = fs::path(localDir) / fileName;
    std::string localAssetPathStr = localAssetPath.string();

    // Prevent hard crashes if the user selects a file ALREADY in the target directory
    bool isSameFile = false;
    if (fs::exists(localAssetPath) && fs::exists(sourcePath)) {
        if (fs::equivalent(sourcePath, localAssetPath)) {
            isSameFile = true;
        }
    }

    // ---------------------------------------------------------------
    // .gltf
    // ---------------------------------------------------------------
    if (ext == ".gltf") {
        fs::path sourceDir = sourcePath.parent_path();
        fs::path targetDir = fs::path(localDir);
        
        bool isSameDir = false;
        if (fs::exists(targetDir) && fs::exists(sourceDir)) {
            if (fs::equivalent(sourceDir, targetDir)) {
                isSameDir = true;
            }
        }

        if (!isSameDir) {
            try {
                fs::copy(sourceDir, targetDir,
                         fs::copy_options::overwrite_existing | fs::copy_options::recursive);
                TraceLog(LOG_INFO, TextFormat("PIPELINE: glTF directory archived to %s", localDir.c_str()));
            } catch (const fs::filesystem_error& e) {
                TraceLog(LOG_ERROR, TextFormat("PIPELINE: Failed to copy glTF: %s", e.what()));
            }
        } else {
            TraceLog(LOG_INFO, "PIPELINE: Source and target are the same directory. Skipping copy.");
        }
        return localAssetPathStr;
    }

    // ---------------------------------------------------------------
    // .glb
    // ---------------------------------------------------------------
    if (ext == ".glb") {
        if (!isSameFile) {
            try {
                fs::copy_file(sourcePath, localAssetPath, fs::copy_options::overwrite_existing);
                TraceLog(LOG_INFO, TextFormat("PIPELINE: GLB copied to %s", localAssetPathStr.c_str()));
            } catch (const fs::filesystem_error& e) {
                TraceLog(LOG_ERROR, TextFormat("PIPELINE: Failed to copy GLB: %s", e.what()));
            }
        } else {
            TraceLog(LOG_INFO, "PIPELINE: GLB already in target location. Skipping copy.");
        }

        // Always run the repacker just in case it hasn't been formatted yet
        std::string repackCmd = "py nexus_glb_repacker.py \"" + localAssetPathStr + "\"";
        int result = std::system(repackCmd.c_str());

        if (result == 0) {
            TraceLog(LOG_INFO, "PIPELINE: GLB repacked — all textures now Raylib-compatible.");
        } else {
            TraceLog(LOG_WARNING, "PIPELINE: Repacker failed or not found. Textures may be white.");
        }

        return localAssetPathStr;
    }

    return sourceFilePath;
}