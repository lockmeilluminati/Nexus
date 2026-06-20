#include "nexus_timeline.h"
#include "imgui.h"
#include <string>
#include <algorithm>
#include <cmath>

extern std::string OpenAudioFileDialog();

void NexusTimeline::AddTrack(const std::string& name, int frames, bool isEnv) {
    int length = frames > 0 ? frames : 300;
    tracks.push_back({name, 0, length, 0, frames, 0, isEnv, false});
}

void NexusTimeline::AddGameZone(int startFrame, int endFrame) {
    GameZone gz;
    gz.startFrame    = startFrame;
    gz.endFrame      = endFrame;
    gz.label         = "Game Zone " + std::to_string(gameZones.size() + 1);
    gz.timeLimitSecs = (float)(endFrame - startFrame) / 60.0f;
    gameZones.push_back(gz);
}

void NexusTimeline::AddTextTrack() {
    CinematicText txt;
    txt.trackName = "Text Track " + std::to_string(textTracks.size() + 1);
    textTracks.push_back(txt);
}

const GameZone* NexusTimeline::GetActiveGameZone(int globalFrame) const {
    for (const auto& gz : gameZones)
        if (globalFrame >= gz.startFrame && globalFrame <= gz.endFrame)
            return &gz;
    return nullptr;
}

bool NexusTimeline::SyncCameraToTimeline(int globalFrame, Camera3D& editorCam) {
    if (!isPlaying) return false;
    for (auto& camTrack : cameraTracks) {
        if (globalFrame >= camTrack.timelineStartFrame && globalFrame <= camTrack.timelineEndFrame) {
            float localTime = ((float)(globalFrame - camTrack.timelineStartFrame) / 60.0f) + camTrack.trimStart;
            Vector3 pos, tgt; float fov;
            camTrack.Evaluate(localTime, pos, tgt, fov);
            editorCam.position = pos;
            editorCam.target = tgt;
            editorCam.fovy = fov;
            return true; 
        }
    }
    return false;
}

