#pragma once
#include "nexus_character.h"
#include "raylib.h"

// Deterministic evaluator: calculates exact position based on timeline frame
void UpdateCharacterWaypoints(SceneObject& obj, Vector3& rotation, int currentFrame, int startFrame);

void DrawWaypointPath(const SceneObject& obj);