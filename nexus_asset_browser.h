#pragma once
#include "nexus_core.h" 
#include <vector>

class NexusAssetBrowser {
public:
    void Draw(bool& isOpen,
              std::vector<Asset>& envLibrary,
              std::vector<Asset>& charLibrary,
              SceneObject& previewObject,
              bool& isPreviewing);
};
