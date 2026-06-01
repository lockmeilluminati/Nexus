#ifndef NEXUS_PROJECT_H
#define NEXUS_PROJECT_H

#include <vector>
#include <string>
#include "raylib.h"
#include "nexus_core.h"
#include "nexus_timeline.h"

void SaveNexusProject(const std::string& filename, const std::vector<SceneObject>& scene, const std::vector<Vector3>& rotations, const NexusTimeline& timeline);

// NEW: Loader Function
void LoadNexusProject(const std::string& filename, std::vector<SceneObject>& scene, std::vector<Vector3>& rotations, NexusTimeline& timeline);

#endif