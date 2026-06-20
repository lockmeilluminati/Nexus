#include "nexus_publisher.h"
#include "imgui.h"
#include <fstream>
#include <filesystem>
#include <cstdlib> 

namespace fs = std::filesystem;

void NexusPublisher::Draw(bool& isOpen, NexusCameraTrack& camTrack, const std::vector<SceneObject>& scene, const std::vector<Vector3>& sceneRotations, const NexusTimeline& timeline, const NexusPlayer& player, bool useMathController) {
    if (!isOpen) return;
    ImGui::SetNextWindowSize(ImVec2(400, 480), ImGuiCond_FirstUseEver);
    ImGui::Begin("Publish Standalone Game", &isOpen);

    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "Output Settings");
    ImGui::Spacing();

    char nameBuf[128]; strncpy(nameBuf, settings.outputName.c_str(), 127); nameBuf[127] = '\0';
    if (ImGui::InputText("Project Name", nameBuf, 128)) settings.outputName = nameBuf;

    char dirBuf[256]; strncpy(dirBuf, settings.outputDir.c_str(), 255); dirBuf[255] = '\0';
    if (ImGui::InputText("Output Folder", dirBuf, 256)) settings.outputDir = dirBuf;

    ImGui::DragInt("Target FPS", &settings.targetFPS, 1, 24, 120);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f), "Compiler Settings");
    ImGui::Checkbox("Compile Windows .EXE (Standalone)", &settings.compileWindowsExe);
    ImGui::TextDisabled("Generates a runtime and compiles it automatically.");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.15f, 0.6f, 0.25f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f,  0.8f, 0.35f, 1.0f));
    if (ImGui::Button("BUILD AND PUBLISH TO WINDOWS", ImVec2(-1, 36))) {
        Publish(camTrack, scene, sceneRotations, timeline, player, useMathController);
    }
    ImGui::PopStyleColor(2);

    if (buildComplete) {
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Build Successful!");
        ImGui::TextDisabled("Path: %s", lastBuildPath.c_str());
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 0.9f, 1.0f));
        if (ImGui::Button("PLAY STANDALONE BUILD", ImVec2(-1, 40))) {
            std::string runCmd = "cd " + settings.outputDir + " && start " + settings.outputName + ".exe";
            system(runCmd.c_str());
        }
        ImGui::PopStyleColor(2);

        if (ImGui::Button("OPEN BUILD FOLDER", ImVec2(-1, 30))) {
            std::string openCmd = "explorer " + settings.outputDir;
            system(openCmd.c_str());
        }
    }

    ImGui::End();
}

