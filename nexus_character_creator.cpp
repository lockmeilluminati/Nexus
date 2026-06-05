#include "nexus_character_creator.h"
#include "imgui.h"
#include <cstdint>
#include <algorithm>
#include <string>

// ---------------------------------------------------------------
// Built-in skin tone + colour palette
// ---------------------------------------------------------------

const Color NexusCharacterCreator::skinPalette[PALETTE_SIZE] = {
    { 255, 224, 189, 255 },   // Porcelain
    { 241, 194, 125, 255 },   // Light
    { 224, 172,  96, 255 },   // Light-Medium
    { 198, 134,  66, 255 },   // Medium
    { 161,  99,  46, 255 },   // Medium-Dark
    { 141,  85,  36, 255 },   // Dark
    { 100,  60,  20, 255 },   // Deep
    {  74,  48,  28, 255 },   // Ebony
    { 176, 196, 160, 255 },   // Zombie Pale
    { 143, 188, 143, 255 },   // Orc Green
    {  80, 120, 200, 255 },   // Fantasy Blue
    { 180,  80,  80, 255 },   // Demon Red
};

const char* NexusCharacterCreator::skinPaletteLabels[PALETTE_SIZE] = {
    "Porcelain", "Light", "Light-Med", "Medium",
    "Med-Dark",  "Dark",  "Deep",      "Ebony",
    "Zombie",    "Orc",   "Fantasy",   "Demon",
};

// ---------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------

// Convert Raylib Color <-> ImVec4 for the ImGui colour picker
static ImVec4 ToImVec4(Color c) {
    return ImVec4(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f);
}

static Color FromImVec4(const ImVec4& v) {
    return Color{
        (unsigned char)(v.x * 255),
        (unsigned char)(v.y * 255),
        (unsigned char)(v.z * 255),
        (unsigned char)(v.w * 255)
    };
}

// Guess a sensible label for a material slot by its index
static std::string GuessSlotLabel(int index, int total) {
    if (total == 1) return "Body";
    switch (index) {
        case 0: return "Skin / Body";
        case 1: return "Clothing";
        case 2: return "Hair / Head";
        case 3: return "Accessories";
        case 4: return "Eyes";
        default: return "Material " + std::to_string(index);
    }
}

// ---------------------------------------------------------------
// Init
// ---------------------------------------------------------------

void NexusCharacterCreator::InitFromObject(const SceneObject& obj) {
    slots.clear();
    if (obj.model.meshCount == 0) return;

    int matCount = obj.model.materialCount;
    // Don't include the default fallback material Raylib adds
    if (matCount > 1) matCount -= 1;

    for (int i = 0; i < matCount; i++) {
        MaterialSlotOverride s;
        s.label   = GuessSlotLabel(i, matCount);
        s.color   = WHITE;
        s.enabled = true;
        slots.push_back(s);
    }
}

// ---------------------------------------------------------------
// Apply
// ---------------------------------------------------------------

void NexusCharacterCreator::ApplyToObject(SceneObject& obj) {
    if (obj.model.meshCount == 0) return;

    for (int i = 0; i < (int)slots.size() && i < obj.model.materialCount; i++) {
        if (slots[i].enabled) {
            obj.model.materials[i].maps[MATERIAL_MAP_ALBEDO].color = slots[i].color;
        } else {
            obj.model.materials[i].maps[MATERIAL_MAP_ALBEDO].color = WHITE;
        }
    }
}

// ---------------------------------------------------------------
// Presets
// ---------------------------------------------------------------

void NexusCharacterCreator::SavePreset() {
    if (strlen(presetNameBuf) == 0) return;
    CharacterPreset p;
    p.name  = presetNameBuf;
    p.slots = slots;
    presets.push_back(p);
    presetNameBuf[0] = '\0';
}

void NexusCharacterCreator::LoadPreset(int index, SceneObject& obj) {
    if (index < 0 || index >= (int)presets.size()) return;
    slots = presets[index].slots;
    ApplyToObject(obj);
}

// ---------------------------------------------------------------
// Draw Panel
// ---------------------------------------------------------------

