#include "nexus_creator_menu.h"
#include "imgui.h"

void NexusCreatorMenu::DrawMainMenu(bool& isOpen) {
    if (!isOpen) return;

    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
    ImGui::Begin("Nexus Creator Hub", &isOpen);

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.8f, 1.0f), "SELECT CONTROLLER TYPE");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
    if (ImGui::Button("THIRD PERSON EXPERIENCE", ImVec2(-1, 60))) {
        currentMode = CreatorMode::MODE_THIRD_PERSON;
    }
    ImGui::PopStyleColor(2);
    
    ImGui::Spacing(); ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
    if (ImGui::Button("FIRST PERSON MECHANICS", ImVec2(-1, 60))) {
        currentMode = CreatorMode::MODE_FIRST_PERSON;
    }
    ImGui::PopStyleColor(2);

    ImGui::End();
}

void NexusCreatorMenu::TriggerGlobalReset() {
    currentMode = CreatorMode::MODE_HUB_SELECT;
    globalClearRequested = true;
}