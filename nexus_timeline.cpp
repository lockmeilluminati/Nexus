#include "nexus_timeline.h"
#include "imgui.h"

void NexusTimeline::AddTrack(const std::string& name, int frames, bool isEnv) {
    int length = (frames > 0) ? frames : 300; 
    tracks.push_back({name, frames, 0, length, isEnv});
}

void NexusTimeline::Draw(int& globalFrame, float leftOffset) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    float timelineHeight = 250.0f;
    
    // FIX: Perfect bottom-docking math
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + leftOffset, viewport->WorkPos.y + viewport->WorkSize.y - timelineHeight), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x - leftOffset, timelineHeight), ImGuiCond_Always);
    
    ImGui::Begin("Nexus Sequencer", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    if (ImGui::Button(isPlaying ? "PAUSE" : "PLAY", ImVec2(80, 25))) {
        isPlaying = !isPlaying;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(400);
    ImGui::SliderInt("Master Frame", &globalFrame, 0, maxTimelineFrames);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    
    float width = ImGui::GetContentRegionAvail().x;
    float trackHeight = 35.0f;
    float headerWidth = 250.0f;
    float timelineWidth = width - headerWidth;
    float currentY = p.y + 10.0f;

    draw_list->AddRectFilled(ImVec2(p.x, currentY), ImVec2(p.x + width, currentY + 20), IM_COL32(30, 30, 30, 255));
    for (int i = 0; i <= maxTimelineFrames; i += 60) {
        float tickX = p.x + headerWidth + (timelineWidth * ((float)i / maxTimelineFrames));
        draw_list->AddLine(ImVec2(tickX, currentY + 10), ImVec2(tickX, currentY + 20), IM_COL32(150, 150, 150, 255));
        draw_list->AddText(ImVec2(tickX + 2, currentY), IM_COL32(150, 150, 150, 255), (std::to_string(i / 60) + "s").c_str());
    }
    currentY += 25.0f;

    for (size_t i = 0; i < tracks.size(); i++) {
        auto& track = tracks[i];
        
        draw_list->AddRectFilled(ImVec2(p.x, currentY), ImVec2(p.x + width, currentY + trackHeight - 4), IM_COL32(40, 40, 40, 255));
        draw_list->AddText(ImVec2(p.x + 5, currentY + 8), IM_COL32(200, 200, 200, 255), track.assetName.c_str());

        float startX = p.x + headerWidth + (timelineWidth * ((float)track.startFrame / maxTimelineFrames));
        float endX = p.x + headerWidth + (timelineWidth * ((float)track.endFrame / maxTimelineFrames));
        
        if (endX < startX + 10.0f) endX = startX + 10.0f; 

        ImU32 clipColor = track.isEnvironment ? IM_COL32(80, 150, 80, 255) : IM_COL32(150, 80, 80, 255);
        ImU32 handleColor = IM_COL32(200, 200, 200, 150);

        draw_list->AddRectFilled(ImVec2(startX, currentY), ImVec2(endX, currentY + trackHeight - 4), clipColor, 4.0f);
        draw_list->AddRectFilled(ImVec2(startX, currentY), ImVec2(startX + 8, currentY + trackHeight - 4), handleColor, 4.0f);
        draw_list->AddRectFilled(ImVec2(endX - 8, currentY), ImVec2(endX, currentY + trackHeight - 4), handleColor, 4.0f);

        ImGui::PushID((int)i);
        
        ImGui::SetCursorScreenPos(ImVec2(startX, currentY));
        ImGui::InvisibleButton("left_handle", ImVec2(8, trackHeight));
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            float deltaFrame = (ImGui::GetIO().MouseDelta.x / timelineWidth) * maxTimelineFrames;
            track.startFrame += (int)deltaFrame;
            if (track.startFrame < 0) track.startFrame = 0;
            if (track.startFrame > track.endFrame - 10) track.startFrame = track.endFrame - 10;
        }

        ImGui::SetCursorScreenPos(ImVec2(endX - 8, currentY));
        ImGui::InvisibleButton("right_handle", ImVec2(8, trackHeight));
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            float deltaFrame = (ImGui::GetIO().MouseDelta.x / timelineWidth) * maxTimelineFrames;
            track.endFrame += (int)deltaFrame;
            if (track.endFrame < track.startFrame + 10) track.endFrame = track.startFrame + 10;
        }

        ImGui::SetCursorScreenPos(ImVec2(startX + 8, currentY));
        ImGui::InvisibleButton("center_body", ImVec2((endX - startX) - 16, trackHeight));
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            float deltaFrame = (ImGui::GetIO().MouseDelta.x / timelineWidth) * maxTimelineFrames;
            int length = track.endFrame - track.startFrame; 
            
            track.startFrame += (int)deltaFrame;
            track.endFrame += (int)deltaFrame;
            
            if (track.startFrame < 0) {
                track.startFrame = 0;
                track.endFrame = length; 
            }
        }

        ImGui::PopID();
        currentY += trackHeight;
    }

    float playheadX = p.x + headerWidth + (timelineWidth * ((float)globalFrame / maxTimelineFrames));
    draw_list->AddLine(ImVec2(playheadX, p.y + 30), ImVec2(playheadX, currentY), IM_COL32(255, 200, 0, 255), 2.0f);

    ImGui::End();
}