void NexusPublisher::Publish(NexusCameraTrack& camTrack, const std::vector<SceneObject>& scene, const std::vector<Vector3>& sceneRotations, const NexusTimeline& timeline, const NexusPlayer& player, bool useMathController) {
    fs::create_directories(settings.outputDir);
    
    try {
        if (fs::exists("Chars")) fs::copy("Chars", settings.outputDir + "/Chars", fs::copy_options::recursive | fs::copy_options::update_existing);
        if (fs::exists("Envs")) fs::copy("Envs", settings.outputDir + "/Envs", fs::copy_options::recursive | fs::copy_options::update_existing);
        if (fs::exists("Audio")) fs::copy("Audio", settings.outputDir + "/Audio", fs::copy_options::recursive | fs::copy_options::update_existing);
        if (fs::exists("Sounds")) fs::copy("Sounds", settings.outputDir + "/Sounds", fs::copy_options::recursive | fs::copy_options::update_existing);
        if (fs::exists("loadingscreen.png")) fs::copy("loadingscreen.png", settings.outputDir + "/loadingscreen.png", fs::copy_options::update_existing);
    } catch (std::exception& e) {
        TraceLog(LOG_ERROR, "PUBLISHER: Failed to copy asset folders - %s", e.what());
    }

    std::string dataPath = settings.outputDir + "/" + settings.outputName + ".nxpub";
    std::ofstream f(dataPath);
    if (!f.is_open()) return;

    f << "[NexusPublish]\n";
    f << "Name=" << settings.outputName << "\n";
    f << "FPS="  << settings.targetFPS  << "\n";

    if (useMathController) {
        f << "Player=Math\n";
    } else if (player.isPlayMode) {
        f << "Player=ThirdPerson\n";
    }

    for (const auto& gz : timeline.gameZones) {
        float startSec = gz.startFrame / 60.0f;
        float endSec = gz.infiniteTime ? 999999.0f : (gz.endFrame / 60.0f);
        f << "Zone=" << startSec << "," << endSec << "\n";
    }

    for (const auto& trk : timeline.audioManager.tracks) {
        f << "Audio=" << trk.filePath << "," << (trk.startFrame / 60.0f) << "\n";
    }

    for (const auto& txt : timeline.textTracks) {
        float startSec = txt.startFrame / 60.0f;
        float durSec = (txt.endFrame - txt.startFrame) / 60.0f;
        f << "Text=" << startSec << "," << durSec << ","
          << txt.position.x << "," << txt.position.y << ","
          << txt.fontSize << "," << txt.fontSpacing << ","
          << (int)txt.color.r << "," << (int)txt.color.g << "," << (int)txt.color.b << "," << (int)txt.color.a << ","
          << (txt.isBold ? "1" : "0") << "," << (txt.hasShadow ? "1" : "0") << ","
          << (int)txt.effect << "," << txt.effectSpeed << "," << txt.effectIntensity << ","
          << txt.text << "\n";
    }

    for (size_t i = 0; i < scene.size(); i++) {
        const auto& obj = scene[i];
        if (obj.filePath != "PROCEDURAL_MOCAP") { 
            Vector3 pivot = { (obj.bounds.max.x + obj.bounds.min.x) / 2.0f, obj.bounds.min.y, (obj.bounds.max.z + obj.bounds.min.z) / 2.0f };
            
            // THE FIX: We now export the active animation index to the Published Game!
            int animIdx = (i < timeline.tracks.size()) ? timeline.tracks[i].currentAnimIndex : 0;
            
            f << "Asset=" << obj.filePath << "," 
              << obj.position.x << "," << obj.position.y << "," << obj.position.z << "," 
              << obj.scale << ","
              << sceneRotations[i].x << "," << sceneRotations[i].y << "," << sceneRotations[i].z << ","
              << pivot.x << "," << pivot.y << "," << pivot.z << ","
              << obj.walkSpeed << "," << (obj.loopWaypoints ? "1" : "0") << "," << animIdx << "\n";
              
            for (const auto& wp : obj.waypoints) {
                f << "ObjWP=" << wp.x << "," << wp.y << "," << wp.z << "\n";
            }
        }
    }

    if (settings.includeCameraTrack) {
        for (const auto& ct : timeline.cameraTracks) {
            if (ct.waypoints.empty()) continue;
            f << "[CameraTrack]\n";
            f << "StartSec=" << (ct.timelineStartFrame / 60.0f) << "\n";
            f << "PlaybackSpeed=" << ct.rig.playbackSpeed << "\n";
            for (const auto& wp : ct.waypoints) {
                f << "CamWP=" << wp.transitTime << ","
                  << wp.position.x << "," << wp.position.y << "," << wp.position.z << ","
                  << wp.target.x   << "," << wp.target.y   << "," << wp.target.z   << ","
                  << wp.fov << "," << wp.holdTime << "\n";
            }
        }
    }
    f.close();

    // ---------------------------------------------------------
    // THE COMPILER BRIDGE: ADVANCED CINEMATIC RUNTIME
    // ---------------------------------------------------------
    if (settings.compileWindowsExe) {
        std::string cppPath = settings.outputDir + "/game_main.cpp";
        std::ofstream cpp(cppPath);
        if (cpp.is_open()) {
            cpp << "#include \"raylib.h\"\n";
            cpp << "#include \"raymath.h\"\n";
            cpp << "#include <vector>\n";
            cpp << "#include <string>\n";
            cpp << "#include <fstream>\n";
            cpp << "#include <sstream>\n";
            cpp << "#include <cstdarg>\n";
            cpp << "#include <cstdio>\n";
            cpp << "#include <cmath>\n"; 
            cpp << "\n";
            
            // THE FIX: GameObj now stores 'currentAnimIdx'
            cpp << "struct GameObj { Model model; Vector3 pos; Vector3 rot; Vector3 pivot; float scale; bool isAnim; ModelAnimation* anims; int animCount; std::vector<Vector3> waypoints; float walkSpeed; bool loopWP; int currentWP; int currentAnimIdx; };\n";
            
            cpp << "struct CamWP { float transitTime; float holdTime; Vector3 pos; Vector3 target; float fov; };\n";
            cpp << "struct CamTrack { float startSec; float playbackSpeed; std::vector<CamWP> waypoints; };\n";
            cpp << "struct TextFX { std::string text; float start; float duration; float px; float py; float size; float spacing; Color col; bool isBold; bool hasShadow; int effect; float fxSpeed; float fxIntensity; };\n";
            cpp << "struct AudioFX { Sound snd; float start; bool played; };\n";
            cpp << "struct GameZone { float start; float end; };\n";
            cpp << "\n";
            
            cpp << "Vector3 CatmullRom(Vector3 p0, Vector3 p1, Vector3 p2, Vector3 p3, float t) {\n";
            cpp << "    float t2 = t * t; float t3 = t2 * t;\n";
            cpp << "    return {\n";
            cpp << "        0.5f * ((2*p1.x) + (-p0.x+p2.x)*t + (2*p0.x-5*p1.x+4*p2.x-p3.x)*t2 + (-p0.x+3*p1.x-3*p2.x+p3.x)*t3),\n";
            cpp << "        0.5f * ((2*p1.y) + (-p0.y+p2.y)*t + (2*p0.y-5*p1.y+4*p2.y-p3.y)*t2 + (-p0.y+3*p1.y-3*p2.y+p3.y)*t3),\n";
            cpp << "        0.5f * ((2*p1.z) + (-p0.z+p2.z)*t + (2*p0.z-5*p1.z+4*p2.z-p3.z)*t2 + (-p0.z+3*p1.z-3*p2.z+p3.z)*t3)\n";
            cpp << "    };\n";
            cpp << "}\n";
            cpp << "\n";

            cpp << "std::vector<std::string> termLog;\n";
            cpp << "void TVLogCallback(int logLevel, const char *text, va_list args) {\n";
            cpp << "    char buffer[256]; vsnprintf(buffer, sizeof(buffer), text, args);\n";
            cpp << "    std::string logLine = std::string(\"> \") + buffer;\n";
            cpp << "    if (logLine.length() > 50) logLine = logLine.substr(0, 47) + \"...\";\n";
            cpp << "    termLog.push_back(logLine);\n";
            cpp << "    if (termLog.size() > 10) termLog.erase(termLog.begin());\n";
            cpp << "}\n";
            cpp << "\n";

            cpp << "int main() {\n";
            cpp << "    SetTraceLogCallback(TVLogCallback);\n";
            cpp << "    InitWindow(1280, 720, \"" << settings.outputName << "\");\n";
            cpp << "    SetTargetFPS(" << settings.targetFPS << ");\n";
            cpp << "    InitAudioDevice();\n";
            cpp << "    \n";
            
            cpp << "    Texture2D tvSplash = LoadTexture(\"loadingscreen.png\");\n";
            cpp << "    int termX = 976; int termY = 512;\n"; 
            cpp << "    termLog.push_back(\"NEXUS OS v1.4 BOOT SEQUENCE INITIATED...\");\n";
            cpp << "    \n";
            
            cpp << "    Camera3D camera = { { 0, 5, 10 }, { 0, 0, 0 }, { 0, 1, 0 }, 45.0f, 0 };\n";
            cpp << "    \n";
            cpp << "    std::vector<GameObj> objects;\n";
            cpp << "    std::vector<CamTrack> camTracks;\n";
            cpp << "    CamTrack* currentTrack = nullptr;\n";
            cpp << "    std::vector<TextFX> textOverlays;\n";
            cpp << "    std::vector<AudioFX> audioTracks;\n";
            cpp << "    std::vector<GameZone> zones;\n";
            cpp << "    bool hasMathPlayer = false;\n";
            cpp << "    \n";
            
            cpp << "    std::ifstream file(\"" << settings.outputName << ".nxpub\");\n";
            cpp << "    std::string line;\n";
            cpp << "    \n";
            cpp << "    while(std::getline(file, line)) {\n";
            cpp << "        termLog.push_back(\"> \" + line.substr(0, 45));\n";
            cpp << "        if (termLog.size() > 10) termLog.erase(termLog.begin());\n";
            cpp << "        \n";
            cpp << "        BeginDrawing();\n";
            cpp << "        ClearBackground(BLACK);\n";
            cpp << "        if (tvSplash.id != 0) {\n";
            cpp << "            Rectangle src = { 0.0f, 0.0f, (float)tvSplash.width, (float)tvSplash.height };\n";
            cpp << "            Rectangle dst = { 0.0f, 0.0f, 1280.0f, 720.0f };\n";
            cpp << "            DrawTexturePro(tvSplash, src, dst, {0.0f, 0.0f}, 0.0f, WHITE);\n";
            cpp << "        }\n";
            cpp << "        for (size_t i = 0; i < termLog.size(); i++) { DrawText(termLog[i].c_str(), termX, termY + (i * 16), 12, LIME); }\n";
            cpp << "        EndDrawing();\n";
            cpp << "        \n";

            cpp << "        if(line.find(\"Asset=\") == 0) {\n";
            cpp << "            std::stringstream ss(line.substr(6)); std::string path, px, py, pz, s, rx, ry, rz, pvx, pvy, pvz, wspd, lwp, animIdxStr;\n";
            cpp << "            std::getline(ss, path, ','); std::getline(ss, px, ','); std::getline(ss, py, ','); std::getline(ss, pz, ','); std::getline(ss, s, ',');\n";
            cpp << "            std::getline(ss, rx, ','); std::getline(ss, ry, ','); std::getline(ss, rz, ',');\n";
            cpp << "            std::getline(ss, pvx, ','); std::getline(ss, pvy, ','); std::getline(ss, pvz, ',');\n";
            
            // THE FIX: Safely parse the trailing Animation Index from the CSV string!
            cpp << "            std::getline(ss, wspd, ','); std::getline(ss, lwp, ','); std::getline(ss, animIdxStr, ',');\n";
            cpp << "            GameObj o; o.model = LoadModel(path.c_str());\n";
            cpp << "            try { o.pos = { std::stof(px), std::stof(py), std::stof(pz) }; o.scale = std::stof(s);\n";
            cpp << "                  o.rot = { std::stof(rx), std::stof(ry), std::stof(rz) };\n";
            cpp << "                  o.pivot = { std::stof(pvx), std::stof(pvy), std::stof(pvz) };\n";
            cpp << "                  o.walkSpeed = std::stof(wspd); o.loopWP = (lwp==\"1\"); o.currentWP = 0;\n";
            cpp << "                  o.currentAnimIdx = animIdxStr.empty() ? 0 : std::stoi(animIdxStr);\n";
            cpp << "            } catch(...) { o.pos={0,0,0}; o.scale=1.0f; o.rot={0,0,0}; o.pivot={0,0,0}; o.walkSpeed=2.0f; o.loopWP=false; o.currentWP=0; o.currentAnimIdx=0; }\n";
            cpp << "            o.anims = LoadModelAnimations(path.c_str(), &o.animCount); o.isAnim = (o.animCount > 0);\n";
            cpp << "            objects.push_back(o);\n";
            cpp << "        }\n";
            cpp << "        else if(line.find(\"ObjWP=\") == 0 && !objects.empty()) {\n";
            cpp << "            std::stringstream ss(line.substr(6)); std::string wx, wy, wz;\n";
            cpp << "            std::getline(ss, wx, ','); std::getline(ss, wy, ','); std::getline(ss, wz, ',');\n";
            cpp << "            try { objects.back().waypoints.push_back({std::stof(wx), std::stof(wy), std::stof(wz)}); } catch(...) {}\n";
            cpp << "        }\n";
            cpp << "        else if(line.find(\"Player=\") == 0) {\n";
            cpp << "            if (line.find(\"Math\") != std::string::npos) { hasMathPlayer = true; }\n";
            cpp << "        }\n";
            cpp << "        else if(line.find(\"Zone=\") == 0) {\n";
            cpp << "            std::stringstream ss(line.substr(5)); std::string tok; GameZone z;\n";
            cpp << "            try { std::getline(ss, tok, ','); z.start = std::stof(tok); std::getline(ss, tok, ','); z.end = std::stof(tok); zones.push_back(z); } catch(...) {}\n";
            cpp << "        }\n";
            cpp << "        else if(line.find(\"Audio=\") == 0) {\n";
            cpp << "            std::stringstream ss(line.substr(6)); std::string tok;\n";
            cpp << "            AudioFX afx; std::getline(ss, tok, ','); afx.snd = LoadSound(tok.c_str());\n";
            cpp << "            try { std::getline(ss, tok, ','); afx.start = std::stof(tok); afx.played = false; audioTracks.push_back(afx); } catch(...) {}\n";
            cpp << "        }\n";
            cpp << "        else if(line.find(\"Text=\") == 0) {\n";
            cpp << "            std::stringstream ss(line.substr(5)); std::string tok; TextFX txt;\n";
            cpp << "            try {\n";
            cpp << "                auto g = [&](float& v){ std::getline(ss,tok,','); try{v=std::stof(tok);}catch(...){}};\n";
            cpp << "                auto gi = [&](int& v){ std::getline(ss,tok,','); try{v=std::stoi(tok);}catch(...){}};\n";
            cpp << "                g(txt.start); g(txt.duration); g(txt.px); g(txt.py); g(txt.size); g(txt.spacing);\n";
            cpp << "                int r=255,g_v=255,b=255,a=255, bold=0, shadow=0;\n";
            cpp << "                gi(r); gi(g_v); gi(b); gi(a); gi(bold); gi(shadow); gi(txt.effect); g(txt.fxSpeed); g(txt.fxIntensity);\n";
            cpp << "                txt.col = {(unsigned char)r, (unsigned char)g_v, (unsigned char)b, (unsigned char)a};\n";
            cpp << "                txt.isBold = (bold==1); txt.hasShadow = (shadow==1);\n";
            cpp << "                std::getline(ss, txt.text);\n";
            cpp << "                textOverlays.push_back(txt);\n";
            cpp << "            } catch(...) {}\n";
            cpp << "        }\n";
            cpp << "        else if(line.find(\"[CameraTrack]\") == 0) {\n";
            cpp << "            camTracks.push_back(CamTrack());\n";
            cpp << "            currentTrack = &camTracks.back();\n";
            cpp << "        }\n";
            cpp << "        else if(currentTrack != nullptr && line.find(\"StartSec=\") == 0) {\n";
            cpp << "            currentTrack->startSec = std::stof(line.substr(9));\n";
            cpp << "        }\n";
            cpp << "        else if(currentTrack != nullptr && line.find(\"PlaybackSpeed=\") == 0) {\n";
            cpp << "            currentTrack->playbackSpeed = std::stof(line.substr(14));\n";
            cpp << "        }\n";
            cpp << "        else if(currentTrack != nullptr && line.find(\"CamWP=\") == 0) {\n";
            cpp << "            std::stringstream ss(line.substr(6)); std::string tok; CamWP wp;\n";
            cpp << "            try {\n";
            cpp << "                std::getline(ss, tok, ','); wp.transitTime = std::stof(tok); std::getline(ss, tok, ','); wp.pos.x = std::stof(tok);\n";
            cpp << "                std::getline(ss, tok, ','); wp.pos.y = std::stof(tok); std::getline(ss, tok, ','); wp.pos.z = std::stof(tok);\n";
            cpp << "                std::getline(ss, tok, ','); wp.target.x = std::stof(tok); std::getline(ss, tok, ','); wp.target.y = std::stof(tok);\n";
            cpp << "                std::getline(ss, tok, ','); wp.target.z = std::stof(tok); std::getline(ss, tok, ','); wp.fov = std::stof(tok);\n";
            cpp << "                std::getline(ss, tok, ','); wp.holdTime = std::stof(tok);\n";
            cpp << "                currentTrack->waypoints.push_back(wp);\n";
            cpp << "            } catch(...) {}\n";
            cpp << "        }\n";
            cpp << "    }\n";
            
            cpp << "    if (tvSplash.id != 0) UnloadTexture(tvSplash);\n";
            cpp << "    \n";
            cpp << "    float currentTime = 0.0f;\n";
            cpp << "    int animFrame = 0;\n";
            cpp << "    \n";
            
            // =========================================================================
            // MAIN STANDALONE GAME LOOP
            // =========================================================================
            cpp << "    while (!WindowShouldClose()) {\n";
            cpp << "        currentTime += GetFrameTime();\n";
            cpp << "        animFrame++;\n";
            cpp << "        \n";
            
            cpp << "        for (auto& afx : audioTracks) {\n";
            cpp << "            if (!afx.played && currentTime >= afx.start) { PlaySound(afx.snd); afx.played = true; }\n";
            cpp << "        }\n";
            cpp << "        \n";

            cpp << "        bool cameraOverride = false;\n";
            cpp << "        for (auto& ct : camTracks) {\n";
            cpp << "            if (ct.waypoints.empty()) continue;\n";
            cpp << "            float localTime = currentTime - ct.startSec;\n";
            cpp << "            if (localTime >= 0.0f) {\n";
            cpp << "                float currentRealTime = 0.0f;\n";
            cpp << "                bool trackActive = false;\n";
            cpp << "                for (size_t i = 0; i < ct.waypoints.size(); i++) {\n";
            cpp << "                    float transitReal = ct.waypoints[i].transitTime / ct.playbackSpeed;\n";
            cpp << "                    if (localTime < currentRealTime + transitReal) {\n";
            cpp << "                        cameraOverride = true; trackActive = true;\n";
            cpp << "                        if (i == 0) { camera.position = ct.waypoints[0].pos; camera.target = ct.waypoints[0].target; camera.fovy = ct.waypoints[0].fov; break; }\n";
            cpp << "                        float localTransitT = localTime - currentRealTime;\n";
            cpp << "                        float normalizedT = (transitReal > 0.001f) ? (localTransitT / transitReal) : 1.0f;\n";
            cpp << "                        if (normalizedT < 0.0f) normalizedT = 0.0f;\n";
            cpp << "                        if (normalizedT > 1.0f) normalizedT = 1.0f;\n";
            cpp << "                        float easeT = 0.5f * (1.0f - cosf(3.14159265358979323846f * normalizedT));\n";
            cpp << "                        int n = (int)ct.waypoints.size();\n";
            cpp << "                        int i0 = ((int)i - 2 < 0) ? 0 : (int)i - 2;\n";
            cpp << "                        int i1 = (int)i - 1;\n";
            cpp << "                        int i2 = (int)i;\n";
            cpp << "                        int i3 = ((int)i + 1 > n - 1) ? n - 1 : (int)i + 1;\n";
            cpp << "                        camera.position = CatmullRom(ct.waypoints[i0].pos, ct.waypoints[i1].pos, ct.waypoints[i2].pos, ct.waypoints[i3].pos, easeT);\n";
            cpp << "                        camera.target = CatmullRom(ct.waypoints[i0].target, ct.waypoints[i1].target, ct.waypoints[i2].target, ct.waypoints[i3].target, easeT);\n";
            cpp << "                        camera.fovy = ct.waypoints[i1].fov + (ct.waypoints[i2].fov - ct.waypoints[i1].fov) * easeT;\n";
            cpp << "                        break;\n";
            cpp << "                    }\n";
            cpp << "                    currentRealTime += transitReal;\n";
            cpp << "                    if (localTime <= currentRealTime + ct.waypoints[i].holdTime) {\n";
            cpp << "                        cameraOverride = true; trackActive = true;\n";
            cpp << "                        camera.position = ct.waypoints[i].pos; camera.target = ct.waypoints[i].target; camera.fovy = ct.waypoints[i].fov;\n";
            cpp << "                        break;\n";
            cpp << "                    }\n";
            cpp << "                    currentRealTime += ct.waypoints[i].holdTime;\n";
            cpp << "                }\n";
            
            cpp << "                if (!trackActive && !hasMathPlayer) {\n";
            cpp << "                    cameraOverride = true;\n";
            cpp << "                    camera.position = ct.waypoints.back().pos; camera.target = ct.waypoints.back().target; camera.fovy = ct.waypoints.back().fov;\n";
            cpp << "                }\n";
            cpp << "            }\n";
            cpp << "        }\n";
            cpp << "        \n";
            
            cpp << "        bool inZone = zones.empty() ? true : false;\n";
            cpp << "        for (auto& z : zones) { if (currentTime >= z.start && currentTime <= z.end) inZone = true; }\n";
            cpp << "        \n";

            cpp << "        if (hasMathPlayer && inZone && !cameraOverride) {\n";
            cpp << "            if (!IsCursorHidden()) DisableCursor();\n";
            cpp << "            Vector2 mouseDelta = GetMouseDelta();\n";
            cpp << "            static float camPitch = 0.0f;\n";
            cpp << "            static float camYaw = 0.0f;\n";
            cpp << "            static bool camInit = false;\n";
            cpp << "            if (!camInit) {\n";
            cpp << "                Vector3 dir = Vector3Normalize(Vector3Subtract(camera.target, camera.position));\n";
            cpp << "                camPitch = asinf(dir.y);\n";
            cpp << "                camYaw = atan2f(dir.x, dir.z);\n";
            cpp << "                camInit = true;\n";
            cpp << "            }\n";
            cpp << "            \n";
            cpp << "            camYaw -= mouseDelta.x * 0.003f;\n";
            cpp << "            camPitch -= mouseDelta.y * 0.003f;\n";
            cpp << "            if (camPitch > 1.5f) camPitch = 1.5f;\n";
            cpp << "            if (camPitch < -1.5f) camPitch = -1.5f;\n";
            cpp << "            \n";
            cpp << "            Vector3 forward = { cosf(camPitch) * sinf(camYaw), sinf(camPitch), cosf(camPitch) * cosf(camYaw) };\n";
            cpp << "            Vector3 right = { cosf(camYaw), 0.0f, -sinf(camYaw) };\n";
            cpp << "            \n";
            cpp << "            float speed = 8.0f * GetFrameTime();\n";
            cpp << "            Vector3 flatForward = Vector3Normalize({forward.x, 0.0f, forward.z});\n";
            cpp << "            if (IsKeyDown(KEY_W)) camera.position = Vector3Add(camera.position, Vector3Scale(flatForward, speed));\n";
            cpp << "            if (IsKeyDown(KEY_S)) camera.position = Vector3Subtract(camera.position, Vector3Scale(flatForward, speed));\n";
            cpp << "            if (IsKeyDown(KEY_A)) camera.position = Vector3Subtract(camera.position, Vector3Scale(right, speed));\n";
            cpp << "            if (IsKeyDown(KEY_D)) camera.position = Vector3Add(camera.position, Vector3Scale(right, speed));\n";
            cpp << "            \n";
            cpp << "            camera.position.y = 2.0f;\n";
            cpp << "            camera.target = Vector3Add(camera.position, forward);\n";
            cpp << "        } else {\n";
            cpp << "            if (IsCursorHidden()) EnableCursor();\n";
            cpp << "        }\n";
            cpp << "        \n";
            
            cpp << "        BeginDrawing();\n";
            cpp << "        ClearBackground(BLACK);\n";
            cpp << "        BeginMode3D(camera);\n";
            cpp << "        for(auto& o : objects) {\n";
            cpp << "            if (!o.waypoints.empty() && o.currentWP < (int)o.waypoints.size()) {\n";
            cpp << "                Vector3 targetWP = o.waypoints[o.currentWP];\n";
            cpp << "                Vector3 dir = Vector3Subtract(targetWP, o.pos);\n";
            cpp << "                dir.y = 0; float dist = Vector3Length(dir);\n";
            cpp << "                if (dist > 0.1f) {\n";
            cpp << "                    dir = Vector3Normalize(dir);\n";
            cpp << "                    o.pos.x += dir.x * o.walkSpeed * GetFrameTime();\n";
            cpp << "                    o.pos.z += dir.z * o.walkSpeed * GetFrameTime();\n";
            cpp << "                    o.rot.y = atan2f(dir.x, dir.z) * RAD2DEG;\n";
            cpp << "                } else {\n";
            cpp << "                    o.currentWP++;\n";
            cpp << "                    if (o.loopWP && o.currentWP >= (int)o.waypoints.size()) o.currentWP = 0;\n";
            cpp << "                }\n";
            cpp << "            }\n";
            
            // THE FIX: We now explicitly force game_main.cpp to play the saved animation index!
            cpp << "            if (o.isAnim && o.currentAnimIdx < o.animCount) UpdateModelAnimation(o.model, o.anims[o.currentAnimIdx], animFrame % o.anims[o.currentAnimIdx].keyframeCount);\n";
            
            cpp << "            Matrix tPivot = MatrixTranslate(-o.pivot.x, -o.pivot.y, -o.pivot.z);\n";
            cpp << "            Matrix tScale = MatrixScale(o.scale, o.scale, o.scale);\n";
            cpp << "            Matrix tRot = MatrixRotateXYZ({ o.rot.x * DEG2RAD, o.rot.y * DEG2RAD, o.rot.z * DEG2RAD });\n";
            cpp << "            Matrix tTrans = MatrixTranslate(o.pos.x, o.pos.y, o.pos.z);\n";
            cpp << "            o.model.transform = MatrixMultiply(MatrixMultiply(MatrixMultiply(tPivot, tScale), tRot), tTrans);\n";
            cpp << "            DrawModel(o.model, Vector3Zero(), 1.0f, WHITE);\n";
            cpp << "        }\n";
            
            cpp << "        if (hasMathPlayer && inZone && !cameraOverride) {\n";
            cpp << "            Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));\n";
            cpp << "            Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, {0,1,0}));\n";
            cpp << "            float bob = sin(currentTime * 8.0f) * 0.05f;\n";
            cpp << "            Vector3 rightArm = Vector3Add(camera.position, Vector3Scale(forward, 0.8f));\n";
            cpp << "            rightArm = Vector3Add(rightArm, Vector3Scale(right, 0.4f)); rightArm.y -= (0.3f + bob);\n";
            cpp << "            DrawCube(rightArm, 0.2f, 0.2f, 0.8f, ORANGE); DrawCubeWires(rightArm, 0.2f, 0.2f, 0.8f, DARKGRAY);\n";
            cpp << "            Vector3 leftArm = Vector3Add(camera.position, Vector3Scale(forward, 0.8f));\n";
            cpp << "            leftArm = Vector3Add(leftArm, Vector3Scale(right, -0.4f)); leftArm.y -= (0.3f + bob);\n";
            cpp << "            DrawCube(leftArm, 0.2f, 0.2f, 0.8f, ORANGE); DrawCubeWires(leftArm, 0.2f, 0.2f, 0.8f, DARKGRAY);\n";
            cpp << "        }\n";
            cpp << "        EndMode3D();\n";
            
            cpp << "        for (auto& txt : textOverlays) {\n";
            cpp << "            if (currentTime >= txt.start && currentTime <= txt.start + txt.duration) {\n";
            cpp << "                float dt = currentTime;\n";
            cpp << "                int framesElapsed = (int)((currentTime - txt.start) * 60.0f);\n";
            cpp << "                Font f = GetFontDefault();\n";
            cpp << "                auto DrawRichText = [&](const char* str, Vector2 pos) {\n";
            cpp << "                    if (txt.hasShadow) DrawTextEx(f, str, {pos.x+3.f, pos.y+3.f}, txt.size, txt.spacing, {0,0,0,180});\n";
            cpp << "                    if (txt.isBold) {\n";
            cpp << "                        DrawTextEx(f, str, {pos.x+1.f, pos.y}, txt.size, txt.spacing, txt.col);\n";
            cpp << "                        DrawTextEx(f, str, {pos.x, pos.y+1.f}, txt.size, txt.spacing, txt.col);\n";
            cpp << "                    }\n";
            cpp << "                    DrawTextEx(f, str, pos, txt.size, txt.spacing, txt.col);\n";
            cpp << "                };\n";
            cpp << "                if (txt.effect == 0) DrawRichText(txt.text.c_str(), {txt.px, txt.py});\n";
            cpp << "                else if (txt.effect == 3) {\n";
            cpp << "                    int chars = (int)(framesElapsed * (txt.fxSpeed * 0.1f));\n";
            cpp << "                    if (chars > (int)txt.text.length()) chars = (int)txt.text.length();\n";
            cpp << "                    DrawRichText(txt.text.substr(0, chars).c_str(), {txt.px, txt.py});\n";
            cpp << "                } else {\n";
            cpp << "                    Vector2 cursor = {txt.px, txt.py};\n";
            cpp << "                    for (size_t k = 0; k < txt.text.length(); k++) {\n";
            cpp << "                        std::string ch(1, txt.text[k]);\n";
            cpp << "                        Vector2 off = {0,0};\n";
            cpp << "                        if (txt.effect == 1) off.y = sinf(dt * txt.fxSpeed + (k * 0.5f)) * txt.fxIntensity;\n";
            cpp << "                        else if (txt.effect == 2) { off.x = (float)GetRandomValue(-(int)txt.fxIntensity, (int)txt.fxIntensity); off.y = (float)GetRandomValue(-(int)txt.fxIntensity, (int)txt.fxIntensity); }\n";
            cpp << "                        DrawRichText(ch.c_str(), {cursor.x + off.x, cursor.y + off.y});\n";
            cpp << "                        cursor.x += MeasureTextEx(f, ch.c_str(), txt.size, txt.spacing).x + txt.spacing;\n";
            cpp << "                    }\n";
            cpp << "                }\n";
            cpp << "            }\n";
            cpp << "        }\n";
            
            cpp << "        if (hasMathPlayer && inZone && !cameraOverride) DrawCircle(1280/2, 720/2, 4.0f, Fade(WHITE, 0.5f));\n"; 
            
            cpp << "        EndDrawing();\n";
            cpp << "    }\n";
            cpp << "    CloseAudioDevice();\n";
            cpp << "    CloseWindow();\n";
            cpp << "    return 0;\n";
            cpp << "}\n";
            cpp.close();
        }

        std::string batPath = settings.outputDir + "/build_game.bat";
        std::ofstream bat(batPath);
        if (bat.is_open()) {
            bat << "@echo off\n";
            bat << "echo Building Standalone Executable...\n";
            bat << "g++ game_main.cpp -o " << settings.outputName << ".exe -O2 -Wall -Wno-missing-braces -I ../include -L ../lib -lraylib -lopengl32 -lgdi32 -lwinmm -mwindows\n";
            bat << "if %ERRORLEVEL% EQU 0 (\n";
            bat << "    echo Build Success! Executable created: " << settings.outputName << ".exe\n";
            bat << ") else (\n";
            bat << "    echo Build Failed.\n";
            bat << "    pause\n";
            bat << ")\n";
            bat.close();
        }

        TraceLog(LOG_INFO, "PUBLISHER: Compiling Standalone EXE...");
        std::string compileCommand = "cd " + settings.outputDir + " && build_game.bat";
        system(compileCommand.c_str()); 
        
        buildComplete = true;
        
        char absPath[512];
        if (_fullpath(absPath, settings.outputDir.c_str(), 512)) lastBuildPath = absPath;
        else lastBuildPath = settings.outputDir;
    }
}