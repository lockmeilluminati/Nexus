#include "nexus_environment.h"
#include "nexus_texture_extractor.h"

SceneObject SpawnEnvironment(const std::string& filePath) {
    SceneObject env;
    env.name = "Environment";
    
    // 1. Send the .glb to the Python extractor and get the local .gltf path back
    env.filePath = IngestAsset(filePath, "Envs");
    
    // 2. Load the model from the extracted .gltf (Textures load automatically!)
    env.model = LoadModel(env.filePath.c_str());
    
    env.position = { 0.0f, 0.0f, 0.0f }; 
    env.scale = 1.0f;
    env.isAnimated = false;
    env.anims = nullptr;
    env.bounds = GetModelBoundingBox(env.model);
    
    return env;
}