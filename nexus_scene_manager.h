#pragma once
#include "nexus_core.h"
#include "nexus_timeline.h"
#include <vector>

class NexusSceneManager {
public:
    void Draw(bool& isOpen,
              std::vector<SceneObject>& scene,
              std::vector<Vector3>& sceneRotations,
              NexusTimeline& timeline,
              int& selectedObjectIndex);
};
