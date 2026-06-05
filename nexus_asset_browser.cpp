#include "nexus_asset_browser.h"
#include "nexus_character.h"
#include "nexus_environment.h"
#include "nexus_prefab.h"
#include "rlImGui.h"
#include "imgui.h"
#include <cstdint>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

// Deletes the asset's folder from disk and unloads its thumbnail from VRAM
static void DeleteAsset(std::vector<Asset>& lib, size_t index) {
    UnloadTexture(lib[index].thumbnail);

    // Remove the folder from disk (e.g. Chars/zombie_walk/)
    std::string path = lib[index].filePath;
    std::string folder = path.substr(0, path.find_last_of("/\\"));
    try {
        fs::remove_all(folder);
        TraceLog(LOG_INFO, "ASSET BROWSER: Deleted asset folder: %s", folder.c_str());
    } catch (...) {
        TraceLog(LOG_WARNING, "ASSET BROWSER: Could not delete folder: %s", folder.c_str());
    }

    lib.erase(lib.begin() + index);
}

void NexusAssetBrowser::Draw(bool& isOpen,
                              std::vector<Asset>& envLibrary,
                              std::vector<Asset>& charLibrary,
                              SceneObject& previewObject,
                              bool& isPreviewing) {
    if (!isOpen) return;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    float sidebarWidth = 300.0f;

    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(sidebarWidth, viewport->WorkSize.y), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGui::Begin("Asset Browser", &isOpen,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    // --- ENVIRONMENTS ---
    if (ImGui::CollapsingHeader("Environments", ImGuiTreeNodeFlags_DefaultOpen)) {
        int deleteEnvIndex = -1;

        for (size_t i = 0; i < envLibrary.size(); i++) {
            ImGui::PushID((int)i);

            if (ImGui::ImageButton(envLibrary[i].name.c_str(),
                                   (ImTextureID)(intptr_t)envLibrary[i].thumbnail.id,
                                   ImVec2(64, 64))) {
                if (isPreviewing) {
                    UnloadModel(previewObject.model);
                    if (previewObject.isAnimated && previewObject.anims != nullptr)
                        UnloadModelAnimations(previewObject.anims, previewObject.animCount);
                }
                previewObject = SpawnEnvironment(envLibrary[i].filePath);
                isPreviewing = true;
            }

            // Right-click context menu
            if (ImGui::BeginPopupContextItem("##envctx")) {
                ImGui::TextDisabled("%s", envLibrary[i].name.c_str());
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                if (ImGui::MenuItem("Delete Asset")) deleteEnvIndex = (int)i;
                ImGui::PopStyleColor();
                ImGui::EndPopup();
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Left-click to place\nRight-click to delete");

            ImGui::SameLine();
            ImGui::TextWrapped("%s", envLibrary[i].name.c_str());
            ImGui::PopID();
        }

        if (deleteEnvIndex >= 0) DeleteAsset(envLibrary, deleteEnvIndex);
    }

    // --- CHARACTERS ---
    if (ImGui::CollapsingHeader("Characters", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "CRITICAL LIMITS:");
        ImGui::TextWrapped("• Vertices:  Max 65.5k (16-bit Engine Limit)");
        ImGui::TextWrapped("• Triangles: ~13.8k (Ideal Performance Target)");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        int deleteCharIndex = -1;

        for (size_t i = 0; i < charLibrary.size(); i++) {
            ImGui::PushID((int)(i + envLibrary.size()));

            if (ImGui::ImageButton(charLibrary[i].name.c_str(),
                                   (ImTextureID)(intptr_t)charLibrary[i].thumbnail.id,
                                   ImVec2(64, 64))) {
                if (isPreviewing) {
                    UnloadModel(previewObject.model);
                    if (previewObject.isAnimated && previewObject.anims != nullptr)
                        UnloadModelAnimations(previewObject.anims, previewObject.animCount);
                }
                std::string ext = charLibrary[i].filePath.substr(charLibrary[i].filePath.find_last_of("."));
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".nxchar")
                    previewObject = LoadNexusCharacterPrefab(charLibrary[i].filePath);
                else
                    previewObject = SpawnCharacter(charLibrary[i].filePath);
                isPreviewing = true;
            }

            // Right-click context menu
            if (ImGui::BeginPopupContextItem("##charctx")) {
                ImGui::TextDisabled("%s", charLibrary[i].name.c_str());
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                if (ImGui::MenuItem("Delete Asset")) deleteCharIndex = (int)i;
                ImGui::PopStyleColor();
                ImGui::EndPopup();
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Left-click to place\nRight-click to delete");

            ImGui::SameLine();
            ImGui::TextWrapped("%s", charLibrary[i].name.c_str());
            ImGui::PopID();
        }

        if (deleteCharIndex >= 0) DeleteAsset(charLibrary, deleteCharIndex);
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
}
