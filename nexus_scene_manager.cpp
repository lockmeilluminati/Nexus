#include "nexus_scene_manager.h"
#include "imgui.h"
#include <string>

static void DeleteSceneObject(std::vector<SceneObject>& scene,
                               std::vector<Vector3>& rotations,
                               NexusTimeline& timeline,
                               int& selectedIndex,
                               int index) {
    // Unload GPU resources
    SceneObject& obj = scene[index];
    UnloadModel(obj.model);
    if (obj.isAnimated && obj.anims != nullptr)
        UnloadModelAnimations(obj.anims, obj.animCount);
    for (size_t i = 0; i < obj.parasiteAnims.size(); i++)
        UnloadModelAnimations(obj.parasiteAnims[i], obj.parasiteAnimCounts[i]);

    scene.erase(scene.begin() + index);
    rotations.erase(rotations.begin() + index);
    if (index < (int)timeline.tracks.size())
        timeline.tracks.erase(timeline.tracks.begin() + index);

    // Fix selection index
    if (selectedIndex >= (int)scene.size())
        selectedIndex = (int)scene.size() - 1;
    if (scene.empty()) selectedIndex = -1;

    TraceLog(LOG_INFO, "SCENE MANAGER: Deleted object at index %d", index);
}

void NexusSceneManager::Draw(bool& isOpen,
                              std::vector<SceneObject>& scene,
                              std::vector<Vector3>& rotations,
                              NexusTimeline& timeline,
                              int& selectedObjectIndex) {
    if (!isOpen) return;

    ImGui::SetNextWindowSize(ImVec2(340, 420), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(260, 200), ImVec2(600, 900));
    ImGui::Begin("Scene Manager", &isOpen);

    ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Scene Objects");
    ImGui::TextDisabled("%d object(s) in scene", (int)scene.size());
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (scene.empty()) {
        ImGui::TextDisabled("No objects in scene yet.");
        ImGui::TextDisabled("Place assets from the Asset Browser.");
        ImGui::End();
        return;
    }

    int deleteIndex = -1;

    for (int i = 0; i < (int)scene.size(); i++) {
        ImGui::PushID(i);

        bool isSelected = (selectedObjectIndex == i);

        // Row highlight for selected
        if (isSelected) {
            ImVec2 rowMin = ImGui::GetCursorScreenPos();
            ImVec2 rowMax = ImVec2(rowMin.x + ImGui::GetContentRegionAvail().x, rowMin.y + 22);
            ImGui::GetWindowDrawList()->AddRectFilled(rowMin, rowMax, IM_COL32(40, 100, 180, 80));
        }

        // Object type icon colour
        ImVec4 typeColor = scene[i].isAnimated
            ? ImVec4(1.0f, 0.6f, 0.2f, 1.0f)   // orange = animated character
            : ImVec4(0.4f, 0.9f, 0.4f, 1.0f);   // green = environment/static

        ImGui::TextColored(typeColor, scene[i].isAnimated ? "[CHAR]" : "[ENV] ");
        ImGui::SameLine();

        // Clickable name — selects the object
        std::string label = scene[i].name.empty() ? ("Object " + std::to_string(i)) : scene[i].name;
        if (!timeline.tracks.empty() && i < (int)timeline.tracks.size())
            label = timeline.tracks[i].assetName;

        if (ImGui::Selectable(label.c_str(), isSelected, 0, ImVec2(ImGui::GetContentRegionAvail().x - 60, 0)))
            selectedObjectIndex = i;

        ImGui::SameLine();

        // Position display
        ImGui::TextDisabled("(%.1f, %.1f, %.1f)",
            scene[i].position.x, scene[i].position.y, scene[i].position.z);

        // Delete button
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 28 + ImGui::GetCursorPosX() - ImGui::GetContentRegionAvail().x);
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
        if (ImGui::SmallButton(" X ")) deleteIndex = i;
        ImGui::PopStyleColor(2);

        // Right-click context menu on the row
        if (ImGui::BeginPopupContextItem("##sceneobj_ctx")) {
            ImGui::TextDisabled("%s", label.c_str());
            ImGui::Separator();
            if (ImGui::MenuItem("Select")) selectedObjectIndex = i;
            if (ImGui::MenuItem("Focus Camera")) {
                // Just selects for now — camera focus can be wired later
                selectedObjectIndex = i;
            }
            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            if (ImGui::MenuItem("Delete")) deleteIndex = i;
            ImGui::PopStyleColor();
            ImGui::EndPopup();
        }

        ImGui::Separator();
        ImGui::PopID();
    }

    // Process deletion outside the loop
    if (deleteIndex >= 0)
        DeleteSceneObject(scene, rotations, timeline, selectedObjectIndex, deleteIndex);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Clear all button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.1f, 0.1f, 1.0f));
    if (ImGui::Button("Clear Entire Scene", ImVec2(-1, 28))) {
        ImGui::OpenPopup("ConfirmClear");
    }
    ImGui::PopStyleColor();

    if (ImGui::BeginPopupModal("ConfirmClear", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Delete ALL objects from the scene?");
        ImGui::Text("This cannot be undone.");
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
        if (ImGui::Button("Yes, Clear All", ImVec2(140, 0))) {
            // Unload all objects
            for (auto& obj : scene) {
                UnloadModel(obj.model);
                if (obj.isAnimated && obj.anims != nullptr)
                    UnloadModelAnimations(obj.anims, obj.animCount);
                for (size_t i = 0; i < obj.parasiteAnims.size(); i++)
                    UnloadModelAnimations(obj.parasiteAnims[i], obj.parasiteAnimCounts[i]);
            }
            scene.clear();
            rotations.clear();
            timeline.tracks.clear();
            selectedObjectIndex = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::End();
}
