#include "nexus_character.h"
#include "nexus_texture_extractor.h"

SceneObject SpawnCharacter(const std::string& filePath) {
    SceneObject character;
    character.name = "Character";
    
    // 1. Send to the Python Extractor Pipeline (Targeting the "Chars" folder)
    character.filePath = IngestAsset(filePath, "Chars");
    
    // 2. Load the model from the newly archived local .glb
    character.model = LoadModel(character.filePath.c_str());

    // --- 3. TEXTURE RESCUE PASS ---
    // Bypass Raylib's specular extension limitations by manually mapping the glbdump texture
    std::string folder = character.filePath.substr(0, character.filePath.find_last_of("/\\"));
    std::string texPathPng = folder + "/img-0000.png";
    std::string texPathJpg = folder + "/img-0000.jpg";

    if (FileExists(texPathPng.c_str())) {
        Texture2D tex = LoadTexture(texPathPng.c_str());
        for (int i = 0; i < character.model.materialCount; i++) {
            character.model.materials[i].maps[MATERIAL_MAP_ALBEDO].texture = tex;
        }
        TraceLog(LOG_INFO, "TEXTURE RESCUE: Applied img-0000.png manually.");
    } else if (FileExists(texPathJpg.c_str())) {
        Texture2D tex = LoadTexture(texPathJpg.c_str());
        for (int i = 0; i < character.model.materialCount; i++) {
            character.model.materials[i].maps[MATERIAL_MAP_ALBEDO].texture = tex;
        }
        TraceLog(LOG_INFO, "TEXTURE RESCUE: Applied img-0000.jpg manually.");
    }

    // 4. Load the animations 
    int animCount = 0;
    character.anims = LoadModelAnimations(character.filePath.c_str(), &animCount);
    
    if (animCount > 0) {
        character.animCount = animCount;
        character.currentFrame = 60; // Skip the default T-Pose
        character.isAnimated = true;
    } else {
        character.isAnimated = false;
        character.anims = nullptr;
    }
    
    character.position = { 0.0f, 0.0f, 0.0f };
    character.scale = 1.0f;
    character.bounds = GetModelBoundingBox(character.model);
    
    return character;
}