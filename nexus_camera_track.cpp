#include "nexus_camera_track.h"
#include "imgui.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

void NexusCameraTrack::Evaluate(float t, Vector3& outPos, Vector3& outTarget, float& outFov) const {
    rig.Evaluate(t, waypoints, mode, loop, totalDuration, outPos, outTarget, outFov);
}

void NexusCameraTrack::Update(float dt) {
    if (!isActive || waypoints.empty()) return;
    playheadTime += dt;
    if (loop && totalDuration > 0.0f) {
        playheadTime = fmodf(playheadTime, totalDuration);
    } else if (playheadTime >= totalDuration) {
        playheadTime = totalDuration;
        isActive = false;
    }
}

void NexusCameraTrack::ApplyToCamera(Camera3D& cam) const {
    if (waypoints.empty()) return; 
    
    Vector3 pos, tgt; float fov;
    Evaluate(playheadTime, pos, tgt, fov);
    cam.position = pos;
    cam.target   = tgt;
    cam.fovy     = fov;
}

void NexusCameraTrack::RecordWaypoint(const Camera3D& cam) {
    CameraWaypoint wp;
    wp.position    = cam.position;
    wp.target      = cam.target;
    wp.fov         = cam.fovy;
    wp.transitTime = waypoints.empty() ? 0.0f : 2.0f; // Default flight time of 2s
    wp.holdTime    = 1.0f;                            // Default hold time of 1s
    wp.label       = "WP " + std::to_string((int)waypoints.size());
    
    waypoints.push_back(wp);
    RefreshDuration();
}

void NexusCameraTrack::DeleteWaypoint(int index) {
    if (index < 0 || index >= (int)waypoints.size()) return;
    waypoints.erase(waypoints.begin() + index);
    if (selectedWaypoint >= (int)waypoints.size()) selectedWaypoint = (int)waypoints.size() - 1;
    RefreshDuration();
    if (waypoints.empty()) isActive = false; 
}

void NexusCameraTrack::RefreshDuration() {
    totalDuration = rig.CalculateDuration(waypoints);
}

void NexusCameraTrack::DrawPath() const {
    if (!showWaypoints) return;  
    if (waypoints.size() < 2) {
        if (!waypoints.empty())
            DrawSphere(waypoints[0].position, 0.2f, YELLOW);
        return;
    }

    int steps = 40 * (int)(waypoints.size() - 1);
    float dur = totalDuration > 0.0f ? totalDuration : 1.0f;
    Vector3 prev = waypoints[0].position;

    for (int i = 1; i <= steps; i++) {
        float t = ((float)i / steps) * dur;
        Vector3 pos, tgt; float fov;
        Evaluate(t, pos, tgt, fov);
        DrawLine3D(prev, pos, YELLOW);
        prev = pos;
    }

    for (int i = 0; i < (int)waypoints.size(); i++) {
        Color col = (i == selectedWaypoint) ? ORANGE : YELLOW;
        DrawSphere(waypoints[i].position, 0.22f, col);
        DrawSphere(waypoints[i].target,   0.12f, Fade(SKYBLUE, 0.7f));
        DrawLine3D(waypoints[i].position, waypoints[i].target, Fade(SKYBLUE, 0.4f));
    }
}

