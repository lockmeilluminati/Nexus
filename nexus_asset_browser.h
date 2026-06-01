#ifndef NEXUS_ASSET_BROWSER_H
#define NEXUS_ASSET_BROWSER_H

#include "raylib.h"
#include "nexus_core.h"
#include <vector>
#include <string>

struct Asset {
    std::string name;
    std::string filePath;
    Texture2D thumbnail; // NEW: Holds the 3D snapshot
};

class NexusAssetBrowser {
public:
    void Draw(bool& showAssetBrowser, const std::vector<Asset>& envLibrary, const std::vector<Asset>& charLibrary, SceneObject& previewObject, bool& isPreviewing);
};

#endif