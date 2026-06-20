#include "nexus_splash.h"
#include <cstdio>
#include <string>

std::vector<std::string> engineLogs;
std::mutex logMutex;

// Intercepts the logs and pushes them to our thread-safe buffer
void NexusLogCallback(int msgType, const char *text, va_list args) {
    char buffer[1024]; 
    vsnprintf(buffer, sizeof(buffer), text, args);
    
    std::string logStr(buffer);
    
    // THE SMART FILTER: Silently drop the GLB index spam so it doesn't flood the TV!
    if (logStr.find("Indices data converted from u32 to u16") != std::string::npos) {
        return; 
    }
    
    std::lock_guard<std::mutex> lock(logMutex);
    engineLogs.push_back(logStr);
    
    // Keep the buffer strictly limited to 100 lines so it doesn't eat memory
    if (engineLogs.size() > 100) {
        engineLogs.erase(engineLogs.begin());
    }
}

void NexusSplash::Init() {
    splashImage = LoadTexture("loadingscreen.png");
}

void NexusSplash::Shutdown() {
    UnloadTexture(splashImage);
}

void NexusSplash::DrawSplashFrame() {
    // Perfectly fit the image to the window size
    Rectangle source = { 0.0f, 0.0f, (float)splashImage.width, (float)splashImage.height };
    Rectangle dest = { 0.0f, 0.0f, (float)GetScreenWidth(), (float)GetScreenHeight() };
    DrawTexturePro(splashImage, source, dest, {0.0f, 0.0f}, 0.0f, WHITE);

    // Mask the text so it only renders strictly inside the TV screen bounds
    BeginScissorMode((int)tvBounds.x, (int)tvBounds.y, (int)tvBounds.width, (int)tvBounds.height);
    
    // Tint the TV screen slightly so the bright green text pops
    DrawRectangleRec(tvBounds, ColorAlpha(BLACK, 0.85f)); 

    int fontSize = 10;
    int lineSpacing = 14;
    int maxWidth = (int)(tvBounds.width - 16); // Leave a small pixel padding on the sides

    std::vector<std::string> wrappedLines;
    
    // Lock the mutex while we read the logs and calculate the hard-wrapping
    {
        std::lock_guard<std::mutex> lock(logMutex);
        for (const auto& log : engineLogs) {
            std::string currentLine = "";
            for (char c : log) {
                // Respect natural line breaks
                if (c == '\n') {
                    wrappedLines.push_back(currentLine);
                    currentLine = "";
                    continue;
                }
                
                // Classic Terminal Hard-Wrap: Break the moment a character exceeds the TV width
                std::string testLine = currentLine + c;
                if (MeasureText(testLine.c_str(), fontSize) > maxWidth) {
                    wrappedLines.push_back(currentLine);
                    currentLine = std::string(1, c);
                } else {
                    currentLine += c;
                }
            }
            if (!currentLine.empty()) {
                wrappedLines.push_back(currentLine);
            }
        }
    } 

    // Render the wrapped text bottom-up so the newest logs stay at the bottom of the TV
    int currentY = (tvBounds.y + tvBounds.height) - lineSpacing - 4;
    for (int i = wrappedLines.size() - 1; i >= 0; --i) {
        if (currentY < tvBounds.y) break; // Stop drawing once we hit the top of the monitor
        
        DrawText(wrappedLines[i].c_str(), tvBounds.x + 8, currentY, fontSize, GREEN);
        currentY -= lineSpacing;
    }

    EndScissorMode();
}