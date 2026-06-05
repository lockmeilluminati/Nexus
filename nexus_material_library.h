#pragma once
#include "raylib.h"
#include "nexus_core.h"
#include <string>
#include <vector>

// A single skin/material preset
struct CharacterMaterial {
    std::string name;           // e.g. "Light Skin", "Dark Skin", "Zombie"
    std::string albedoPath;     // Path to the diffuse/albedo texture on disk
    Texture2D   albedo  = {0};  // Loaded GPU texture
    bool        loaded  = false;
};

class NexusMaterialLibrary {
public:
    std::vector<CharacterMaterial> materials;
    int selectedIndex = -1;     // Which preset is currently active

    // Scans "Materials/Characters/" folder and loads all presets found
    void ScanAndLoad();

    // Applies the selected material to every mesh on a SceneObject
    void ApplyToObject(SceneObject& obj);

    // Draws the ImGui panel — call from your inspector
    void DrawPanel(SceneObject& obj, bool& isOpen);

    // Unloads all GPU textures (call on shutdown)
    void Unload();

private:
    void LoadMaterialEntry(const std::string& texPath, const std::string& name);
};
