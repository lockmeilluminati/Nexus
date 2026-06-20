#include "nexus_text_fx.h"
#include "raymath.h"
#include "imgui.h"
#include <cmath>
#include <cstring>
#include <algorithm>

void NexusTextManager::LoadTextFont(CinematicText& track) {
    if (track.isLoaded) return;
    
    if (!track.fontPath.empty() && FileExists(track.fontPath.c_str())) {
        track.font = LoadFontEx(track.fontPath.c_str(), (int)track.fontSize, 0, 250);
    } else {
        track.font = GetFontDefault();
    }
    track.isLoaded = true;
}

void NexusTextManager::UnloadTextFont(CinematicText& track) {
    if (track.isLoaded && !track.fontPath.empty()) {
        UnloadFont(track.font);
    }
    track.isLoaded = false;
}

void NexusTextManager::DrawCinematicText(CinematicText& track, int currentGlobalFrame) {
    if (currentGlobalFrame < track.startFrame || currentGlobalFrame > track.endFrame) return;
    if (!track.isLoaded) LoadTextFont(track);

    float dt = GetTime(); 
    int framesElapsed = currentGlobalFrame - track.startFrame;
    
    auto DrawRichText = [&](const char* txt, Vector2 pos) {
        if (track.hasShadow) {
            DrawTextEx(track.font, txt, {pos.x + 3.0f, pos.y + 3.0f}, track.fontSize, track.fontSpacing, {0, 0, 0, 180});
        }
        if (track.isBold) {
            DrawTextEx(track.font, txt, {pos.x + 1.0f, pos.y}, track.fontSize, track.fontSpacing, track.color);
            DrawTextEx(track.font, txt, {pos.x, pos.y + 1.0f}, track.fontSize, track.fontSpacing, track.color);
        }
        DrawTextEx(track.font, txt, pos, track.fontSize, track.fontSpacing, track.color);
    };

    if (track.effect == TextEffect::NONE) {
        DrawRichText(track.text.c_str(), track.position);
    } 
    else if (track.effect == TextEffect::TYPEWRITER) {
        int charsToShow = (int)(framesElapsed * (track.effectSpeed * 0.1f));
        if (charsToShow > (int)track.text.length()) charsToShow = track.text.length();
        std::string sub = track.text.substr(0, charsToShow);
        DrawRichText(sub.c_str(), track.position);
    }
    else {
        Vector2 cursor = track.position;
        for (size_t i = 0; i < track.text.length(); i++) {
            char c = track.text[i];
            std::string singleChar(1, c);
            Vector2 charOffset = { 0.0f, 0.0f };
            
            if (track.effect == TextEffect::WAVE) {
                charOffset.y = sin(dt * track.effectSpeed + (i * 0.5f)) * track.effectIntensity;
            } 
            else if (track.effect == TextEffect::SHAKE) {
                charOffset.x = (float)GetRandomValue(-(int)track.effectIntensity, (int)track.effectIntensity);
                charOffset.y = (float)GetRandomValue(-(int)track.effectIntensity, (int)track.effectIntensity);
            }

            Vector2 finalPos = { cursor.x + charOffset.x, cursor.y + charOffset.y };
            DrawRichText(singleChar.c_str(), finalPos);
            
            Vector2 size = MeasureTextEx(track.font, singleChar.c_str(), track.fontSize, track.fontSpacing);
            cursor.x += size.x + track.fontSpacing; 
        }
    }
}

// THE EDITOR PANEL 
void NexusTextManager::DrawTextEditorLayout(CinematicText& track, int& selectedTextIndex) {
    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "Cinematic Text Editor");
    ImGui::Separator();
    
    static char nameBuf[64] = "";
    strncpy(nameBuf, track.trackName.c_str(), sizeof(nameBuf) - 1);
    nameBuf[sizeof(nameBuf) - 1] = '\0';
    if (ImGui::InputText("Track Label", nameBuf, sizeof(nameBuf))) track.trackName = nameBuf;

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.8f, 1.0f), "Text Content");
    
    static char textBuf[512] = "";
    strncpy(textBuf, track.text.c_str(), sizeof(textBuf) - 1);
    textBuf[sizeof(textBuf) - 1] = '\0';
    if (ImGui::InputTextMultiline("##Text", textBuf, sizeof(textBuf), ImVec2(-1, 80))) {
        track.text = textBuf;
    }

    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextDisabled("Font & Styling");
    
    static char fontBuf[256] = "";
    strncpy(fontBuf, track.fontPath.c_str(), sizeof(fontBuf) - 1);
    fontBuf[sizeof(fontBuf) - 1] = '\0';
    if (ImGui::InputText("TTF Path", fontBuf, sizeof(fontBuf))) {
        track.fontPath = fontBuf;
        UnloadTextFont(track); 
    }
    ImGui::TextDisabled("Leave blank for Raylib Default. (e.g. fonts/Roboto.ttf)");

    ImGui::Spacing();
    ImGui::Checkbox("Simulate BOLD", &track.isBold);
    ImGui::SameLine(0, 20);
    ImGui::Checkbox("Drop Shadow", &track.hasShadow);

    ImGui::Spacing();
    float colorVec[4] = { (float)track.color.r/255.f, (float)track.color.g/255.f, (float)track.color.b/255.f, (float)track.color.a/255.f };
    if (ImGui::ColorEdit4("Color", colorVec)) {
        track.color = { (unsigned char)(colorVec[0]*255), (unsigned char)(colorVec[1]*255), (unsigned char)(colorVec[2]*255), (unsigned char)(colorVec[3]*255) };
    }

    ImGui::DragFloat("Font Size", &track.fontSize, 1.0f, 10.0f, 300.0f);
    ImGui::DragFloat("Spacing", &track.fontSpacing, 0.1f, 0.0f, 50.0f);
    ImGui::DragFloat2("Screen Pos", &track.position.x, 1.0f, 0.0f, 3000.0f);

    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Animation Effects");
    
    int e = (int)track.effect;
    if (ImGui::Combo("Effect", &e, "None\0Wave (Floating)\0Shake (Jitter)\0Typewriter\0")) {
        track.effect = (TextEffect)e;
    }

    if (track.effect != TextEffect::NONE) {
        ImGui::DragFloat("Effect Speed", &track.effectSpeed, 0.1f, 0.1f, 50.0f);
        if (track.effect == TextEffect::WAVE || track.effect == TextEffect::SHAKE) {
            ImGui::DragFloat("Intensity (Px)", &track.effectIntensity, 0.1f, 1.0f, 50.0f);
        }
    }

    // THE FIX: Close Button
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.4f, 0.4f, 1.0f));
    if (ImGui::Button("CLOSE TEXT EDITOR", ImVec2(-1, 35))) {
        selectedTextIndex = -1; // Deselects track, closing inspector panel automatically
    }
    ImGui::PopStyleColor(2);
}