#pragma once
#include "nexus_core.h"
#include <string>

// Saves a character's custom data to a lightweight text file
void SaveNexusCharacterPrefab(const SceneObject& obj, const std::string& savePath);

// Parses the text file, loads the base mesh, and restores the custom data
SceneObject LoadNexusCharacterPrefab(const std::string& loadPath);