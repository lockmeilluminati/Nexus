#include "nexus_character_creator.h"
#include "nexus_character.h"
#include "file_dialog.h" 
#include "imgui.h"
#include <algorithm>
#include <string>
#include <filesystem>

const Color NexusCharacterCreator::skinPalette[PALETTE_SIZE] = {
    {255,224,189,255},{241,194,125,255},{224,172, 96,255},{198,134, 66,255},
    {161, 99, 46,255},{141, 85, 36,255},{100, 60, 20,255},{ 74, 48, 28,255},
    {176,196,160,255},{143,188,143,255},{ 80,120,200,255},{180, 80, 80,255},
};
const char* NexusCharacterCreator::skinPaletteLabels[PALETTE_SIZE] = {
    "Porcelain","Light","Light-Med","Medium","Med-Dark","Dark","Deep","Ebony",
    "Zombie","Orc","Fantasy","Demon",
};

static ImVec4 ToImVec4(Color c){return ImVec4(c.r/255.f,c.g/255.f,c.b/255.f,c.a/255.f);}

void NexusCharacterCreator::InitFromObject(const SceneObject& obj) {
    slots.clear();
    trackedFilePath = obj.filePath;
    skinTargetSlot  = 0;
    if (obj.model.meshCount == 0) return;
    int matCount = obj.model.materialCount;
    if (matCount > 1) matCount--;
    for (int i = 0; i < matCount; i++) {
        MaterialSlotOverride s;
        s.label   = (i==0?"Skin / Body":i==1?"Clothing":i==2?"Hair / Head":i==3?"Accessories":"Material "+std::to_string(i));
        s.color   = WHITE;
        s.enabled = true;
        slots.push_back(s);
    }
}

void NexusCharacterCreator::ApplyToObject(SceneObject& obj) {
    if (obj.model.meshCount == 0) return;
    for (int i = 0; i < (int)slots.size() && i < obj.model.materialCount; i++)
        obj.model.materials[i].maps[MATERIAL_MAP_ALBEDO].color = slots[i].enabled ? slots[i].color : WHITE;
}

void NexusCharacterCreator::SavePreset() {
    if (!strlen(presetNameBuf)) return;
    presets.push_back({presetNameBuf, slots});
    presetNameBuf[0] = '\0';
}

void NexusCharacterCreator::LoadPreset(int i, SceneObject& obj) {
    if (i < 0 || i >= (int)presets.size()) return;
    slots = presets[i].slots; ApplyToObject(obj);
}

void NexusCharacterCreator::DrawColorTab(SceneObject& obj) {
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1.f,.85f,.3f,1.f),"Quick Skin Tones");

    ImGui::TextDisabled("Target slot:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(130);
    const char* targetLabel = slots.empty() ? "---" : slots[skinTargetSlot].label.c_str();
    if (ImGui::BeginCombo("##st", targetLabel)) {
        for (int i = 0; i < (int)slots.size(); i++) {
            if (ImGui::Selectable(slots[i].label.c_str(), skinTargetSlot==i))
                skinTargetSlot = i;
        }
        ImGui::EndCombo();
    }
    ImGui::Spacing();

    for (int i = 0; i < PALETTE_SIZE; i++) {
        ImGui::PushID(1000+i);
        if (ImGui::ColorButton(skinPaletteLabels[i], ToImVec4(skinPalette[i]),
                               ImGuiColorEditFlags_NoTooltip, ImVec2(30,30))) {
            if (!slots.empty() && skinTargetSlot < (int)slots.size()) {
                slots[skinTargetSlot].color   = skinPalette[i];
                slots[skinTargetSlot].enabled = true;
                ApplyToObject(obj);
            }
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s",skinPaletteLabels[i]);
        ImGui::PopID();
        if ((i+1)%6 != 0) ImGui::SameLine();
    }

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
    ImGui::TextColored(ImVec4(1.f,.85f,.3f,1.f),"Material Slots");
    ImGui::Spacing();

    bool changed = false;
    for (int i = 0; i < (int)slots.size(); i++) {
        auto& s = slots[i];
        ImGui::PushID(i);
        ImGui::Checkbox("##en",&s.enabled);
        if (ImGui::IsItemEdited()) changed = true;
        ImGui::SameLine();
        char lb[64]; strncpy(lb,s.label.c_str(),63); lb[63]='\0';
        ImGui::SetNextItemWidth(110);
        if (ImGui::InputText("##lbl",lb,64)) s.label=lb;
        ImGui::SameLine();
        float col[3]={s.color.r/255.f,s.color.g/255.f,s.color.b/255.f};
        ImGui::SetNextItemWidth(190);
        if (ImGui::ColorEdit3("##c",col,ImGuiColorEditFlags_NoLabel|ImGuiColorEditFlags_PickerHueWheel)) {
            s.color={( unsigned char)(col[0]*255),(unsigned char)(col[1]*255),(unsigned char)(col[2]*255),255};
            changed=true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("X##r")) { s.color=WHITE; s.enabled=true; changed=true; }
        ImGui::PopID();
        ImGui::Spacing();
    }
    if (changed) ApplyToObject(obj);

    ImGui::Separator(); ImGui::Spacing();
    if (ImGui::Button("Apply All", ImVec2(ImGui::GetContentRegionAvail().x*.5f-4,28))) ApplyToObject(obj);
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(.6f,.1f,.1f,1.f));
    if (ImGui::Button("Reset All to White",ImVec2(-1,28))) {
        for (auto& s:slots){s.color=WHITE;s.enabled=true;} ApplyToObject(obj);
    }
    ImGui::PopStyleColor();

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
    ImGui::TextColored(ImVec4(.5f,.8f,1.f,1.f),"Saved Looks");
    ImGui::SetNextItemWidth(180);
    ImGui::InputText("##pn",presetNameBuf,64);
    ImGui::SameLine();
    if (ImGui::Button("Save Look",ImVec2(-1,0))) SavePreset();
    ImGui::Spacing();
    if (presets.empty()) { ImGui::TextDisabled("No saved looks yet."); }
    else {
        int del=-1;
        for (int i=0;i<(int)presets.size();i++) {
            ImGui::PushID(2000+i);
            for (int j=0;j<(int)presets[i].slots.size()&&j<6;j++) {
                ImGui::ColorButton("##sw",ToImVec4(presets[i].slots[j].color),ImGuiColorEditFlags_NoTooltip,ImVec2(14,14));
                ImGui::SameLine();
            }
            if (ImGui::Button(presets[i].name.c_str(),ImVec2(140,0))) LoadPreset(i,obj);
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(.5f,.1f,.1f,1.f));
            if (ImGui::SmallButton("X")) del=i;
            ImGui::PopStyleColor();
            ImGui::PopID();
        }
        if (del>=0) presets.erase(presets.begin()+del);
    }
}

