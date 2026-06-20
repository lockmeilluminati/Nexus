#pragma once
#include "raylib.h"
#include <vector>
#include <string>
#include <mutex>

void NexusLogCallback(int msgType, const char *text, va_list args);

class NexusSplash {
public:
    void Init();
    void Shutdown();
    void DrawSplashFrame();

    // PERFECTED TV BOUNDS locked in from your screenshot!
    Rectangle tvBounds = { 965.0f, 499.0f, 273.0f, 155.0f }; 
    Texture2D splashImage;
};

extern std::vector<std::string> engineLogs;
extern std::mutex logMutex;