#include "nexus_3dviewportcontrols.h"
#include "raymath.h"

void UpdateViewportCamera(Camera3D& camera) {
    float moveSpeed = 0.5f;
    float turnSpeed = 0.05f;

    // Calculate which way the camera is currently facing
    Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));

    // --- 1. WASD MOVEMENT ---
    if (IsKeyDown(KEY_W)) {
        camera.position = Vector3Add(camera.position, Vector3Scale(forward, moveSpeed));
        camera.target = Vector3Add(camera.target, Vector3Scale(forward, moveSpeed));
    }
    if (IsKeyDown(KEY_S)) {
        camera.position = Vector3Subtract(camera.position, Vector3Scale(forward, moveSpeed));
        camera.target = Vector3Subtract(camera.target, Vector3Scale(forward, moveSpeed));
    }
    if (IsKeyDown(KEY_D)) {
        camera.position = Vector3Add(camera.position, Vector3Scale(right, moveSpeed));
        camera.target = Vector3Add(camera.target, Vector3Scale(right, moveSpeed));
    }
    if (IsKeyDown(KEY_A)) {
        camera.position = Vector3Subtract(camera.position, Vector3Scale(right, moveSpeed));
        camera.target = Vector3Subtract(camera.target, Vector3Scale(right, moveSpeed));
    }

    // --- 2. TURNING: ARROW KEYS ---
    float yaw = 0.0f;
    float pitch = 0.0f;

    if (IsKeyDown(KEY_LEFT))  yaw -= turnSpeed;
    if (IsKeyDown(KEY_RIGHT)) yaw += turnSpeed;
    if (IsKeyDown(KEY_UP))    pitch -= turnSpeed;
    if (IsKeyDown(KEY_DOWN))  pitch += turnSpeed;

    // --- 3. TURNING: MOUSE DRAG ---
    // Hold Right-Click to look around like in Blender/Unreal
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) { 
        Vector2 mouseDelta = GetMouseDelta();
        yaw -= mouseDelta.x * 0.005f;
        pitch -= mouseDelta.y * 0.005f;
    }

    // --- APPLY ROTATION ---
    if (yaw != 0.0f || pitch != 0.0f) {
        Matrix rotation = MatrixIdentity();
        rotation = MatrixMultiply(rotation, MatrixRotate(camera.up, yaw));
        rotation = MatrixMultiply(rotation, MatrixRotate(right, pitch));

        forward = Vector3Transform(forward, rotation);
        camera.target = Vector3Add(camera.position, forward);
    }
}