void NexusCharacterCreator::Draw(SceneObject& obj, bool& isOpen) {
    if (!isOpen) return;

    bool objectChanged = (obj.filePath != trackedFilePath);
    bool slotMismatch  = ((int)slots.size() != std::max(1, obj.model.materialCount-1));
    if (objectChanged || slotMismatch) InitFromObject(obj);

    ImGui::SetNextWindowSize(ImVec2(450, 720), ImGuiCond_FirstUseEver);
    ImGui::Begin("Character Creator Pipeline", &isOpen);

    // =========================================================================
    // STEP 1: SELECTION SCREEN
    // =========================================================================
    if (obj.model.meshCount == 0) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.6f, 1.0f), "STEP 1: SELECT CHARACTER");
        ImGui::TextWrapped("Select a character from your library, or import a new external .glb file.");
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.45f, 0.75f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.55f, 0.85f, 1.0f));
        if (ImGui::Button("IMPORT EXTERNAL GLB", ImVec2(-1, 36))) {
            std::string path = OpenGLTFFileDialog();
            if (!path.empty()) {
                obj = SpawnCharacter(path);
                InitFromObject(obj); 
            }
        }
        ImGui::PopStyleColor(2);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextDisabled("Local Library (Chars/):");
        ImGui::Spacing();

        ImGui::BeginChild("CharSelectView", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), true);
        
        if (std::filesystem::exists("Chars")) {
            bool foundAsset = false;
            
            for (const auto& entry : std::filesystem::recursive_directory_iterator("Chars")) {
                if (!entry.is_regular_file()) continue;
                
                std::string path = entry.path().string();
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                if (ext == ".glb" || ext == ".gltf" || ext == ".nxchar") {
                    foundAsset = true;
                    std::string name = entry.path().stem().string();
                    std::replace(name.begin(), name.end(), '_', ' ');

                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.25f, 0.3f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.4f, 0.5f, 1.0f));
                    
                    if (ImGui::Button(name.c_str(), ImVec2(-1, 40))) {
                        obj = SpawnCharacter(path);
                        InitFromObject(obj); 
                    }
                    
                    ImGui::PopStyleColor(2);
                    ImGui::Spacing();
                }
            }
            if (!foundAsset) {
                ImGui::TextDisabled("No valid models found in 'Chars' folder.");
            }
        } else {
            ImGui::TextDisabled("The 'Chars' directory does not exist yet.");
        }
        
        ImGui::EndChild();
        ImGui::End(); 
        return;
    }

    // =========================================================================
    // STEP 2: CONFIGURATION SCREEN
    // =========================================================================
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
    if (ImGui::Button("X REMOVE CHARACTER (BACK TO START)", ImVec2(-1, 30))) {
        requestClear = true; 
    }
    ImGui::PopStyleColor();
    ImGui::Spacing();

    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "STEP 2: CONFIGURE MATERIALS");
    ImGui::TextDisabled("Target: %s", obj.name.c_str());
    ImGui::Spacing();

    ImGui::BeginChild("ConfigTabs", ImVec2(0, ImGui::GetContentRegionAvail().y - 85), false);
    DrawColorTab(obj);
    ImGui::EndChild();

    // =========================================================================
    // STEP 3: DEPLOYMENT
    // =========================================================================
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f), "STEP 3: DEPLOY TO GAME");
    
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.55f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.75f, 0.3f, 1.0f));
    
    if (ImGui::Button("SEND TO GAME PLAYER", ImVec2(-1, 40))) {
        sendToPlayer = true;
        isOpen = false; 
    }
    
    ImGui::PopStyleColor(2);

    ImGui::End();
}