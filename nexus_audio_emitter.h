#pragma once
#include "raylib.h"
#include "raymath.h"
#include <string>
#include <vector>

enum class AudioPlaybackMode { SEQUENTIAL, RANDOM };
enum class AudioTriggerType { PROXIMITY, INTERACT };

struct AudioDialogLine {
    std::string filePath;
    Sound sound;
    bool isLoaded = false;
};

struct CharacterAudioComponent {
    std::vector<AudioDialogLine> playlist;
    AudioPlaybackMode playbackMode = AudioPlaybackMode::SEQUENTIAL;
    AudioTriggerType triggerType   = AudioTriggerType::PROXIMITY;
    
    float triggerRadius = 5.0f;
    float cooldownTime  = 2.0f;
    int   interactKey   = KEY_E; // Default to E

    // Internal state
    float currentCooldown = 0.0f;
    int   currentIndex    = 0;
    int   currentlyPlayingIndex = -1;
    bool  playerIsInRange = false;

    void AddLine(const std::string& path);
    void RemoveLine(int index);
    
    // Core Logic
    void Update(Vector3 playerPos, Vector3 myPos, float dt);
    
    // UI & Rendering
    void DrawUI(); 
    void DrawDebugZone(Vector3 myPos); // Draws the green ring in editor
    void DrawGameplayPrompt(int screenWidth, int screenHeight); // Draws "Press E" on screen
    
    void UnloadAll();
    std::string GetKeyName() const;
};