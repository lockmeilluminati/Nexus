#include "nexus_audio_emitter.h"
#include "imgui.h"

extern std::string OpenAudioFileDialog();

std::string CharacterAudioComponent::GetKeyName() const {
    switch (interactKey) {
        case KEY_E: return "E";
        case KEY_F: return "F";
        case KEY_R: return "R";
        case KEY_Q: return "Q";
        case KEY_SPACE: return "SPACE";
        case KEY_ENTER: return "ENTER";
        default: return "E";
    }
}

void CharacterAudioComponent::AddLine(const std::string& path) {
    AudioDialogLine line;
    line.filePath = path;
    line.sound = LoadSound(path.c_str());
    line.volume = 1.0f; // Initialize volume
    if (line.sound.frameCount > 0) {
        line.isLoaded = true;
        playlist.push_back(line);
    }
}

void CharacterAudioComponent::RemoveLine(int index) {
    if (index >= 0 && index < (int)playlist.size()) {
        if (playlist[index].isLoaded) {
            UnloadSound(playlist[index].sound);
        }
        playlist.erase(playlist.begin() + index);
        if (currentlyPlayingIndex == index) currentlyPlayingIndex = -1;
    }
}

void CharacterAudioComponent::Update(Vector3 playerPos, Vector3 myPos, float dt) {
    if (playlist.empty()) {
        playerIsInRange = false;
        return;
    }

    if (currentCooldown > 0.0f) currentCooldown -= dt;

    if (currentlyPlayingIndex != -1) {
        if (!IsSoundPlaying(playlist[currentlyPlayingIndex].sound)) {
            currentlyPlayingIndex = -1;
        }
    }

    float dist = Vector3Distance(playerPos, myPos);
    playerIsInRange = (dist <= triggerRadius);

    if (playerIsInRange && currentCooldown <= 0.0f && currentlyPlayingIndex == -1) {
        bool shouldPlay = false;
        
        if (triggerType == AudioTriggerType::PROXIMITY) {
            shouldPlay = true;
        } 
        else if (triggerType == AudioTriggerType::INTERACT) {
            if (IsKeyPressed(interactKey)) shouldPlay = true;
        }

        if (shouldPlay) {
            int playIndex = 0;
            if (playbackMode == AudioPlaybackMode::SEQUENTIAL) {
                playIndex = currentIndex;
                currentIndex = (currentIndex + 1) % playlist.size();
            } else {
                playIndex = GetRandomValue(0, (int)playlist.size() - 1);
            }

            // THE FIX: Set the volume right before playing the clip!
            SetSoundVolume(playlist[playIndex].sound, playlist[playIndex].volume);
            PlaySound(playlist[playIndex].sound);
            
            currentlyPlayingIndex = playIndex;
            currentCooldown = cooldownTime;
        }
    }
}

void CharacterAudioComponent::DrawDebugZone(Vector3 myPos) {
    if (!playlist.empty()) {
        DrawCircle3D(myPos, triggerRadius, {1.0f, 0.0f, 0.0f}, 90.0f, Fade(GREEN, 0.2f));
        DrawCircle3D(myPos, triggerRadius, {1.0f, 0.0f, 0.0f}, 90.0f, Fade(DARKGREEN, 0.8f)); 
    }
}

void CharacterAudioComponent::DrawGameplayPrompt(int screenWidth, int screenHeight) {
    if (playerIsInRange && triggerType == AudioTriggerType::INTERACT && currentlyPlayingIndex == -1 && currentCooldown <= 0.0f) {
        std::string prompt = "Press " + GetKeyName() + " to Interact";
        int textWidth = MeasureText(prompt.c_str(), 30);
        int posX = (screenWidth / 2) - (textWidth / 2);
        int posY = screenHeight - 150;

        DrawText(prompt.c_str(), posX + 2, posY + 2, 30, BLACK); 
        DrawText(prompt.c_str(), posX, posY, 30, RAYWHITE);
    }
}

void CharacterAudioComponent::DrawUI() {
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Gameplay Audio Emitter");
    
    int mode = (int)playbackMode;
    ImGui::TextDisabled("Playback Order:");
    ImGui::RadioButton("Sequential", &mode, 0); ImGui::SameLine();
    ImGui::RadioButton("Random", &mode, 1);
    playbackMode = (AudioPlaybackMode)mode;

    int trigger = (int)triggerType;
    ImGui::TextDisabled("Trigger Condition:");
    ImGui::RadioButton("Proximity", &trigger, 0); ImGui::SameLine();
    ImGui::RadioButton("Interact", &trigger, 1);
    triggerType = (AudioTriggerType)trigger;

    if (triggerType == AudioTriggerType::INTERACT) {
        const char* keys[] = { "E", "F", "R", "Q", "SPACE", "ENTER" };
        const int keyCodes[] = { KEY_E, KEY_F, KEY_R, KEY_Q, KEY_SPACE, KEY_ENTER };
        
        int currentKeyIndex = 0;
        for(int i=0; i<6; i++) {
            if (interactKey == keyCodes[i]) currentKeyIndex = i;
        }
        
        if (ImGui::Combo("Interact Button", &currentKeyIndex, keys, 6)) {
            interactKey = keyCodes[currentKeyIndex];
        }
    }

    ImGui::DragFloat("Trigger Radius", &triggerRadius, 0.1f, 1.0f, 100.0f, "%.1f m");
    ImGui::DragFloat("Cooldown Delay", &cooldownTime, 0.1f, 0.0f, 60.0f, "%.1f s");

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.65f, 0.75f, 1.0f));
    if (ImGui::Button("+ ADD VOICE LINE", ImVec2(-1, 26))) {
        std::string path = OpenAudioFileDialog();
        if (!path.empty()) AddLine(path);
    }
    ImGui::PopStyleColor();

    ImGui::Spacing();
    int toDelete = -1;
    for (size_t i = 0; i < playlist.size(); i++) {
        ImGui::PushID((int)i);
        std::string name = playlist[i].filePath.substr(playlist[i].filePath.find_last_of("/\\") + 1);
        ImGui::Text("%s", name.c_str());
        
        // THE FIX: Volume Slider added directly to the Inspector Panel!
        if (ImGui::SliderFloat("Volume", &playlist[i].volume, 0.0f, 1.0f, "Vol: %.2f")) {
            SetSoundVolume(playlist[i].sound, playlist[i].volume); // Test volume immediately
        }
        
        ImGui::SameLine(ImGui::GetWindowWidth() - 40);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.2f, 0.2f, 1));
        if (ImGui::Button("X")) toDelete = (int)i;
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::PopID();
    }
    if (toDelete != -1) RemoveLine(toDelete);
}

void CharacterAudioComponent::UnloadAll() {
    for (auto& line : playlist) {
        if (line.isLoaded) UnloadSound(line.sound);
    }
    playlist.clear();
}