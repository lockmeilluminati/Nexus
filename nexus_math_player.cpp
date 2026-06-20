#include "nexus_math_player.h"

void NexusMathPlayer::Init() {
    camera = { 0 };
    camera.position = { 0.0f, 2.0f, 4.0f };
    camera.target = { 0.0f, 2.0f, 0.0f };
    camera.up = { 0.0f, 1.0f, 0.0f };
    camera.fovy = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    isActive = false;
    yaw = -90.0f; // Start looking forward down the -Z axis
    pitch = 0.0f;
    walkBobTime = 0.0f;
    punchExtension = 0.0f;
    isPunching = false;
}

void NexusMathPlayer::Update() {
    if (!isActive) return;

    float dt = GetFrameTime();

    // 1. Independent Mouse Look (Hold Right-Click)
    // Allows you to aim, but leaves cursor free otherwise for UI
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        DisableCursor();
        Vector2 md = GetMouseDelta();
        yaw += md.x * 0.15f;
        pitch -= md.y * 0.15f;

        // Clamp pitch to prevent backflips
        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;
    } else {
        EnableCursor();
    }

    // 2. Calculate View Direction Vector (Euler Math)
    Vector3 direction;
    direction.x = cos(DEG2RAD * pitch) * cos(DEG2RAD * yaw);
    direction.y = sin(DEG2RAD * pitch);
    direction.z = cos(DEG2RAD * pitch) * sin(DEG2RAD * yaw);
    direction = Vector3Normalize(direction);

    // 3. Independent WASD Movement (Always works, no clicking required)
    Vector3 forward = direction;
    forward.y = 0.0f; // Keep movement flat on the floor
    forward = Vector3Normalize(forward);
    
    // Find the right vector relative to where we are looking
    Vector3 right = Vector3CrossProduct(forward, camera.up);
    right = Vector3Normalize(right);

    Vector3 velocity = { 0.0f, 0.0f, 0.0f };
    if (IsKeyDown(KEY_W)) velocity = Vector3Add(velocity, forward);
    if (IsKeyDown(KEY_S)) velocity = Vector3Subtract(velocity, forward);
    if (IsKeyDown(KEY_A)) velocity = Vector3Subtract(velocity, right); // A = left
    if (IsKeyDown(KEY_D)) velocity = Vector3Add(velocity, right);      // D = right

    if (Vector3Length(velocity) > 0.01f) {
        velocity = Vector3Normalize(velocity);
        camera.position = Vector3Add(camera.position, Vector3Scale(velocity, 6.0f * dt));
        walkBobTime += dt * 12.0f; // Fast walk bob
    } else {
        walkBobTime += dt * 2.0f;  // Slow idle breath bob
    }

    // Update Camera Target based on our new position and aim direction
    camera.target = Vector3Add(camera.position, direction);

    // 4. Punching Mechanics (Left Click)
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !isPunching) {
        isPunching = true;
        punchExtension = 1.0f; 
    }

    // Smooth punch retraction
    if (isPunching) {
        punchExtension = Lerp(punchExtension, 0.0f, dt * 15.0f);
        if (punchExtension < 0.01f) {
            punchExtension = 0.0f;
            isPunching = false;
        }
    }
}

void NexusMathPlayer::DrawProceduralArms() {
    if (!isActive) return;

    Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
    Vector3 trueUp = Vector3CrossProduct(right, forward);

    float verticalBob = sin(walkBobTime) * 0.05f;
    
    // RIGHT ARM
    Vector3 rightArmPos = camera.position;
    rightArmPos = Vector3Add(rightArmPos, Vector3Scale(forward, 0.8f + punchExtension));
    rightArmPos = Vector3Add(rightArmPos, Vector3Scale(right, 0.4f));
    rightArmPos = Vector3Add(rightArmPos, Vector3Scale(trueUp, -0.3f + verticalBob));

    // LEFT ARM
    Vector3 leftArmPos = camera.position;
    leftArmPos = Vector3Add(leftArmPos, Vector3Scale(forward, 0.8f));
    leftArmPos = Vector3Add(leftArmPos, Vector3Scale(right, -0.4f));
    leftArmPos = Vector3Add(leftArmPos, Vector3Scale(trueUp, -0.3f + verticalBob));

    DrawCube(rightArmPos, 0.2f, 0.2f, 0.8f, ORANGE);
    DrawCubeWires(rightArmPos, 0.2f, 0.2f, 0.8f, DARKGRAY);

    DrawCube(leftArmPos, 0.2f, 0.2f, 0.8f, ORANGE);
    DrawCubeWires(leftArmPos, 0.2f, 0.2f, 0.8f, DARKGRAY);
}