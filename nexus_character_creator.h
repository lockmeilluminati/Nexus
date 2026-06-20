#pragma once
#include "raylib.h"
#include "nexus_core.h"
#include <string>
#include <vector>

struct MaterialSlotOverride {
    std::string label   = "Material";
    Color       color   = WHITE;
    bool        enabled = true;
};

struct CharacterPreset {
    std::string name;
    std::vector<MaterialSlotOverride> slots;
};

class NexusCharacterCreator {
public:
    std::vector<MaterialSlotOverride> slots;
    std::vector<CharacterPreset>      presets;
    bool  sendToPlayer    = false;
    bool  hasPendingColor = false;
    Color pendingColor    = WHITE;
    
    // NEW: The kill switch to return to the hub menu
    bool  requestClear    = false;

    void InitFromObject(const SceneObject& obj);
    void ApplyToObject(SceneObject& obj);
    void Draw(SceneObject& obj, bool& isOpen);

private:
    char        presetNameBuf[64] = "";
    std::string trackedFilePath;
    int         skinTargetSlot = 0;

    void DrawColorTab(SceneObject& obj); 
    void SavePreset();
    void LoadPreset(int index, SceneObject& obj);

    static const int   PALETTE_SIZE = 12;
    static const Color skinPalette[PALETTE_SIZE];
    static const char* skinPaletteLabels[PALETTE_SIZE];
};