void NexusCameraTrack::DrawUI(bool& isOpen, Camera3D& editorCam, float currentTimeSeconds) {
    if (!isOpen) return;

    ImGui::SetNextWindowSize(ImVec2(420, 560), ImGuiCond_FirstUseEver);
    ImGui::Begin("Camera Track Editor", &isOpen);

    // ---------------------------------------------------------------
    // ADD WAYPOINT
    // ---------------------------------------------------------------
    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f), "Add Camera Waypoint");
    ImGui::TextDisabled("Fly editor camera to position and append to sequence.");
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.15f, 0.55f, 0.8f,  1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f,  0.7f,  1.0f,  1.0f));
    if (ImGui::Button("Add Waypoint Here", ImVec2(-1, 35))) {
        RecordWaypoint(editorCam);
    }
    ImGui::PopStyleColor(2);

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    // ---------------------------------------------------------------
    // SAVE / LOAD + SAVE TO TIMELINE
    // ---------------------------------------------------------------
    ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Save / Load Track");

    char saveBuf[256]; strncpy(saveBuf, saveFilePath.c_str(), 255); saveBuf[255]='\0';
    ImGui::SetNextItemWidth(200);
    if (ImGui::InputText("##savepath", saveBuf, 256)) saveFilePath = saveBuf;
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.55f, 0.2f, 1.0f));
    if (ImGui::Button("Save File##ctsave")) SaveTrack(saveFilePath);
    ImGui::PopStyleColor();
    ImGui::SameLine();
    if (ImGui::Button("Load##ctload")) LoadTrack(saveFilePath);

    ImGui::Spacing();
    ImGui::Checkbox("Show waypoints in editor##shwp", &showWaypoints);
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.5f, 0.15f, 0.75f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.65f,0.25f, 0.95f, 1.0f));
    if (ImGui::Button("Save to Timeline", ImVec2(-1, 30))) {
        if (!waypoints.empty()) {
            savedTrackCount++;
            savedTrackName    = "Camera " + std::to_string(savedTrackCount);
            savedTrackDuration = totalDuration; 
            saveToTimeline    = true; 
        }
    }
    ImGui::PopStyleColor(2);

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    // ---------------------------------------------------------------
    // TIMING & INTERPOLATION MODE
    // ---------------------------------------------------------------
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "Master Speed Multiplier");
    
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##pbsp", &rig.playbackSpeed, 0.1f, 5.0f, "Global Flight Speed: %.2fx")) {
        RefreshDuration();
    }
    ImGui::TextDisabled("Scales travel time. Hold times remain completely unaffected.");
    ImGui::Spacing();
    
    int m = (int)mode;
    ImGui::RadioButton("Spline##m",  &m, 0); ImGui::SameLine();
    ImGui::RadioButton("Linear##m",  &m, 1); ImGui::SameLine();
    ImGui::RadioButton("Cut##m",     &m, 2);
    mode = (CamTrackMode)m;
    ImGui::Checkbox("Loop##lp", &loop);
    ImGui::SameLine();
    ImGui::Checkbox("Include in Publish##pub", &publishEnabled);

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    // ---------------------------------------------------------------
    // PREVIEW PLAYBACK
    // ---------------------------------------------------------------
    ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Preview Playback");
    ImGui::TextDisabled("Total Track Time: %.2fs", totalDuration);

    ImGui::SetNextItemWidth(-1);
    ImGui::SliderFloat("##ph", &playheadTime, 0.0f, std::max(totalDuration, 0.01f), "%.2fs");
    bool isScrubbing = ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left);

    if (isActive) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("STOP PREVIEW##stop", ImVec2(-1, 28))) isActive = false;
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.6f, 0.25f, 1.0f));
        if (ImGui::Button("PREVIEW TRACK##prev", ImVec2(-1, 28))) {
            if (!waypoints.empty()) { 
                isActive = true;
                playheadTime = 0.0f;
            }
        }
        ImGui::PopStyleColor();
    }

    if ((isActive || isScrubbing) && !waypoints.empty()) {
        Vector3 pos, tgt; float fov;
        Evaluate(playheadTime, pos, tgt, fov);
        editorCam.position = pos;
        editorCam.target   = tgt;
        editorCam.fovy     = fov;
    }

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    // ---------------------------------------------------------------
    // SEQUENCE LIST
    // ---------------------------------------------------------------
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "Sequence List (%d)", (int)waypoints.size());
    ImGui::Spacing();

    int toDelete = -1;
    for (int i = 0; i < (int)waypoints.size(); i++) {
        ImGui::PushID(i);

        bool sel = (selectedWaypoint == i);
        if (sel) ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));

        std::string hdr = (waypoints[i].label.empty() ? "WP " + std::to_string(i) : waypoints[i].label);
        bool open = ImGui::CollapsingHeader(hdr.c_str());

        if (sel) ImGui::PopStyleColor();
        if (ImGui::IsItemClicked()) selectedWaypoint = i;

        if (open) {
            char lb[64]; strncpy(lb, waypoints[i].label.c_str(), 63); lb[63]='\0';
            ImGui::SetNextItemWidth(160);
            if (ImGui::InputText("Label##wl", lb, 64)) waypoints[i].label = lb;
            
            // THE FIX: Decoupled Timing Editing!
            if (i > 0) {
                ImGui::TextDisabled("Travel Time (from WP %d):", i - 1);
                if (ImGui::DragFloat("##wt", &waypoints[i].transitTime, 0.1f, 0.0f, 600.0f, "%.1fs")) RefreshDuration();
            } else {
                ImGui::TextDisabled("Initial Delay Before Start:");
                if (ImGui::DragFloat("##wt", &waypoints[i].transitTime, 0.1f, 0.0f, 600.0f, "%.1fs")) RefreshDuration();
            }
            
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "Hold Time (At this point):");
            if (ImGui::DragFloat("##wh", &waypoints[i].holdTime, 0.1f, 0.0f, 3600.0f, "%.1fs")) RefreshDuration();
            ImGui::Spacing();

            ImGui::DragFloat3("Pos##wp",  &waypoints[i].position.x, 0.1f);
            ImGui::DragFloat3("Tgt##wg",  &waypoints[i].target.x,   0.1f);
            ImGui::DragFloat("FOV##wf",   &waypoints[i].fov, 0.5f, 10.0f, 120.0f);
            
            if (ImGui::SmallButton("Snap to Cam##ws")) {
                waypoints[i].position = editorCam.position;
                waypoints[i].target   = editorCam.target;
                waypoints[i].fov      = editorCam.fovy;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Go To##wg2")) {
                editorCam.position = waypoints[i].position;
                editorCam.target   = waypoints[i].target;
                editorCam.fovy     = waypoints[i].fov;
                
                // Calculates exactly where this waypoint begins in absolute time
                float tgtTime = 0.0f;
                for (int p = 0; p <= i; p++) {
                    tgtTime += (waypoints[p].transitTime / rig.playbackSpeed);
                    if (p < i) tgtTime += waypoints[p].holdTime;
                }
                playheadTime = tgtTime;
            }
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
            if (ImGui::SmallButton("Delete##wd")) toDelete = i;
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }
        ImGui::PopID();
    }

    if (toDelete >= 0) DeleteWaypoint(toDelete);

    if (waypoints.empty())
        ImGui::TextDisabled("No waypoints yet. Fly the camera and click Add Waypoint Here.");

    if (!waypoints.empty()) {
        ImGui::Spacing(); ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.1f, 0.1f, 1.0f));
        if (ImGui::Button("Clear Sequence##clr", ImVec2(-1, 26))) {
            waypoints.clear(); selectedWaypoint=-1;
            totalDuration=0.f; playheadTime=0.f; isActive=false;
        }
        ImGui::PopStyleColor();
    }

    ImGui::End();
}

