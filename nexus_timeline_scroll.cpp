#include "nexus_timeline_scroll.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>

float NexusTimelineScroll::GetVisibleHeight(float tHeight) const {
    return tHeight - topMargin - rulerH - scrollbarH - 5.0f;
}

int NexusTimelineScroll::GetMaxScrollTracks(float trackH, int totalTracks, float tHeight) const {
    float visibleH = GetVisibleHeight(tHeight);
    float totalH = totalTracks * trackH;
    return std::max(0, (int)std::ceil((totalH - visibleH) / trackH));
}

float NexusTimelineScroll::GetX(int frame, ImVec2 tPos, float headerW, float tWidth, int maxTimelineFrames) const {
    float tlW = tWidth - headerW;
    float virtualW = tlW * zoomScale;
    return tPos.x + headerW - scrollX + (virtualW * ((float)frame / maxTimelineFrames));
}

float NexusTimelineScroll::GetFrameDelta(float mouseDeltaX, float headerW, float tWidth, int maxTimelineFrames) const {
    float tlW = tWidth - headerW;
    float virtualW = tlW * zoomScale;
    return (mouseDeltaX / virtualW) * maxTimelineFrames;
}

void NexusTimelineScroll::ProcessInput(ImVec2 tPos, float tWidth, float tHeight, float trackH, int totalTracks) {
    bool isHoveringTimeline = ImGui::IsMouseHoveringRect(tPos, ImVec2(tPos.x + tWidth, tPos.y + tHeight));
    int maxScrollTracks = GetMaxScrollTracks(trackH, totalTracks, tHeight);

    if (isHoveringTimeline) {
        ImGuiIO& io = ImGui::GetIO();
        if (io.KeyCtrl && io.MouseWheel != 0.0f) {
            zoomScale += io.MouseWheel * 0.2f;
            if (zoomScale < 1.0f) zoomScale = 1.0f;
            if (zoomScale > 20.0f) zoomScale = 20.0f;
        } else if (io.KeyShift && io.MouseWheel != 0.0f) {
            scrollX -= io.MouseWheel * 50.0f; 
        } else if (io.MouseWheel != 0.0f) {
            // Scroll 2.5 tracks per wheel notch for speed
            exactScrollTrack -= io.MouseWheel * 2.5f; 
        }
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            scrollX -= io.MouseDelta.x;
        }
    }
    
    // Clamp Vertical Scroll
    if (exactScrollTrack < 0.0f) exactScrollTrack = 0.0f;
    if (exactScrollTrack > (float)maxScrollTracks) exactScrollTrack = (float)maxScrollTracks;
    scrollTrackIndex = (int)std::round(exactScrollTrack);

    // Clamp Horizontal Scroll
    float tlW = tWidth - 240.0f; // Header Width
    float virtualW = tlW * zoomScale;               
    float maxScroll = std::max(0.0f, virtualW - tlW);
    if (scrollX < 0.0f) scrollX = 0.0f;
    if (scrollX > maxScroll) scrollX = maxScroll;
}

void NexusTimelineScroll::DrawScrollbars(ImDrawList* dl, ImVec2 tPos, float tWidth, float tHeight, float headerW, float trackH, int totalTracks, int maxTimelineFrames) {
    float tlW = tWidth - headerW;
    float virtualW = tlW * zoomScale;
    float maxScroll = std::max(0.0f, virtualW - tlW);
    
    // --- HORIZONTAL SCROLLBAR ---
    float hScrollbarY = tPos.y + tHeight - scrollbarH;
    float hScrollbarX = tPos.x + headerW;

    dl->AddRectFilled(ImVec2(hScrollbarX, hScrollbarY), ImVec2(hScrollbarX + tlW, hScrollbarY + scrollbarH), IM_COL32(20, 20, 25, 255));
    
    if (virtualW > tlW) {
        float viewRatio = tlW / virtualW;
        float thumbW = std::max(30.0f, tlW * viewRatio);
        float maxThumbX = tlW - thumbW;
        float thumbX = hScrollbarX + (scrollX / maxScroll) * maxThumbX;

        ImGui::SetCursorScreenPos(ImVec2(thumbX, hScrollbarY));
        ImGui::InvisibleButton("##hscrollbar_thumb", ImVec2(thumbW, scrollbarH));
        
        bool scrollThumbActive = ImGui::IsItemActive();
        bool scrollThumbHovered = ImGui::IsItemHovered();

        if (scrollThumbActive && maxThumbX > 0.0f) {
            float dragDelta = ImGui::GetIO().MouseDelta.x;
            scrollX += (dragDelta / maxThumbX) * maxScroll;
        }
        scrollX = std::clamp(scrollX, 0.0f, maxScroll);

        ImU32 thumbCol = scrollThumbActive ? IM_COL32(110, 110, 120, 255) : 
                        (scrollThumbHovered ? IM_COL32(90, 90, 100, 255) : IM_COL32(70, 70, 80, 255));
        dl->AddRectFilled(ImVec2(thumbX, hScrollbarY + 2), ImVec2(thumbX + thumbW, hScrollbarY + scrollbarH - 2), thumbCol, 4.0f);
    }

    // --- VERTICAL SCROLLBAR ---
    int maxScrollTracks = GetMaxScrollTracks(trackH, totalTracks, tHeight);
    if (maxScrollTracks > 0) {
        float vScrollbarX = tPos.x + tWidth - 14.0f;
        float vScrollbarY = tPos.y + topMargin + rulerH;
        float vScrollbarH = GetVisibleHeight(tHeight);
        
        dl->AddRectFilled(ImVec2(vScrollbarX, vScrollbarY), ImVec2(vScrollbarX + 14.0f, vScrollbarY + vScrollbarH), IM_COL32(20, 20, 25, 255));
        
        float totalH = totalTracks * trackH;
        float vRatio = vScrollbarH / totalH;
        float vThumbH = std::max(20.0f, vScrollbarH * vRatio);
        float maxThumbY = vScrollbarH - vThumbH;
        
        float thumbY = vScrollbarY + (exactScrollTrack / maxScrollTracks) * maxThumbY;
        
        ImGui::SetCursorScreenPos(ImVec2(vScrollbarX, thumbY));
        ImGui::InvisibleButton("##vscrollbar_thumb", ImVec2(14.0f, vThumbH));
        
        bool vActive = ImGui::IsItemActive();
        bool vHovered = ImGui::IsItemHovered();
        
        if (vActive && maxThumbY > 0.0f) {
            float dragDelta = ImGui::GetIO().MouseDelta.y;
            float scrollDelta = (dragDelta / maxThumbY) * maxScrollTracks;
            exactScrollTrack += scrollDelta;
            exactScrollTrack = std::clamp(exactScrollTrack, 0.0f, (float)maxScrollTracks);
            scrollTrackIndex = (int)std::round(exactScrollTrack);
        }
        
        ImU32 vCol = vActive ? IM_COL32(110, 110, 120, 255) : (vHovered ? IM_COL32(90, 90, 100, 255) : IM_COL32(70, 70, 80, 255));
        dl->AddRectFilled(ImVec2(vScrollbarX + 2, thumbY), ImVec2(vScrollbarX + 12, thumbY + vThumbH), vCol, 4.0f);
    }
}