#pragma once
#include "raylib.h"
#include "nexus_core.h"
#include <string>
#include <vector>

// Per-material slot override — lets you colour skin, clothing, armour separately
struct MaterialSlotOverride {
    std::string label       = "Material";
    Color       color       = WHITE;
    bool        enabled     = true;     // toggle per-slot
};

// A saved character appearance preset
struct CharacterPreset {
    std::string name;
    std::vector<MaterialSlotOverride> slots;
};

class NexusCharacterCreator {
public:
    std::vector<MaterialSlotOverride> slots;    // One entry per material on the loaded model
    std::vector<CharacterPreset>      presets;  // Saved looks

    // Call once after a character is loaded/selected — reads material count from obj
    void InitFromObject(const SceneObject& obj);

    // Applies current slot colours to the object every frame (call after InitFromObject)
    void ApplyToObject(SceneObject& obj);

    // ImGui panel — pass your currently selected SceneObject
    void Draw(SceneObject& obj, bool& isOpen);

private:
    char presetNameBuf[64] = "";
    void SavePreset();
    void LoadPreset(int index, SceneObject& obj);

    // Built-in skin tone palette
    static const int PALETTE_SIZE = 12;
    static const Color skinPalette[PALETTE_SIZE];
    static const char* skinPaletteLabels[PALETTE_SIZE];
};