void NexusTimeline::Draw(int& globalFrame, float leftOffset, int& deleteIndex,
                         bool& playModeRequested, float& playTimeLimitOut,
                         int& selectedObjectIndex, int& selectedTextIndex) {
    deleteIndex       = -1;
    playModeRequested = false;
    playTimeLimitOut  = 10.0f;

    ImGuiViewport* vp = ImGui::GetMainViewport();

    int trackCount = gameZones.size() + cameraTracks.size() + audioManager.tracks.size() + textTracks.size() + tracks.size();
    float trackH   = 34.0f;
    float totalTracksH = trackCount * trackH;
    float desiredTL_H = scroller.topMargin + scroller.rulerH + 4.0f + totalTracksH + scroller.scrollbarH + 5.0f;
    
    // Caps timeline to 45% of screen height to allow scrolling
    float TL_H = std::clamp(desiredTL_H, 260.0f, vp->WorkSize.y * 0.45f);

    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + leftOffset, vp->WorkPos.y + vp->WorkSize.y - TL_H), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x - leftOffset, TL_H), ImGuiCond_Always);

    ImGui::Begin("Nexus Sequencer", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p       = ImGui::GetCursorScreenPos();
    float width    = ImGui::GetContentRegionAvail().x;
    float headerW  = 240.0f;

    // Process all scrolling math remotely
    scroller.ProcessInput(p, width, TL_H, trackH, trackCount);

    auto GetX = [&](int frame) -> float {
        return scroller.GetX(frame, p, headerW, width, maxTimelineFrames);
    };
    auto GetFrameDelta = [&](float mouseDeltaX) -> float {
        return scroller.GetFrameDelta(mouseDeltaX, headerW, width, maxTimelineFrames);
    };

    if (ImGui::Button(isPlaying ? "PAUSE" : "PLAY", ImVec2(70, 25))) isPlaying = !isPlaying;
    ImGui::SameLine();
    if (ImGui::Button("STOP", ImVec2(55, 25))) { isPlaying = false; globalFrame = 0; }
    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.45f, 0.15f, 0.75f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.60f, 0.25f, 0.95f, 1.0f));
    if (ImGui::Button("+ GAME ZONE", ImVec2(105, 25))) AddGameZone(std::max(0, globalFrame - 150), globalFrame + 150);
    ImGui::PopStyleColor(2);
    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.15f, 0.65f, 0.75f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.80f, 0.95f, 1.0f));
    if (ImGui::Button("+ AUDIO TRACK", ImVec2(110, 25))) {
        std::string path = OpenAudioFileDialog();
        if (!path.empty()) audioManager.AddAudioTrack(path);
    }
    ImGui::PopStyleColor(2);
    ImGui::SameLine();
    
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.85f, 0.55f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.65f, 0.25f, 1.0f));
    if (ImGui::Button("+ TEXT TRACK", ImVec2(105, 25))) AddTextTrack();
    ImGui::PopStyleColor(2);
    ImGui::SameLine();

    const GameZone* az = GetActiveGameZone(globalFrame);
    if (az) {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.1f, 0.65f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.85f, 0.3f, 1.0f));
        if (ImGui::Button("PLAY GAME", ImVec2(100, 25))) {
            playModeRequested = true;
            playTimeLimitOut  = az->infiniteTime ? 999999.0f : az->timeLimitSecs;
        }
        ImGui::PopStyleColor(2);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.22f, 0.22f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text,   ImVec4(0.40f, 0.40f, 0.40f, 1.0f));
        ImGui::Button("PLAY GAME", ImVec2(100, 25));
        ImGui::PopStyleColor(2);
    }

    ImGui::SameLine(0, 15);
    ImGui::SetNextItemWidth(80);
    int dur = maxTimelineFrames / 60;
    if (ImGui::InputInt("s##dur", &dur, 10)) {
        if (dur < 10) dur = 10;
        if (dur > 3600) dur = 3600;
        maxTimelineFrames = dur * 60;
        if (globalFrame > maxTimelineFrames) globalFrame = maxTimelineFrames;
    }
    
    ImGui::SameLine(0, 15);
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "Zoom:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::SliderFloat("##zm", &scroller.zoomScale, 1.0f, 20.0f, "%.1fx");

    float rulerY  = p.y + scroller.topMargin; 
    
    // VISIBILITY REGIONS 
    float minVisibleY = rulerY + scroller.rulerH;
    float maxVisibleY = p.y + TL_H - scroller.scrollbarH;
    float currentY = minVisibleY + 4.0f - (scroller.exactScrollTrack * trackH);

    dl->AddRectFilled(ImVec2(p.x, rulerY), ImVec2(p.x + width, minVisibleY), IM_COL32(25, 25, 25, 255));

    float tlW = width - headerW;
    float virtualW = tlW * scroller.zoomScale;
    float pxPerSec = (virtualW / maxTimelineFrames) * 60.0f;
    int stepSecs = 1;
    if (pxPerSec < 40.0f) stepSecs = 2;
    if (pxPerSec < 20.0f) stepSecs = 5;
    if (pxPerSec < 10.0f) stepSecs = 10;
    if (pxPerSec <  5.0f) stepSecs = 30;
    if (pxPerSec <  2.0f) stepSecs = 60;

    dl->PushClipRect(ImVec2(p.x + headerW, p.y), ImVec2(p.x + width, maxVisibleY), true);
    for (int i = 0; i <= maxTimelineFrames; i += 60) {
        float tx = GetX(i);
        if (tx >= p.x + headerW - 50 && tx <= p.x + width + 50) { 
            dl->AddLine(ImVec2(tx, rulerY + 10), ImVec2(tx, minVisibleY), IM_COL32(120, 120, 120, 255));
            int sec = i / 60;
            if (sec % stepSecs == 0) {
                dl->AddText(ImVec2(tx + 2, rulerY), IM_COL32(120, 120, 120, 255), (std::to_string(sec) + "s").c_str());
            }
        }
    }
    dl->PopClipRect();

    ImGui::SetCursorScreenPos(ImVec2(p.x + headerW, rulerY));
    ImGui::InvisibleButton("ruler_scrub", ImVec2(tlW, scroller.rulerH));
    if (ImGui::IsItemActive() && (ImGui::IsMouseDown(ImGuiMouseButton_Left) || ImGui::IsMouseDragging(ImGuiMouseButton_Left))) {
        float mx = ImGui::GetIO().MousePos.x - (p.x + headerW) + scroller.scrollX;
        float ratio = std::clamp(mx / virtualW, 0.0f, 1.0f);
        globalFrame = (int)(ratio * maxTimelineFrames);
    }

    // =========================================================
    // GAME ZONES
    // =========================================================
    static float gzDragAccumL = 0.0f;
    static float gzDragAccumR = 0.0f;
    static float gzDragAccumB = 0.0f;
    int gzDel = -1;
    
    for (int gi = 0; gi < (int)gameZones.size(); gi++) {
        auto& gz  = gameZones[gi];
        
        // FIX: Re-engineered culling for smooth glides instead of popping
        bool isVisible = (currentY + trackH >= minVisibleY) && (currentY <= maxVisibleY);

        if (isVisible) {
            ImGui::PushID(7000 + gi);
            float gx0 = GetX(gz.startFrame);
            float gx1 = GetX(gz.endFrame);
            if (gx1 < gx0 + 22.0f) gx1 = gx0 + 22.0f;

            dl->AddRectFilled(ImVec2(p.x, currentY), ImVec2(p.x + width, currentY + trackH), IM_COL32(25, 40, 25, 255));
            dl->AddLine(ImVec2(p.x, currentY + trackH), ImVec2(p.x + width, currentY + trackH), IM_COL32(40, 60, 40, 255));

            dl->AddRectFilled(ImVec2(p.x, currentY), ImVec2(p.x + headerW, currentY + trackH), IM_COL32(20, 30, 20, 255));
            std::string lbl = gz.label + (gz.infiniteTime ? " [INF]" : " [" + std::to_string((int)gz.timeLimitSecs) + "s]");
            dl->AddText(ImVec2(p.x + 5, currentY + 4), IM_COL32(100, 255, 100, 255), lbl.c_str());

            ImGui::SetCursorScreenPos(ImVec2(p.x + headerW - 65, currentY + 14));
            ImGui::SetNextItemWidth(45);
            ImGui::DragFloat("##gzt", &gz.timeLimitSecs, 0.5f, 1.0f, 3600.0f, "%.0f");
            
            ImGui::SetCursorScreenPos(ImVec2(p.x + headerW - 105, currentY + 14));
            ImGui::Checkbox("INF##gzi", &gz.infiniteTime);
            
            ImGui::SetCursorScreenPos(ImVec2(p.x + headerW - 25, currentY + 2));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.1f, 0.1f, 1.0f));
            if (ImGui::SmallButton("X##gzd")) gzDel = gi;
            ImGui::PopStyleColor();

            dl->PushClipRect(ImVec2(p.x + headerW, minVisibleY), ImVec2(p.x + width, maxVisibleY), true);
            dl->AddRectFilled(ImVec2(gx0, currentY), ImVec2(gx1, currentY + trackH + 500.0f), IM_COL32(110, 45, 180, 55)); 
            dl->AddRectFilled(ImVec2(gx0, currentY + 2), ImVec2(gx1, currentY + trackH - 2), IM_COL32(50, 150, 50, 200), 4.0f);
            dl->AddText(ImVec2(gx0 + 5, currentY + 4), IM_COL32(255, 255, 255, 255), "Game Zone");
            dl->PopClipRect();

            float btnSx = std::max(gx0, p.x + headerW);
            float btnEx = std::min(gx1, p.x + width);

            if (btnEx > p.x + headerW && btnSx < p.x + width) {
                ImGui::SetCursorScreenPos(ImVec2(btnSx - 5, currentY));
                ImGui::InvisibleButton("gzL", ImVec2(10, trackH));
                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    gzDragAccumL += GetFrameDelta(ImGui::GetIO().MouseDelta.x);
                    int df = (int)gzDragAccumL;
                    if (df != 0) {
                        if (gz.startFrame + df > gz.endFrame - 30) df = gz.endFrame - 30 - gz.startFrame;
                        gz.startFrame = std::max(0, gz.startFrame + df);
                        gz.timeLimitSecs = (float)(gz.endFrame - gz.startFrame) / 60.0f;
                        gzDragAccumL -= df;
                    }
                } else if (ImGui::IsItemDeactivated()) { gzDragAccumL = 0.0f; }

                ImGui::SetCursorScreenPos(ImVec2(btnEx - 5, currentY));
                ImGui::InvisibleButton("gzR", ImVec2(10, trackH));
                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    gzDragAccumR += GetFrameDelta(ImGui::GetIO().MouseDelta.x);
                    int df = (int)gzDragAccumR;
                    if (df != 0) {
                        if (gz.endFrame + df < gz.startFrame + 30) df = gz.startFrame + 30 - gz.endFrame;
                        gz.endFrame += df;
                        gz.timeLimitSecs = (float)(gz.endFrame - gz.startFrame) / 60.0f;
                        gzDragAccumR -= df;
                    }
                } else if (ImGui::IsItemDeactivated()) { gzDragAccumR = 0.0f; }

                float bw = (btnEx - btnSx) - 14.0f;
                if (bw > 4.0f) {
                    ImGui::SetCursorScreenPos(ImVec2(btnSx + 7, currentY));
                    ImGui::InvisibleButton("gzB", ImVec2(bw, trackH));
                    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                        gzDragAccumB += GetFrameDelta(ImGui::GetIO().MouseDelta.x);
                        int df = (int)gzDragAccumB;
                        if (df != 0) {
                            int len = gz.endFrame - gz.startFrame;
                            if (gz.startFrame + df < 0) df = -gz.startFrame;
                            gz.startFrame += df;
                            gz.endFrame = gz.startFrame + len;
                            gzDragAccumB -= df;
                        }
                    } else if (ImGui::IsItemDeactivated()) { gzDragAccumB = 0.0f; }
                }
            }
            ImGui::PopID();
        }
        currentY += trackH;
    }
    if (gzDel >= 0) gameZones.erase(gameZones.begin() + gzDel);

    // =========================================================
    // CAMERA TRACKS
    // =========================================================
    static float camDragAccumL = 0.0f;
    static float camDragAccumR = 0.0f;
    static float camDragAccumB = 0.0f;
    int camDel = -1;
    
    for (size_t i = 0; i < cameraTracks.size(); i++) {
        auto& ct = cameraTracks[i];
        bool isVisible = (currentY + trackH >= minVisibleY) && (currentY <= maxVisibleY);

        if (isVisible) {
            ImGui::PushID(5000 + (int)i);
            dl->AddRectFilled(ImVec2(p.x, currentY), ImVec2(p.x + width, currentY + trackH), IM_COL32(35,35,45,255));
            dl->AddLine(ImVec2(p.x, currentY + trackH), ImVec2(p.x + width, currentY + trackH), IM_COL32(60,60,70,255));

            dl->AddRectFilled(ImVec2(p.x, currentY), ImVec2(p.x + headerW, currentY + trackH), IM_COL32(25,25,35,255));
            dl->AddText(ImVec2(p.x + 5, currentY + 4), IM_COL32(200, 100, 255, 255), ct.savedTrackName.c_str());

            ImGui::SetCursorScreenPos(ImVec2(p.x + headerW - 25, currentY + 2));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            if (ImGui::SmallButton("X##delcam")) camDel = (int)i;
            ImGui::PopStyleColor();

            float sx = GetX(ct.timelineStartFrame);
            float ex = GetX(ct.timelineEndFrame);
            if (ex < sx + 12.0f) ex = sx + 12.0f;

            dl->PushClipRect(ImVec2(p.x + headerW, minVisibleY), ImVec2(p.x + width, maxVisibleY), true);
            ImU32 cc = IM_COL32(140, 50, 200, 255); 
            ImU32 hc = IM_COL32(210, 210, 210, 160);
            dl->AddRectFilled(ImVec2(sx, currentY + 2), ImVec2(ex, currentY + trackH - 2), cc, 4.0f);
            dl->AddRectFilled(ImVec2(sx, currentY + 2), ImVec2(sx + 8, currentY + trackH - 2), hc, 4.0f);
            dl->AddRectFilled(ImVec2(ex - 8, currentY + 2), ImVec2(ex, currentY + trackH - 2), hc, 4.0f);
            dl->AddText(ImVec2(sx + 10, currentY + 4), IM_COL32(255, 255, 255, 255), "Flight Path");
            dl->PopClipRect();

            float btnSx = std::max(sx, p.x + headerW);
            float btnEx = std::min(ex, p.x + width);

            if (btnEx > p.x + headerW && btnSx < p.x + width) {
                ImGui::SetCursorScreenPos(ImVec2(btnSx, currentY));
                ImGui::InvisibleButton("lh", ImVec2(8, trackH));
                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    camDragAccumL += GetFrameDelta(ImGui::GetIO().MouseDelta.x);
                    int df = (int)camDragAccumL;
                    if (df != 0) {
                        if (ct.timelineStartFrame + df > ct.timelineEndFrame - 10) df = ct.timelineEndFrame - 10 - ct.timelineStartFrame;
                        ct.timelineStartFrame += df;
                        ct.trimStart += (float)df / 60.0f; 
                        camDragAccumL -= df;
                    }
                } else if (ImGui::IsItemDeactivated()) { camDragAccumL = 0.0f; }

                ImGui::SetCursorScreenPos(ImVec2(btnEx - 8, currentY));
                ImGui::InvisibleButton("rh", ImVec2(8, trackH));
                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    camDragAccumR += GetFrameDelta(ImGui::GetIO().MouseDelta.x);
                    int df = (int)camDragAccumR;
                    if (df != 0) {
                        if (ct.timelineEndFrame + df < ct.timelineStartFrame + 10) df = ct.timelineStartFrame + 10 - ct.timelineEndFrame;
                        ct.timelineEndFrame += df;
                        camDragAccumR -= df;
                    }
                } else if (ImGui::IsItemDeactivated()) { camDragAccumR = 0.0f; }

                float bw = (btnEx - btnSx) - 16.0f;
                if (bw > 2.0f) {
                    ImGui::SetCursorScreenPos(ImVec2(btnSx + 8, currentY));
                    ImGui::InvisibleButton("body", ImVec2(bw, trackH));
                    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                        camDragAccumB += GetFrameDelta(ImGui::GetIO().MouseDelta.x);
                        int df = (int)camDragAccumB;
                        if (df != 0) {
                            if (ct.timelineStartFrame + df < 0) df = -ct.timelineStartFrame;
                            ct.timelineStartFrame += df;
                            ct.timelineEndFrame += df;
                            camDragAccumB -= df;
                        }
                    } else if (ImGui::IsItemDeactivated()) { camDragAccumB = 0.0f; }
                }
            }
            ImGui::PopID();
        }
        currentY += trackH;
    }
    if (camDel >= 0) cameraTracks.erase(cameraTracks.begin() + camDel);

    // =========================================================
    // AUDIO TRACKS 
    // =========================================================
    static float audDragAccumL = 0.0f;
    static float audDragAccumR = 0.0f;
    static float audDragAccumB = 0.0f;
    int audioDel = -1;
    
    for (size_t i = 0; i < audioManager.tracks.size(); i++) {
        auto& at = audioManager.tracks[i];
        bool isVisible = (currentY + trackH >= minVisibleY) && (currentY <= maxVisibleY);

        if (isVisible) {
            ImGui::PushID(9000 + (int)i);
            dl->AddRectFilled(ImVec2(p.x, currentY), ImVec2(p.x + width, currentY + trackH), IM_COL32(35,35,45,255));
            dl->AddLine(ImVec2(p.x, currentY + trackH), ImVec2(p.x + width, currentY + trackH), IM_COL32(60,60,70,255));

            dl->AddRectFilled(ImVec2(p.x, currentY), ImVec2(p.x + headerW, currentY + trackH), IM_COL32(25,25,35,255));
            dl->AddText(ImVec2(p.x + 5, currentY + 4), IM_COL32(100, 255, 200, 255), at.assetName.c_str());

            ImGui::SetCursorScreenPos(ImVec2(p.x + headerW - 25, currentY + 2));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            if (ImGui::SmallButton("X")) audioDel = (int)i;
            ImGui::PopStyleColor();

            float sx = GetX(at.startFrame);
            float ex = GetX(at.startFrame + at.durationFrames);
            if (ex < sx + 12.0f) ex = sx + 12.0f;

            dl->PushClipRect(ImVec2(p.x + headerW, minVisibleY), ImVec2(p.x + width, maxVisibleY), true);
            ImU32 cc = IM_COL32(40, 150, 150, 200); 
            ImU32 hc = IM_COL32(80, 190, 190, 160); 
            dl->AddRectFilled(ImVec2(sx, currentY + 2), ImVec2(ex, currentY + trackH - 2), cc, 4.0f);
            dl->AddRectFilled(ImVec2(sx, currentY + 2), ImVec2(sx + 8, currentY + trackH - 2), hc, 4.0f);
            dl->AddRectFilled(ImVec2(ex - 8, currentY + 2), ImVec2(ex, currentY + trackH - 2), hc, 4.0f);
            
            for (float wx = sx + 12; wx < ex - 12; wx += 6) {
                float waveHeight = 4.0f + 8.0f * std::abs(sin((wx * 0.1f) + at.startFrame));
                dl->AddLine(ImVec2(wx, currentY + trackH/2 - waveHeight), 
                            ImVec2(wx, currentY + trackH/2 + waveHeight), 
                            IM_COL32(100, 255, 255, 100), 2.0f);
            }
            
            dl->AddText(ImVec2(sx + 10, currentY + 4), IM_COL32(255, 255, 255, 255), "Audio Stream");
            dl->PopClipRect();

            float btnSx = std::max(sx, p.x + headerW);
            float btnEx = std::min(ex, p.x + width);

            if (btnEx > p.x + headerW && btnSx < p.x + width) {
                ImGui::SetCursorScreenPos(ImVec2(btnSx, currentY));
                ImGui::InvisibleButton("lh", ImVec2(8, trackH));
                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    audDragAccumL += GetFrameDelta(ImGui::GetIO().MouseDelta.x);
                    int df = (int)audDragAccumL;
                    if (df != 0) {
                        if (df > at.durationFrames - 10) df = at.durationFrames - 10;
                        if (at.trimStartSecs + ((float)df / 60.0f) < 0.0f) df = -(int)(at.trimStartSecs * 60.0f); 
                        
                        at.startFrame += df;
                        at.durationFrames -= df;
                        at.trimStartSecs += (float)df / 60.0f;
                        audDragAccumL -= df;
                    }
                } else if (ImGui::IsItemDeactivated()) { audDragAccumL = 0.0f; }

                ImGui::SetCursorScreenPos(ImVec2(btnEx - 8, currentY));
                ImGui::InvisibleButton("rh", ImVec2(8, trackH));
                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    audDragAccumR += GetFrameDelta(ImGui::GetIO().MouseDelta.x);
                    int df = (int)audDragAccumR;
                    if (df != 0) {
                        if (at.durationFrames + df < 10) df = 10 - at.durationFrames;
                        at.durationFrames += df;
                        audDragAccumR -= df;
                    }
                } else if (ImGui::IsItemDeactivated()) { audDragAccumR = 0.0f; }

                float bw = (btnEx - btnSx) - 16.0f;
                if (bw > 2.0f) {
                    ImGui::SetCursorScreenPos(ImVec2(btnSx + 8, currentY));
                    ImGui::InvisibleButton("body", ImVec2(bw, trackH));
                    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                        audDragAccumB += GetFrameDelta(ImGui::GetIO().MouseDelta.x);
                        int df = (int)audDragAccumB;
                        if (df != 0) {
                            if (at.startFrame + df < 0) df = -at.startFrame;
                            at.startFrame += df;
                            audDragAccumB -= df;
                        }
                    } else if (ImGui::IsItemDeactivated()) { audDragAccumB = 0.0f; }
                }
            }
            ImGui::PopID();
        }
        currentY += trackH;
    }
    if (audioDel >= 0) audioManager.RemoveTrack(audioDel);

    // =========================================================
    // TEXT TRACKS
    // =========================================================
    static float txtDragAccumL = 0.0f;
    static float txtDragAccumR = 0.0f;
    static float txtDragAccumB = 0.0f;
    int textDel = -1;
    
    for (size_t i = 0; i < textTracks.size(); i++) {
        auto& txt = textTracks[i];
        bool isVisible = (currentY + trackH >= minVisibleY) && (currentY <= maxVisibleY);

        if (isVisible) {
            ImGui::PushID(12000 + (int)i);
            dl->AddRectFilled(ImVec2(p.x, currentY), ImVec2(p.x + width, currentY + trackH), IM_COL32(35,35,45,255));
            dl->AddLine(ImVec2(p.x, currentY + trackH), ImVec2(p.x + width, currentY + trackH), IM_COL32(60,60,70,255));

            dl->AddRectFilled(ImVec2(p.x, currentY), ImVec2(p.x + headerW, currentY + trackH), IM_COL32(25,25,35,255));
            if (selectedTextIndex == (int)i) {
                dl->AddRectFilled(ImVec2(p.x, currentY), ImVec2(p.x + headerW, currentY + trackH), IM_COL32(180, 100, 40, 80));
            }
            
            dl->AddText(ImVec2(p.x + 5, currentY + 4), IM_COL32(255, 180, 50, 255), txt.trackName.c_str());

            ImGui::SetCursorScreenPos(ImVec2(p.x + headerW - 65, currentY + 2));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
            if (ImGui::SmallButton("EDIT")) {
                selectedTextIndex = (int)i;
                selectedObjectIndex = -1;
            }
            ImGui::PopStyleColor();

            ImGui::SetCursorScreenPos(ImVec2(p.x + headerW - 25, currentY + 2));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            if (ImGui::SmallButton("X")) textDel = (int)i;
            ImGui::PopStyleColor();

            float sx = GetX(txt.startFrame);
            float ex = GetX(txt.endFrame);
            if (ex < sx + 12.0f) ex = sx + 12.0f;

            dl->PushClipRect(ImVec2(p.x + headerW, minVisibleY), ImVec2(p.x + width, maxVisibleY), true);
            ImU32 cc = IM_COL32(200, 140, 40, 200); 
            ImU32 hc = IM_COL32(230, 170, 70, 160);
            dl->AddRectFilled(ImVec2(sx, currentY + 2), ImVec2(ex, currentY + trackH - 2), cc, 4.0f);
            dl->AddRectFilled(ImVec2(sx, currentY + 2), ImVec2(sx + 8, currentY + trackH - 2), hc, 4.0f);
            dl->AddRectFilled(ImVec2(ex - 8, currentY + 2), ImVec2(ex, currentY + trackH - 2), hc, 4.0f);
            dl->AddText(ImVec2(sx + 10, currentY + 4), IM_COL32(255, 255, 255, 255), "Text FX");
            dl->PopClipRect();

            float btnSx = std::max(sx, p.x + headerW);
            float btnEx = std::min(ex, p.x + width);

            if (btnEx > p.x + headerW && btnSx < p.x + width) {
                ImGui::SetCursorScreenPos(ImVec2(btnSx, currentY));
                ImGui::InvisibleButton("lh", ImVec2(8, trackH));
                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    txtDragAccumL += GetFrameDelta(ImGui::GetIO().MouseDelta.x);
                    int df = (int)txtDragAccumL;
                    if (df != 0) {
                        if (txt.startFrame + df > txt.endFrame - 10) df = txt.endFrame - 10 - txt.startFrame;
                        txt.startFrame += df;
                        if (txt.startFrame < 0) txt.startFrame = 0;
                        txtDragAccumL -= df;
                    }
                } else if (ImGui::IsItemDeactivated()) { txtDragAccumL = 0.0f; }

                ImGui::SetCursorScreenPos(ImVec2(btnEx - 8, currentY));
                ImGui::InvisibleButton("rh", ImVec2(8, trackH));
                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    txtDragAccumR += GetFrameDelta(ImGui::GetIO().MouseDelta.x);
                    int df = (int)txtDragAccumR;
                    if (df != 0) {
                        if (txt.endFrame + df < txt.startFrame + 10) df = txt.startFrame + 10 - txt.endFrame;
                        txt.endFrame += df;
                        txtDragAccumR -= df;
                    }
                } else if (ImGui::IsItemDeactivated()) { txtDragAccumR = 0.0f; }

                float bw = (btnEx - btnSx) - 16.0f;
                if (bw > 2.0f) {
                    ImGui::SetCursorScreenPos(ImVec2(btnSx + 8, currentY));
                    ImGui::InvisibleButton("body", ImVec2(bw, trackH));
                    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                        txtDragAccumB += GetFrameDelta(ImGui::GetIO().MouseDelta.x);
                        int df = (int)txtDragAccumB;
                        if (df != 0) {
                            int len = txt.endFrame - txt.startFrame;
                            if (txt.startFrame + df < 0) df = -txt.startFrame;
                            txt.startFrame += df; 
                            txt.endFrame = txt.startFrame + len;
                            txtDragAccumB -= df;
                        }
                    } else if (ImGui::IsItemDeactivated()) { txtDragAccumB = 0.0f; }
                }
            }
            ImGui::PopID();
        }
        currentY += trackH;
    }
    if (textDel >= 0) {
        textTracks.erase(textTracks.begin() + textDel);
        if (selectedTextIndex == textDel) selectedTextIndex = -1;
        else if (selectedTextIndex > textDel) selectedTextIndex--;
    }

    // =========================================================
    // 3D OBJECT TRACKS
    // =========================================================
    static float objDragAccumL = 0.0f;
    static float objDragAccumR = 0.0f;
    static float objDragAccumB = 0.0f;

    for (size_t i = 0; i < tracks.size(); i++) {
        auto& t = tracks[i];
        bool isVisible = (currentY + trackH >= minVisibleY) && (currentY <= maxVisibleY);

        if (isVisible) {
            ImGui::PushID((int)i);
            dl->AddRectFilled(ImVec2(p.x, currentY), ImVec2(p.x + width, currentY + trackH), IM_COL32(35,35,45,255));
            dl->AddLine(ImVec2(p.x, currentY + trackH), ImVec2(p.x + width, currentY + trackH), IM_COL32(60,60,70,255));

            dl->AddRectFilled(ImVec2(p.x, currentY), ImVec2(p.x + headerW, currentY + trackH), IM_COL32(25,25,35,255));
            
            if (selectedObjectIndex == (int)i) {
                dl->AddRectFilled(ImVec2(p.x, currentY), ImVec2(p.x + headerW, currentY + trackH), IM_COL32(40, 100, 180, 80));
            }

            ImU32 nameCol = t.isEnvironment ? IM_COL32(100, 200, 255, 255) : IM_COL32(200, 200, 200, 255);
            dl->AddText(ImVec2(p.x + 5, currentY + 4), nameCol, t.assetName.c_str());

            ImGui::SetCursorScreenPos(ImVec2(p.x + headerW - 25, currentY + 2));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            if (ImGui::SmallButton("X")) deleteIndex = (int)i;
            ImGui::PopStyleColor();

            float sx = GetX(t.startFrame);
            float ex = GetX(t.endFrame);
            if (ex < sx + 12.0f) ex = sx + 12.0f;

            dl->PushClipRect(ImVec2(p.x + headerW, minVisibleY), ImVec2(p.x + width, maxVisibleY), true);
            ImU32 cc = t.isEnvironment ? IM_COL32(70, 140, 70, 255) : IM_COL32(140, 70, 70, 255);
            ImU32 hc = IM_COL32(210, 210, 210, 160);
            dl->AddRectFilled(ImVec2(sx, currentY + 2), ImVec2(ex, currentY + trackH - 2), cc, 4.0f);
            dl->AddRectFilled(ImVec2(sx, currentY + 2), ImVec2(sx + 8, currentY + trackH - 2), hc, 4.0f);
            dl->AddRectFilled(ImVec2(ex - 8, currentY + 2), ImVec2(ex, currentY + trackH - 2), hc, 4.0f);
            dl->PopClipRect();

            float btnSx = std::max(sx, p.x + headerW);
            float btnEx = std::min(ex, p.x + width);

            if (btnEx > p.x + headerW && btnSx < p.x + width) {
                
                // --- LEFT HANDLE (SHAVE & TRIM OFFSET) ---
                ImGui::SetCursorScreenPos(ImVec2(btnSx, currentY));
                ImGui::InvisibleButton("lh", ImVec2(8, trackH));
                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    objDragAccumL += GetFrameDelta(ImGui::GetIO().MouseDelta.x);
                    int df = (int)objDragAccumL;
                    if (df != 0) {
                        if (t.startFrame + df > t.endFrame - 10) df = t.endFrame - 10 - t.startFrame;
                        if (t.trimStartFrames + df < 0) df = -t.trimStartFrames; 
                        
                        t.startFrame += df;
                        t.trimStartFrames += df; 
                        objDragAccumL -= df;
                    }
                } else if (ImGui::IsItemDeactivated()) { objDragAccumL = 0.0f; }

                // RIGHT HANDLE
                ImGui::SetCursorScreenPos(ImVec2(btnEx - 8, currentY));
                ImGui::InvisibleButton("rh", ImVec2(8, trackH));
                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    objDragAccumR += GetFrameDelta(ImGui::GetIO().MouseDelta.x);
                    int df = (int)objDragAccumR;
                    if (df != 0) {
                        if (t.endFrame + df < t.startFrame + 10) df = t.startFrame + 10 - t.endFrame;
                        t.endFrame += df;
                        objDragAccumR -= df;
                    }
                } else if (ImGui::IsItemDeactivated()) { objDragAccumR = 0.0f; }

                // BODY
                float bw = (btnEx - btnSx) - 16.0f;
                if (bw > 2.0f) {
                    ImGui::SetCursorScreenPos(ImVec2(btnSx + 8, currentY));
                    ImGui::InvisibleButton("body", ImVec2(bw, trackH));
                    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                        objDragAccumB += GetFrameDelta(ImGui::GetIO().MouseDelta.x);
                        int df = (int)objDragAccumB;
                        if (df != 0) {
                            int len = t.endFrame - t.startFrame;
                            if (t.startFrame + df < 0) df = -t.startFrame;
                            t.startFrame += df; 
                            t.endFrame = t.startFrame + len;
                            objDragAccumB -= df;
                        }
                    } else if (ImGui::IsItemDeactivated()) { objDragAccumB = 0.0f; }
                }
            }
            ImGui::PopID();
        }
        currentY += trackH;
    }

    // =========================================================
    // THE FIX: PERFECT NEEDLE RENDERING
    // =========================================================
    float nx = GetX(globalFrame);
    if (nx >= p.x + headerW && nx <= p.x + width) {
        // Clips the needle safely under the ruler and above the scrollbar
        dl->PushClipRect(ImVec2(p.x + headerW, p.y), ImVec2(p.x + width, maxVisibleY), true);
        
        // Locks the vertical line exactly between the ruler and the absolute bottom track edge
        dl->AddLine(ImVec2(nx, rulerY), ImVec2(nx, maxVisibleY), IM_COL32(255, 210, 0, 255), 2.0f);
        
        // Draws the needle head securely on the timeline ruler
        dl->AddTriangleFilled(ImVec2(nx, rulerY + scroller.rulerH), ImVec2(nx - 7,  rulerY + 6), ImVec2(nx + 7,  rulerY + 6), IM_COL32(255, 210, 0, 255));
        dl->PopClipRect();
    }

    // DRAW THE DELEGATED CUSTOM SCROLLBARS ON TOP 
    scroller.DrawScrollbars(dl, p, width, TL_H, headerW, trackH, trackCount, maxTimelineFrames);

    ImGui::End();
}