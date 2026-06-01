#ifndef NEXUS_TEXTURE_EXTRACTOR_H
#define NEXUS_TEXTURE_EXTRACTOR_H
#include "raylib.h"
#include <string>

// Extracts the .glb via Python, organizes it by category, and returns the .gltf path
std::string IngestAsset(const std::string& sourceFilePath, const std::string& category);

#endif