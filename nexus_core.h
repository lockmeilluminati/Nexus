#ifndef NEXUS_CORE_H
#define NEXUS_CORE_H

#include "raylib.h"
#include <string>
#include <vector>

struct SceneObject {
    std::string name;       
    std::string filePath;   
    
    Model model;            
    Vector3 position;       
    float scale;            
    
    bool isAnimated;        
    int currentFrame;       
    ModelAnimation *anims;  
    int animCount;          
    
    BoundingBox bounds;     // For future mouse-clicking
};

// The universal, dynamic Scene Graph list
extern std::vector<SceneObject> scene;

#endif