void NexusCameraTrack::SaveTrack(const std::string& path) const {
    std::ofstream f(path);
    if (!f.is_open()) return;
    f << "[NexusCameraTrack]\n";
    f << "Version=2\n"; // Tagging new save system!
    f << "Mode=" << (int)mode << "\n";
    f << "Loop=" << (loop ? "1" : "0") << "\n";
    f << "PlaybackSpeed=" << rig.playbackSpeed << "\n";
    for (const auto& wp : waypoints) {
        f << "WP=" << wp.transitTime << ","
          << wp.position.x << "," << wp.position.y << "," << wp.position.z << ","
          << wp.target.x   << "," << wp.target.y   << "," << wp.target.z   << ","
          << wp.fov << "," << wp.holdTime << "," << wp.label << "\n";
    }
    f.close();
    TraceLog(LOG_INFO, "CAMERA TRACK: Saved to %s", path.c_str());
}

void NexusCameraTrack::LoadTrack(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return;
    waypoints.clear();
    std::string line;
    
    int version = 1; // Default to v1 for backwards compatibility
    
    while (std::getline(f, line)) {
        if (line.find("Version=") == 0) version = std::stoi(line.substr(8));
        else if (line.find("Mode=") == 0) mode = (CamTrackMode)std::stoi(line.substr(5));
        else if (line.find("Loop=") == 0) loop = (line.substr(5) == "1");
        else if (line.find("PlaybackSpeed=") == 0) rig.playbackSpeed = std::stof(line.substr(14));
        else if (line.find("WP=") == 0) {
            std::istringstream ss(line.substr(3));
            std::string tok; CameraWaypoint wp;
            auto g = [&](float& v){ std::getline(ss,tok,','); try{v=std::stof(tok);}catch(...){}};
            
            // In v1, this was absolute timeStamp. In v2, it is transitTime.
            g(wp.legacyTimeStamp); 
            
            g(wp.position.x); g(wp.position.y); g(wp.position.z);
            g(wp.target.x);   g(wp.target.y);   g(wp.target.z);
            g(wp.fov);
            
            if (std::getline(ss, tok, ',')) {
                try {
                    size_t pos;
                    wp.holdTime = std::stof(tok, &pos);
                    if (pos != tok.length()) throw std::invalid_argument("Found label");
                    if (std::getline(ss, tok)) wp.label = tok; 
                } catch (...) {
                    wp.holdTime = 1.0f;
                    wp.label = tok;
                    std::string rest;
                    if (std::getline(ss, rest)) wp.label += "," + rest;
                }
            }
            waypoints.push_back(wp);
        }
    }
    
    // AUTO-UPGRADE SYSTEM FOR OLD SAVES
    if (version == 1) {
        float prevDeparture = 0.0f;
        for (auto& wp : waypoints) {
            wp.transitTime = wp.legacyTimeStamp - prevDeparture;
            if (wp.transitTime < 0.0f) wp.transitTime = 0.0f; // Prevent reverse math
            prevDeparture = wp.legacyTimeStamp + wp.holdTime;
        }
        TraceLog(LOG_INFO, "CAMERA TRACK: Auto-Upgraded v1 Save File to Cinematic Sequence!");
    } else {
        // Safe to assign directly in v2
        for (auto& wp : waypoints) {
            wp.transitTime = wp.legacyTimeStamp; 
        }
    }
    
    RefreshDuration();
    TraceLog(LOG_INFO, "CAMERA TRACK: Loaded %d waypoints from %s", (int)waypoints.size(), path.c_str());
}