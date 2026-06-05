#include "nexus_material_library.h"
#include "imgui.h"
#include "rlImGui.h"
#include <filesystem>
#include <algorithm>
#include <cstdint>

namespace fs = std::filesystem;

// ---------------------------------------------------------------
// Scanning
// ---------------------------------------------------------------

void NexusMaterialLibrary::ScanAndLoad() {
    materials.clear();
    selectedIndex = -1;

    // Auto-create the folder if it doesn't exist so users know where to drop textures
    if (!fs::exists("Materials/Characters")) {
        fs::create_directories("Materials/Characters");
        TraceLog(LOG_INFO, "MATERIALS: Created Materials/Characters/ folder. Drop .png/.jpg skin textures here.");
    }

    for (const auto& entry : fs::recursive_directory_iterator("Materials/Characters")) {
        if (!entry.is_regular_file()) continue;

        std::string path = entry.path().string();
        std::replace(path.begin(), path.end(), '\\', '/');

        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg") {
            std::string filename = entry.path().stem().string();
            // Make filename readable: underscores → spaces
            std::replace(filename.begin(), filename.end(), '_', ' ');
            LoadMaterialEntry(path, filename);
        }
    }

    TraceLog(LOG_INFO, "MATERIALS: Loaded %d character skin presets.", (int)materials.size());
}

void NexusMaterialLibrary::LoadMaterialEntry(const std::string& texPath, const std::string& name) {
    CharacterMaterial mat;
    mat.name       = name;
    mat.albedoPath = texPath;
    mat.albedo     = LoadTexture(texPath.c_str());
    mat.loaded     = (mat.albedo.id > 0);

    if (mat.loaded) {
        SetTextureFilter(mat.albedo, TEXTURE_FILTER_BILINEAR);
        materials.push_back(mat);
    } else {
        TraceLog(LOG_WARNING, "MATERIALS: Failed to load texture: %s", texPath.c_str());
    }
}

// ---------------------------------------------------------------
// Apply to SceneObject
// ---------------------------------------------------------------

void NexusMaterialLibrary::ApplyToObject(SceneObject& obj) {
    if (selectedIndex < 0 || selectedIndex >= (int)materials.size()) return;
    if (obj.model.meshCount == 0) return;

    CharacterMaterial& mat = materials[selectedIndex];
    if (!mat.loaded) return;

    for (int i = 0; i < obj.model.materialCount; i++) {
        obj.model.materials[i].maps[MATERIAL_MAP_ALBEDO].texture = mat.albedo;
        obj.model.materials[i].maps[MATERIAL_MAP_ALBEDO].color   = WHITE;
    }

    TraceLog(LOG_INFO, "MATERIALS: Applied '%s' to '%s'", mat.name.c_str(), obj.name.c_str());
}

// ---------------------------------------------------------------
// ImGui Panel
// ---------------------------------------------------------------

void NexusMaterialLibrary::DrawPanel(SceneObject& obj, bool& isOpen) {
    if (!isOpen) return;

    ImGui::SetNextWindowSize(ImVec2(320, 480), ImGuiCond_FirstUseEver);
    ImGui::Begin("Character Material Library", &isOpen);

    ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Skin & Material Presets");
    ImGui::TextDisabled("Drop .png/.jpg files into:");
    ImGui::TextDisabled("  Materials/Characters/");
    ImGui::Spacing();

    // Rescan button
    if (ImGui::Button("Rescan Folder", ImVec2(-1, 26))) {
        Unload();
        ScanAndLoad();
    }

    ImGui::Separator();
    ImGui::Spacing();

    if (materials.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "No textures found.");
        ImGui::TextWrapped("Add .png or .jpg skin textures to the Materials/Characters/ folder, then click Rescan.");
        ImGui::Spacing();

        // Helpful links panel
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "Free Texture Sources:");
        ImGui::BulletText("ambientcg.com  (CC0 - skin atlases)");
        ImGui::BulletText("3dtextures.me  (free human skin)");
        ImGui::BulletText("textures.com   (free tier available)");
        ImGui::BulletText("Blender: bake a flat skin tone to PNG");
        ImGui::BulletText("Any 1024x1024 solid skin-tone PNG works!");

        ImGui::End();
        return;
    }

    // Grid of texture thumbnails — 2 per row
    float thumbSize = 96.0f;
    float panelWidth = ImGui::GetContentRegionAvail().x;
    int   cols = (int)(panelWidth / (thumbSize + 12.0f));
    if (cols < 1) cols = 1;

    int col = 0;
    for (int i = 0; i < (int)materials.size(); i++) {
        CharacterMaterial& mat = materials[i];

        ImGui::PushID(i);

        bool isSelected = (selectedIndex == i);

        // Highlight selected
        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.2f, 0.6f, 1.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 1.0f, 1.0f));
        }

        if (ImGui::ImageButton(mat.name.c_str(),
                               (ImTextureID)(intptr_t)mat.albedo.id,
                               ImVec2(thumbSize, thumbSize))) {
            selectedIndex = i;
            // Auto-apply immediately on click if a valid object is passed
            if (obj.model.meshCount > 0) ApplyToObject(obj);
        }

        if (isSelected) ImGui::PopStyleColor(2);

        // Label under thumbnail
        ImGui::TextWrapped("%s", mat.name.c_str());

        col++;
        if (col < cols) ImGui::SameLine();
        else col = 0;

        ImGui::PopID();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Apply button (in case user wants to re-apply after changing selection)
    if (selectedIndex >= 0) {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f),
                           "Selected: %s", materials[selectedIndex].name.c_str());

        if (ImGui::Button("Apply to Selected Object", ImVec2(-1, 32))) {
            ApplyToObject(obj);
        }
    } else {
        ImGui::TextDisabled("Click a skin above to apply it.");
    }

    ImGui::End();
}

// ---------------------------------------------------------------
// Cleanup
// ---------------------------------------------------------------

void NexusMaterialLibrary::Unload() {
    for (auto& mat : materials) {
        if (mat.loaded) UnloadTexture(mat.albedo);
    }
    materials.clear();
    selectedIndex = -1;
}
