#include "nexus_waypoint.h"
#include "raymath.h"
#include <cmath>

void UpdateCharacterWaypoints(SceneObject& obj, Vector3& rotation, int currentFrame, int startFrame) {
    if (obj.waypoints.empty()) return;

    int activeFrames = currentFrame - startFrame;
    if (activeFrames <= 0) {
        obj.position = obj.waypoints[0];
        if (obj.waypoints.size() > 1) {
            Vector3 dir = Vector3Subtract(obj.waypoints[1], obj.waypoints[0]);
            rotation.y = atan2f(dir.x, dir.z) * RAD2DEG;
        }
        return;
    }

    float timeActive = (float)activeFrames / 60.0f;
    float distanceToTravel = timeActive * obj.walkSpeed;

    // --- NEW: LOOP MATH ---
    // If the loop flag is checked, we calculate the total perimeter of the path
    float totalPathLength = 0.0f;
    for (size_t i = 0; i < obj.waypoints.size() - 1; i++) {
        totalPathLength += Vector3Distance(
            {obj.waypoints[i].x, 0, obj.waypoints[i].z}, 
            {obj.waypoints[i+1].x, 0, obj.waypoints[i+1].z}
        );
    }
    
    // Add the distance from the last node back to the first node to close the shape
    if (obj.loopWaypoints && obj.waypoints.size() > 1) {
        totalPathLength += Vector3Distance(
            {obj.waypoints.back().x, 0, obj.waypoints.back().z}, 
            {obj.waypoints[0].x, 0, obj.waypoints[0].z}
        );
        
        // Use modulus to wrap the distance traveled infinitely!
        if (totalPathLength > 0) {
            distanceToTravel = fmodf(distanceToTravel, totalPathLength);
        }
    }

    // Traversal Check
    size_t nodeCount = obj.loopWaypoints ? obj.waypoints.size() : obj.waypoints.size() - 1;
    
    for (size_t i = 0; i < nodeCount; i++) {
        Vector3 p1 = obj.waypoints[i];
        
        // If looping, the point after the last point is point 0
        Vector3 p2 = (i + 1 == obj.waypoints.size()) ? obj.waypoints[0] : obj.waypoints[i+1];
        
        Vector3 dir = Vector3Subtract(p2, p1);
        dir.y = 0; 
        float segmentLength = Vector3Length(dir);

        if (distanceToTravel <= segmentLength) {
            float t = (segmentLength > 0.0f) ? (distanceToTravel / segmentLength) : 0.0f;
            obj.position.x = p1.x + dir.x * t;
            obj.position.z = p1.z + dir.z * t;
            obj.position.y = p1.y; 

            if (segmentLength > 0.001f) {
                Vector3 dirNorm = Vector3Normalize(dir);
                rotation.y = atan2f(dirNorm.x, dirNorm.z) * RAD2DEG;
            }
            return; 
        } else {
            distanceToTravel -= segmentLength;
        }
    }

    if (!obj.loopWaypoints) {
        obj.position = obj.waypoints.back();
    }
}

void DrawWaypointPath(const SceneObject& obj) {
    if (obj.waypoints.empty()) return;

    for (size_t i = 0; i < obj.waypoints.size(); i++) {
        DrawSphere(obj.waypoints[i], 0.15f, RED);
        if (i > 0) {
            DrawLine3D(obj.waypoints[i-1], obj.waypoints[i], Fade(RED, 0.5f));
        }
    }
    
    // Visually close the loop in the editor with a distinct orange line
    if (obj.loopWaypoints && obj.waypoints.size() > 2) {
        DrawLine3D(obj.waypoints.back(), obj.waypoints[0], Fade(ORANGE, 0.5f));
    }
}