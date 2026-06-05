#ifndef NEXUS_ASSET_SCANNER_H
#define NEXUS_ASSET_SCANNER_H

#include <vector>
#include <string>
#include "raylib.h"
#include "nexus_core.h"

// Expose the thumbnail generator to the main engine
Texture2D GenerateThumbnail(const std::string& path, bool isEnv);

// Expose the scanner module
void ScanForAssets(std::vector<Asset>& envLibrary, std::vector<Asset>& charLibrary);

#endif