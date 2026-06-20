#include "nexus_scene_manager.h"
#include "imgui.h"
#include <string>
#include <algorithm> // Required for std::max safety

static void DeleteSceneObject(std::vector<SceneObject>& scene,
                               std::vector<Vector3>& rotations,
                               NexusTimeline& timeline,
                               int& selectedIndex, int index) {
    SceneObject& obj = scene[index];
    UnloadModel(obj.model);
    if (obj.isAnimated && obj.anims)
        UnloadModelAnimations(obj.anims, obj.animCount);
    for (size_t i=0;i<obj.parasiteAnims.size();i++)
        UnloadModelAnimations(obj.parasiteAnims[i], obj.parasiteAnimCounts[i]);
    scene.erase(scene.begin()+index);
    rotations.erase(rotations.begin()+index);
    if (index<(int)timeline.tracks.size())
        timeline.tracks.erase(timeline.tracks.begin()+index);
    if (selectedIndex>=(int)scene.size()) selectedIndex=(int)scene.size()-1;
    if (scene.empty()) selectedIndex=-1;
}

void NexusSceneManager::Draw(bool& isOpen,
                              std::vector<SceneObject>& scene,
                              std::vector<Vector3>& rotations,
                              NexusTimeline& timeline,
                              int& selectedObjectIndex,
                              int& selectedTextIndex, 
                              NexusPlayer& player,
                              bool& playerInitialized) {
    if (!isOpen) return;

    ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_Appearing); 
    ImGui::SetNextWindowSize(ImVec2(350, 500), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(260,200), ImVec2(800,1000));
    
    if (!ImGui::Begin("Scene Hierarchy", &isOpen)) {
        ImGui::End();
        return;
    }

    // ---------------------------------------------------------------
    // 3D SCENE OBJECTS
    // ---------------------------------------------------------------
    ImGui::TextColored(ImVec4(0.5f,0.8f,1.f,1.f),"Scene Objects");
    ImGui::TextDisabled("%d object(s)",(int)scene.size());
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    int delIdx=-1;
    if (scene.empty()) {
        ImGui::TextDisabled("No 3D objects placed yet.");
    } else {
        for (int i=0;i<(int)scene.size();i++) {
            ImGui::PushID(i);
            bool isSel=(selectedObjectIndex==i);
            if (isSel) {
                ImVec2 rMin=ImGui::GetCursorScreenPos();
                ImVec2 rMax=ImVec2(rMin.x+ImGui::GetContentRegionAvail().x,rMin.y+22);
                ImGui::GetWindowDrawList()->AddRectFilled(rMin,rMax,IM_COL32(40,100,180,80));
            }
            ImVec4 tc=scene[i].isAnimated?ImVec4(1.f,.6f,.2f,1.f):ImVec4(.4f,.9f,.4f,1.f);
            ImGui::TextColored(tc,scene[i].isAnimated?"[CHAR]":"[ENV] ");
            ImGui::SameLine();
            std::string lbl=(!timeline.tracks.empty()&&i<(int)timeline.tracks.size())
                ?timeline.tracks[i].assetName:(scene[i].name.empty()?"Object "+std::to_string(i):scene[i].name);
            
            float availW = ImGui::GetContentRegionAvail().x;
            float btnW = std::max(10.0f, availW - 36.0f);
            
            if (ImGui::Selectable(lbl.c_str(),isSel,0,ImVec2(btnW,0))) {
                selectedObjectIndex=i;
                selectedTextIndex=-1; 
            }
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(.6f,.1f,.1f,1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(.9f,.2f,.2f,1.f));
            if (ImGui::SmallButton(" X ")) delIdx=i;
            ImGui::PopStyleColor(2);
            ImGui::Separator();
            ImGui::PopID();
        }
    }
    if (delIdx>=0) DeleteSceneObject(scene,rotations,timeline,selectedObjectIndex,delIdx);

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    // ---------------------------------------------------------------
    // TEXT TRACKS
    // ---------------------------------------------------------------
    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "Cinematic Text Overlays");
    ImGui::TextDisabled("%d overlay(s)",(int)timeline.textTracks.size());
    ImGui::Spacing();

    int textDelIdx = -1;
    if (timeline.textTracks.empty()) {
        ImGui::TextDisabled("No text tracks placed yet.");
    } else {
        for (int i=0; i < (int)timeline.textTracks.size(); i++) {
            ImGui::PushID(10000 + i);
            bool isSel = (selectedTextIndex == i);
            if (isSel) {
                ImVec2 rMin=ImGui::GetCursorScreenPos();
                ImVec2 rMax=ImVec2(rMin.x+ImGui::GetContentRegionAvail().x,rMin.y+22);
                ImGui::GetWindowDrawList()->AddRectFilled(rMin,rMax,IM_COL32(180,100,40,80));
            }
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "[TEXT]");
            ImGui::SameLine();
            
            float availW = ImGui::GetContentRegionAvail().x;
            float btnW = std::max(10.0f, availW - 36.0f);
            
            if (ImGui::Selectable(timeline.textTracks[i].trackName.c_str(), isSel, 0, ImVec2(btnW,0))) {
                selectedTextIndex = i;
                selectedObjectIndex = -1; 
            }
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(.6f,.1f,.1f,1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(.9f,.2f,.2f,1.f));
            if (ImGui::SmallButton(" X ")) textDelIdx=i;
            ImGui::PopStyleColor(2);
            ImGui::Separator();
            ImGui::PopID();
        }
    }
    
    if (textDelIdx >= 0) {
        timeline.textTracks.erase(timeline.textTracks.begin() + textDelIdx);
        if (selectedTextIndex == textDelIdx) selectedTextIndex = -1;
        else if (selectedTextIndex > textDelIdx) selectedTextIndex--;
    }

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    // ---------------------------------------------------------------
    // CINEMATIC AUDIO TRACKS (NEW)
    // ---------------------------------------------------------------
    ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.7f, 1.0f), "Cinematic Audio Tracks");
    ImGui::TextDisabled("%d track(s)", (int)timeline.audioManager.tracks.size());
    ImGui::Spacing();

    int audioDelIdx = -1;
    if (timeline.audioManager.tracks.empty()) {
        ImGui::TextDisabled("No audio tracks placed yet.");
    } else {
        for (int i=0; i < (int)timeline.audioManager.tracks.size(); i++) {
            ImGui::PushID(20000 + i);
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.8f, 1.0f), "[AUDIO]");
            ImGui::SameLine();
            ImGui::Text("%s", timeline.audioManager.tracks[i].assetName.c_str());
            
            float availW = ImGui::GetContentRegionAvail().x;
            float btnW = std::max(10.0f, availW - 36.0f);
            
            ImGui::SetNextItemWidth(btnW - 10.0f);
            if (ImGui::SliderFloat("##vol", &timeline.audioManager.tracks[i].volume, 0.0f, 1.0f, "Vol: %.2f")) {
                // Instantly update the music volume stream!
                SetMusicVolume(timeline.audioManager.tracks[i].stream, timeline.audioManager.tracks[i].volume);
            }
            
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(.6f,.1f,.1f,1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(.9f,.2f,.2f,1.f));
            if (ImGui::SmallButton(" X ")) audioDelIdx=i;
            ImGui::PopStyleColor(2);
            ImGui::Separator();
            ImGui::PopID();
        }
    }
    
    if (audioDelIdx >= 0) {
        timeline.audioManager.RemoveTrack(audioDelIdx);
    }

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    // ---------------------------------------------------------------
    // PLAYABLE CHARACTER
    // ---------------------------------------------------------------
    ImGui::TextColored(ImVec4(.4f,1.f,.5f,1.f),"Playable Character");
    ImGui::Spacing();

    if (!playerInitialized || player.obj.model.meshCount==0) {
        ImGui::TextDisabled("No player assigned.");
    } else {
        ImGui::TextColored(ImVec4(.5f,1.f,.6f,1.f),"[PLAYER]");
        ImGui::SameLine();
        ImGui::TextUnformatted(player.obj.name.empty()?"Player Character":player.obj.name.c_str());
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(.5f,.1f,.1f,1.f));
        if (ImGui::Button("Remove Player",ImVec2(-1,24))) {
            playerInitialized=false; player.isPlayMode=false;
            player.obj=SceneObject{}; EnableCursor();
        }
        ImGui::PopStyleColor();
    }

    ImGui::End();
}