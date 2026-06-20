#pragma once
#include "raylib.h"
#include <string>

enum class TextEffect { NONE, WAVE, SHAKE, TYPEWRITER };

struct CinematicText {
    std::string trackName = "Text Track";
    std::string text = "THE NEXUS PROJECT";
    std::string fontPath = "";
    Font font = { 0 };
    bool isLoaded = false;
    
    int startFrame = 0;
    int endFrame = 300;

    Vector2 position = { 100.0f, 100.0f };
    float fontSize = 50.0f;
    float fontSpacing = 2.0f; 
    Color color = WHITE;

    // Rich Text formatting
    bool isBold = false;
    bool hasShadow = false;

    TextEffect effect = TextEffect::NONE;
    float effectSpeed = 8.0f;
    float effectIntensity = 4.0f;
};

class NexusTextManager {
public:
    void LoadTextFont(CinematicText& track);
    void UnloadTextFont(CinematicText& track);
    
    void DrawCinematicText(CinematicText& track, int currentGlobalFrame);
    
    // UPDATED: Now requires selectedTextIndex so it can close the panel
    void DrawTextEditorLayout(CinematicText& track, int& selectedTextIndex);
};