void NexusCharacterCreator::Draw(SceneObject& obj, bool& isOpen) {
    if (!isOpen) return;

    // Re-init slot list if model changed or not yet set up
    if ((int)slots.size() != std::max(1, obj.model.materialCount - 1)) {
        InitFromObject(obj);
    }

    ImGui::SetNextWindowSize(ImVec2(380, 620), ImGuiCond_FirstUseEver);
    ImGui::Begin("Character Creator", &isOpen);

    if (obj.model.meshCount == 0) {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "No character selected.");
        ImGui::TextWrapped("Select a character in the scene or asset browser first.");
        ImGui::End();
        return;
    }

    ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Editing: %s", obj.name.c_str());
    ImGui::TextDisabled("%d material slot(s) detected", (int)slots.size());
    ImGui::Spacing();

    // ---------------------------------------------------------------
    // SKIN TONE QUICK-PICK PALETTE
    // ---------------------------------------------------------------
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "Quick Skin Tones");
    ImGui::Spacing();

    for (int i = 0; i < PALETTE_SIZE; i++) {
        ImVec4 col = ToImVec4(skinPalette[i]);

        ImGui::PushID(1000 + i);
        if (ImGui::ColorButton(skinPaletteLabels[i], col,
                               ImGuiColorEditFlags_NoTooltip, ImVec2(28, 28))) {
            // Apply to slot 0 (skin) on click, or whichever is selected
            if (!slots.empty()) {
                slots[0].color   = skinPalette[i];
                slots[0].enabled = true;
                ApplyToObject(obj);
            }
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", skinPaletteLabels[i]);
        ImGui::PopID();

        if ((i + 1) % 6 != 0) ImGui::SameLine();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ---------------------------------------------------------------
    // PER-SLOT COLOUR PICKERS
    // ---------------------------------------------------------------
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "Material Slots");
    ImGui::Spacing();

    bool anyChanged = false;

    for (int i = 0; i < (int)slots.size(); i++) {
        auto& slot = slots[i];

        ImGui::PushID(i);

        // Slot toggle checkbox
        ImGui::Checkbox("##en", &slot.enabled);
        ImGui::SameLine();

        // Slot label (editable)
        char labelBuf[64];
        strncpy(labelBuf, slot.label.c_str(), 63);
        labelBuf[63] = '\0';
        ImGui::SetNextItemWidth(120);
        if (ImGui::InputText("##lbl", labelBuf, 64)) {
            slot.label = labelBuf;
        }
        ImGui::SameLine();

        // Full colour picker button — opens inline picker
        ImVec4 imCol = ToImVec4(slot.color);
        ImGuiColorEditFlags flags =
            ImGuiColorEditFlags_NoInputs  |
            ImGuiColorEditFlags_NoLabel   |
            ImGuiColorEditFlags_PickerHueWheel;

        if (ImGui::ColorEdit4("##col", (float*)&imCol, flags)) {
            slot.color = FromImVec4(imCol);
            anyChanged = true;
        }

        ImGui::SameLine();

        // Reset to white button
        if (ImGui::SmallButton("Reset")) {
            slot.color   = WHITE;
            slot.enabled = true;
            anyChanged   = true;
        }

        // Show the slot label properly to the right
        ImGui::SameLine();
        if (!slot.enabled) {
            ImGui::TextDisabled("%s (off)", slot.label.c_str());
        } else {
            ImGui::TextUnformatted(slot.label.c_str());
        }

        ImGui::PopID();
        ImGui::Spacing();
    }

    // Apply changes live as colours are dragged
    if (anyChanged) ApplyToObject(obj);

    ImGui::Separator();
    ImGui::Spacing();

    // Apply All button (safety re-apply)
    if (ImGui::Button("Apply All Colours", ImVec2(-1, 32))) {
        ApplyToObject(obj);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ---------------------------------------------------------------
    // RESET
    // ---------------------------------------------------------------
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
    if (ImGui::Button("Reset All to White", ImVec2(-1, 26))) {
        for (auto& s : slots) { s.color = WHITE; s.enabled = true; }
        ApplyToObject(obj);
    }
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ---------------------------------------------------------------
    // PRESET SAVE / LOAD
    // ---------------------------------------------------------------
    ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Saved Looks");
    ImGui::Spacing();

    ImGui::SetNextItemWidth(180);
    ImGui::InputText("##presetname", presetNameBuf, 64);
    ImGui::SameLine();
    if (ImGui::Button("Save Look", ImVec2(-1, 0))) SavePreset();

    ImGui::Spacing();

    if (presets.empty()) {
        ImGui::TextDisabled("No saved looks yet.");
    } else {
        for (int i = 0; i < (int)presets.size(); i++) {
            ImGui::PushID(2000 + i);

            // Show a row of colour swatches for the preset
            for (int j = 0; j < (int)presets[i].slots.size() && j < 6; j++) {
                ImVec4 c = ToImVec4(presets[i].slots[j].color);
                ImGui::ColorButton("##sw", c, ImGuiColorEditFlags_NoTooltip, ImVec2(14, 14));
                ImGui::SameLine();
            }

            if (ImGui::Button(presets[i].name.c_str(), ImVec2(160, 0))) {
                LoadPreset(i, obj);
            }
            ImGui::SameLine();

            // Delete preset
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.1f, 0.1f, 1.0f));
            if (ImGui::SmallButton("X")) {
                presets.erase(presets.begin() + i);
                ImGui::PopStyleColor();
                ImGui::PopID();
                break;
            }
            ImGui::PopStyleColor();
            ImGui::PopID();
        }
    }

    ImGui::End();
}
