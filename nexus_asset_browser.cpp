#include "nexus_asset_browser.h"
#include "nexus_environment.h"
#include "nexus_character.h"
#include "imgui.h"
#include "rlImGui.h" 

void NexusAssetBrowser::Draw(bool& showAssetBrowser, const std::vector<Asset>& envLibrary, const std::vector<Asset>& charLibrary, SceneObject& previewObject, bool& isPreviewing) {
    if (!showAssetBrowser) return;

    // FIX: Use ImGui's dynamic WorkArea to perfectly fit the screen
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y), ImGuiCond_Always); 
    ImGui::SetNextWindowSize(ImVec2(300.0f, viewport->WorkSize.y), ImGuiCond_Always); 
    
    ImGui::Begin("Asset Browser", &showAssetBrowser, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    auto DrawAssetList = [&](const std::vector<Asset>& library, bool isEnv) {
        for (size_t i = 0; i < library.size(); i++) {
            ImGui::PushID(library[i].filePath.c_str());
            ImGui::BeginGroup();
            
            if (rlImGuiImageButton("##btn", &library[i].thumbnail)) {
                if (isPreviewing) UnloadModel(previewObject.model); 
                if (isEnv) previewObject = SpawnEnvironment(library[i].filePath);
                else previewObject = SpawnCharacter(library[i].filePath);
                isPreviewing = true;
            }

            ImGui::SameLine();
            ImGui::BeginGroup(); 
            ImVec2 pos = ImGui::GetCursorPos();
            ImGui::SetCursorPosY(pos.y + 20.0f); 
            
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 200.0f);
            ImGui::TextWrapped("%s", library[i].name.c_str());
            ImGui::PopTextWrapPos();
            
            ImGui::EndGroup(); 
            ImGui::EndGroup(); 
            ImGui::PopID();
            ImGui::Spacing();
        }
    };

    if (ImGui::CollapsingHeader("Environments", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawAssetList(envLibrary, true);
    }
    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Characters", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawAssetList(charLibrary, false);
    }
    
    ImGui::End();
}