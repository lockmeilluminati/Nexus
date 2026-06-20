#pragma once
#include "raylib.h"
#include "imgui.h"

class NexusTimelineScroll {
public:
    float zoomScale = 1.0f;
    float scrollX = 0.0f;
    float exactScrollTrack = 0.0f; // Floating point allows buttery smooth scrolling
    int scrollTrackIndex = 0;

    // Timeline UI Constants
    float scrollbarH = 14.0f;
    float rulerH = 20.0f;
    float topMargin = 30.0f;

    // Processes mouse wheel and dragging logic cleanly
    void ProcessInput(ImVec2 tPos, float tWidth, float tHeight, float trackH, int totalTracks);
    
    // Renders the horizontal and vertical scrollbars
    void DrawScrollbars(ImDrawList* dl, ImVec2 tPos, float tWidth, float tHeight, float headerW, float trackH, int totalTracks, int maxTimelineFrames);

    // Global Math Helpers to place tracks accurately on screen
    float GetX(int frame, ImVec2 tPos, float headerW, float tWidth, int maxTimelineFrames) const;
    float GetFrameDelta(float mouseDeltaX, float headerW, float tWidth, int maxTimelineFrames) const;
    float GetVisibleHeight(float tHeight) const;
    int GetMaxScrollTracks(float trackH, int totalTracks, float tHeight) const;
};