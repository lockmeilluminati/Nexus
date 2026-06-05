#include "nexus_anim_selector.h"
#include "imgui.h"
#include <vector>
#include <string>

void DrawAnimationSelector(SceneObject& selObj, TimelineTrack& selTrack) {
    if (selObj.isAnimated && selObj.animCount > 0) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Animation Target");

        static std::vector<std::string> tempNames;
        tempNames.clear();
        std::vector<const char*> comboItems;
        
        for (int i = 0; i < selObj.animCount; i++) {
            if (selObj.anims[i].name[0] == '\0') {
                tempNames.push_back("Clip " + std::to_string(i));
            } else {
                tempNames.push_back(selObj.anims[i].name);
            }
        }
        
        for(auto& str : tempNames) comboItems.push_back(str.c_str());

        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##animSelector", &selTrack.currentAnimIndex, comboItems.data(), comboItems.size())) {
            
            selTrack.sourceDuration = selObj.anims[selTrack.currentAnimIndex].keyframeCount; 
            
            // THE FIX: Physically stretch the timeline track so it doesn't freeze the animation!
            selTrack.endFrame = selTrack.startFrame + selTrack.sourceDuration;
        }
    }
}