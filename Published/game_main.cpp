#include "raylib.h"
#include "raymath.h"
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <cstdarg>
#include <cstdio>
#include <cmath>

struct GameObj { Model model; Vector3 pos; Vector3 rot; Vector3 pivot; float scale; bool isAnim; ModelAnimation* anims; int animCount; std::vector<Vector3> waypoints; float walkSpeed; bool loopWP; int currentWP; int currentAnimIdx; };
struct CamWP { float transitTime; float holdTime; Vector3 pos; Vector3 target; float fov; };
struct CamTrack { float startSec; float playbackSpeed; std::vector<CamWP> waypoints; };
struct TextFX { std::string text; float start; float duration; float px; float py; float size; float spacing; Color col; bool isBold; bool hasShadow; int effect; float fxSpeed; float fxIntensity; };
struct AudioFX { Sound snd; float start; bool played; };
struct GameZone { float start; float end; };

Vector3 CatmullRom(Vector3 p0, Vector3 p1, Vector3 p2, Vector3 p3, float t) {
    float t2 = t * t; float t3 = t2 * t;
    return {
        0.5f * ((2*p1.x) + (-p0.x+p2.x)*t + (2*p0.x-5*p1.x+4*p2.x-p3.x)*t2 + (-p0.x+3*p1.x-3*p2.x+p3.x)*t3),
        0.5f * ((2*p1.y) + (-p0.y+p2.y)*t + (2*p0.y-5*p1.y+4*p2.y-p3.y)*t2 + (-p0.y+3*p1.y-3*p2.y+p3.y)*t3),
        0.5f * ((2*p1.z) + (-p0.z+p2.z)*t + (2*p0.z-5*p1.z+4*p2.z-p3.z)*t2 + (-p0.z+3*p1.z-3*p2.z+p3.z)*t3)
    };
}

std::vector<std::string> termLog;
void TVLogCallback(int logLevel, const char *text, va_list args) {
    char buffer[256]; vsnprintf(buffer, sizeof(buffer), text, args);
    std::string logLine = std::string("> ") + buffer;
    if (logLine.length() > 50) logLine = logLine.substr(0, 47) + "...";
    termLog.push_back(logLine);
    if (termLog.size() > 10) termLog.erase(termLog.begin());
}

int main() {
    SetTraceLogCallback(TVLogCallback);
    InitWindow(1280, 720, "MyProject");
    SetTargetFPS(60);
    InitAudioDevice();
    
    Texture2D tvSplash = LoadTexture("loadingscreen.png");
    int termX = 976; int termY = 512;
    termLog.push_back("NEXUS OS v1.4 BOOT SEQUENCE INITIATED...");
    
    Camera3D camera = { { 0, 5, 10 }, { 0, 0, 0 }, { 0, 1, 0 }, 45.0f, 0 };
    
    std::vector<GameObj> objects;
    std::vector<CamTrack> camTracks;
    CamTrack* currentTrack = nullptr;
    std::vector<TextFX> textOverlays;
    std::vector<AudioFX> audioTracks;
    std::vector<GameZone> zones;
    bool hasMathPlayer = false;
    
    std::ifstream file("MyProject.nxpub");
    std::string line;
    
    while(std::getline(file, line)) {
        termLog.push_back("> " + line.substr(0, 45));
        if (termLog.size() > 10) termLog.erase(termLog.begin());
        
        BeginDrawing();
        ClearBackground(BLACK);
        if (tvSplash.id != 0) {
            Rectangle src = { 0.0f, 0.0f, (float)tvSplash.width, (float)tvSplash.height };
            Rectangle dst = { 0.0f, 0.0f, 1280.0f, 720.0f };
            DrawTexturePro(tvSplash, src, dst, {0.0f, 0.0f}, 0.0f, WHITE);
        }
        for (size_t i = 0; i < termLog.size(); i++) { DrawText(termLog[i].c_str(), termX, termY + (i * 16), 12, LIME); }
        EndDrawing();
        
        if(line.find("Asset=") == 0) {
            std::stringstream ss(line.substr(6)); std::string path, px, py, pz, s, rx, ry, rz, pvx, pvy, pvz, wspd, lwp, animIdxStr;
            std::getline(ss, path, ','); std::getline(ss, px, ','); std::getline(ss, py, ','); std::getline(ss, pz, ','); std::getline(ss, s, ',');
            std::getline(ss, rx, ','); std::getline(ss, ry, ','); std::getline(ss, rz, ',');
            std::getline(ss, pvx, ','); std::getline(ss, pvy, ','); std::getline(ss, pvz, ',');
            std::getline(ss, wspd, ','); std::getline(ss, lwp, ','); std::getline(ss, animIdxStr, ',');
            GameObj o; o.model = LoadModel(path.c_str());
            try { o.pos = { std::stof(px), std::stof(py), std::stof(pz) }; o.scale = std::stof(s);
                  o.rot = { std::stof(rx), std::stof(ry), std::stof(rz) };
                  o.pivot = { std::stof(pvx), std::stof(pvy), std::stof(pvz) };
                  o.walkSpeed = std::stof(wspd); o.loopWP = (lwp=="1"); o.currentWP = 0;
                  o.currentAnimIdx = animIdxStr.empty() ? 0 : std::stoi(animIdxStr);
            } catch(...) { o.pos={0,0,0}; o.scale=1.0f; o.rot={0,0,0}; o.pivot={0,0,0}; o.walkSpeed=2.0f; o.loopWP=false; o.currentWP=0; o.currentAnimIdx=0; }
            o.anims = LoadModelAnimations(path.c_str(), &o.animCount); o.isAnim = (o.animCount > 0);
            objects.push_back(o);
        }
        else if(line.find("ObjWP=") == 0 && !objects.empty()) {
            std::stringstream ss(line.substr(6)); std::string wx, wy, wz;
            std::getline(ss, wx, ','); std::getline(ss, wy, ','); std::getline(ss, wz, ',');
            try { objects.back().waypoints.push_back({std::stof(wx), std::stof(wy), std::stof(wz)}); } catch(...) {}
        }
        else if(line.find("Player=") == 0) {
            if (line.find("Math") != std::string::npos) { hasMathPlayer = true; }
        }
        else if(line.find("Zone=") == 0) {
            std::stringstream ss(line.substr(5)); std::string tok; GameZone z;
            try { std::getline(ss, tok, ','); z.start = std::stof(tok); std::getline(ss, tok, ','); z.end = std::stof(tok); zones.push_back(z); } catch(...) {}
        }
        else if(line.find("Audio=") == 0) {
            std::stringstream ss(line.substr(6)); std::string tok;
            AudioFX afx; std::getline(ss, tok, ','); afx.snd = LoadSound(tok.c_str());
            try { std::getline(ss, tok, ','); afx.start = std::stof(tok); afx.played = false; audioTracks.push_back(afx); } catch(...) {}
        }
        else if(line.find("Text=") == 0) {
            std::stringstream ss(line.substr(5)); std::string tok; TextFX txt;
            try {
                auto g = [&](float& v){ std::getline(ss,tok,','); try{v=std::stof(tok);}catch(...){}};
                auto gi = [&](int& v){ std::getline(ss,tok,','); try{v=std::stoi(tok);}catch(...){}};
                g(txt.start); g(txt.duration); g(txt.px); g(txt.py); g(txt.size); g(txt.spacing);
                int r=255,g_v=255,b=255,a=255, bold=0, shadow=0;
                gi(r); gi(g_v); gi(b); gi(a); gi(bold); gi(shadow); gi(txt.effect); g(txt.fxSpeed); g(txt.fxIntensity);
                txt.col = {(unsigned char)r, (unsigned char)g_v, (unsigned char)b, (unsigned char)a};
                txt.isBold = (bold==1); txt.hasShadow = (shadow==1);
                std::getline(ss, txt.text);
                textOverlays.push_back(txt);
            } catch(...) {}
        }
        else if(line.find("[CameraTrack]") == 0) {
            camTracks.push_back(CamTrack());
            currentTrack = &camTracks.back();
        }
        else if(currentTrack != nullptr && line.find("StartSec=") == 0) {
            currentTrack->startSec = std::stof(line.substr(9));
        }
        else if(currentTrack != nullptr && line.find("PlaybackSpeed=") == 0) {
            currentTrack->playbackSpeed = std::stof(line.substr(14));
        }
        else if(currentTrack != nullptr && line.find("CamWP=") == 0) {
            std::stringstream ss(line.substr(6)); std::string tok; CamWP wp;
            try {
                std::getline(ss, tok, ','); wp.transitTime = std::stof(tok); std::getline(ss, tok, ','); wp.pos.x = std::stof(tok);
                std::getline(ss, tok, ','); wp.pos.y = std::stof(tok); std::getline(ss, tok, ','); wp.pos.z = std::stof(tok);
                std::getline(ss, tok, ','); wp.target.x = std::stof(tok); std::getline(ss, tok, ','); wp.target.y = std::stof(tok);
                std::getline(ss, tok, ','); wp.target.z = std::stof(tok); std::getline(ss, tok, ','); wp.fov = std::stof(tok);
                std::getline(ss, tok, ','); wp.holdTime = std::stof(tok);
                currentTrack->waypoints.push_back(wp);
            } catch(...) {}
        }
    }
    if (tvSplash.id != 0) UnloadTexture(tvSplash);
    
    float currentTime = 0.0f;
    int animFrame = 0;
    
    while (!WindowShouldClose()) {
        currentTime += GetFrameTime();
        animFrame++;
        
        for (auto& afx : audioTracks) {
            if (!afx.played && currentTime >= afx.start) { PlaySound(afx.snd); afx.played = true; }
        }
        
        bool cameraOverride = false;
        for (auto& ct : camTracks) {
            if (ct.waypoints.empty()) continue;
            float localTime = currentTime - ct.startSec;
            if (localTime >= 0.0f) {
                float currentRealTime = 0.0f;
                bool trackActive = false;
                for (size_t i = 0; i < ct.waypoints.size(); i++) {
                    float transitReal = ct.waypoints[i].transitTime / ct.playbackSpeed;
                    if (localTime < currentRealTime + transitReal) {
                        cameraOverride = true; trackActive = true;
                        if (i == 0) { camera.position = ct.waypoints[0].pos; camera.target = ct.waypoints[0].target; camera.fovy = ct.waypoints[0].fov; break; }
                        float localTransitT = localTime - currentRealTime;
                        float normalizedT = (transitReal > 0.001f) ? (localTransitT / transitReal) : 1.0f;
                        if (normalizedT < 0.0f) normalizedT = 0.0f;
                        if (normalizedT > 1.0f) normalizedT = 1.0f;
                        float easeT = 0.5f * (1.0f - cosf(3.14159265358979323846f * normalizedT));
                        int n = (int)ct.waypoints.size();
                        int i0 = ((int)i - 2 < 0) ? 0 : (int)i - 2;
                        int i1 = (int)i - 1;
                        int i2 = (int)i;
                        int i3 = ((int)i + 1 > n - 1) ? n - 1 : (int)i + 1;
                        camera.position = CatmullRom(ct.waypoints[i0].pos, ct.waypoints[i1].pos, ct.waypoints[i2].pos, ct.waypoints[i3].pos, easeT);
                        camera.target = CatmullRom(ct.waypoints[i0].target, ct.waypoints[i1].target, ct.waypoints[i2].target, ct.waypoints[i3].target, easeT);
                        camera.fovy = ct.waypoints[i1].fov + (ct.waypoints[i2].fov - ct.waypoints[i1].fov) * easeT;
                        break;
                    }
                    currentRealTime += transitReal;
                    if (localTime <= currentRealTime + ct.waypoints[i].holdTime) {
                        cameraOverride = true; trackActive = true;
                        camera.position = ct.waypoints[i].pos; camera.target = ct.waypoints[i].target; camera.fovy = ct.waypoints[i].fov;
                        break;
                    }
                    currentRealTime += ct.waypoints[i].holdTime;
                }
                if (!trackActive && !hasMathPlayer) {
                    cameraOverride = true;
                    camera.position = ct.waypoints.back().pos; camera.target = ct.waypoints.back().target; camera.fovy = ct.waypoints.back().fov;
                }
            }
        }
        
        bool inZone = zones.empty() ? true : false;
        for (auto& z : zones) { if (currentTime >= z.start && currentTime <= z.end) inZone = true; }
        
        if (hasMathPlayer && inZone && !cameraOverride) {
            if (!IsCursorHidden()) DisableCursor();
            Vector2 mouseDelta = GetMouseDelta();
            static float camPitch = 0.0f;
            static float camYaw = 0.0f;
            static bool camInit = false;
            if (!camInit) {
                Vector3 dir = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
                camPitch = asinf(dir.y);
                camYaw = atan2f(dir.x, dir.z);
                camInit = true;
            }
            
            camYaw -= mouseDelta.x * 0.003f;
            camPitch -= mouseDelta.y * 0.003f;
            if (camPitch > 1.5f) camPitch = 1.5f;
            if (camPitch < -1.5f) camPitch = -1.5f;
            
            Vector3 forward = { cosf(camPitch) * sinf(camYaw), sinf(camPitch), cosf(camPitch) * cosf(camYaw) };
            Vector3 right = { cosf(camYaw), 0.0f, -sinf(camYaw) };
            
            float speed = 8.0f * GetFrameTime();
            Vector3 flatForward = Vector3Normalize({forward.x, 0.0f, forward.z});
            if (IsKeyDown(KEY_W)) camera.position = Vector3Add(camera.position, Vector3Scale(flatForward, speed));
            if (IsKeyDown(KEY_S)) camera.position = Vector3Subtract(camera.position, Vector3Scale(flatForward, speed));
            if (IsKeyDown(KEY_A)) camera.position = Vector3Subtract(camera.position, Vector3Scale(right, speed));
            if (IsKeyDown(KEY_D)) camera.position = Vector3Add(camera.position, Vector3Scale(right, speed));
            
            camera.position.y = 2.0f;
            camera.target = Vector3Add(camera.position, forward);
        } else {
            if (IsCursorHidden()) EnableCursor();
        }
        
        BeginDrawing();
        ClearBackground(BLACK);
        BeginMode3D(camera);
        for(auto& o : objects) {
            if (!o.waypoints.empty() && o.currentWP < (int)o.waypoints.size()) {
                Vector3 targetWP = o.waypoints[o.currentWP];
                Vector3 dir = Vector3Subtract(targetWP, o.pos);
                dir.y = 0; float dist = Vector3Length(dir);
                if (dist > 0.1f) {
                    dir = Vector3Normalize(dir);
                    o.pos.x += dir.x * o.walkSpeed * GetFrameTime();
                    o.pos.z += dir.z * o.walkSpeed * GetFrameTime();
                    o.rot.y = atan2f(dir.x, dir.z) * RAD2DEG;
                } else {
                    o.currentWP++;
                    if (o.loopWP && o.currentWP >= (int)o.waypoints.size()) o.currentWP = 0;
                }
            }
            if (o.isAnim && o.currentAnimIdx < o.animCount) UpdateModelAnimation(o.model, o.anims[o.currentAnimIdx], animFrame % o.anims[o.currentAnimIdx].keyframeCount);
            Matrix tPivot = MatrixTranslate(-o.pivot.x, -o.pivot.y, -o.pivot.z);
            Matrix tScale = MatrixScale(o.scale, o.scale, o.scale);
            Matrix tRot = MatrixRotateXYZ({ o.rot.x * DEG2RAD, o.rot.y * DEG2RAD, o.rot.z * DEG2RAD });
            Matrix tTrans = MatrixTranslate(o.pos.x, o.pos.y, o.pos.z);
            o.model.transform = MatrixMultiply(MatrixMultiply(MatrixMultiply(tPivot, tScale), tRot), tTrans);
            DrawModel(o.model, Vector3Zero(), 1.0f, WHITE);
        }
        if (hasMathPlayer && inZone && !cameraOverride) {
            Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
            Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, {0,1,0}));
            float bob = sin(currentTime * 8.0f) * 0.05f;
            Vector3 rightArm = Vector3Add(camera.position, Vector3Scale(forward, 0.8f));
            rightArm = Vector3Add(rightArm, Vector3Scale(right, 0.4f)); rightArm.y -= (0.3f + bob);
            DrawCube(rightArm, 0.2f, 0.2f, 0.8f, ORANGE); DrawCubeWires(rightArm, 0.2f, 0.2f, 0.8f, DARKGRAY);
            Vector3 leftArm = Vector3Add(camera.position, Vector3Scale(forward, 0.8f));
            leftArm = Vector3Add(leftArm, Vector3Scale(right, -0.4f)); leftArm.y -= (0.3f + bob);
            DrawCube(leftArm, 0.2f, 0.2f, 0.8f, ORANGE); DrawCubeWires(leftArm, 0.2f, 0.2f, 0.8f, DARKGRAY);
        }
        EndMode3D();
        for (auto& txt : textOverlays) {
            if (currentTime >= txt.start && currentTime <= txt.start + txt.duration) {
                float dt = currentTime;
                int framesElapsed = (int)((currentTime - txt.start) * 60.0f);
                Font f = GetFontDefault();
                auto DrawRichText = [&](const char* str, Vector2 pos) {
                    if (txt.hasShadow) DrawTextEx(f, str, {pos.x+3.f, pos.y+3.f}, txt.size, txt.spacing, {0,0,0,180});
                    if (txt.isBold) {
                        DrawTextEx(f, str, {pos.x+1.f, pos.y}, txt.size, txt.spacing, txt.col);
                        DrawTextEx(f, str, {pos.x, pos.y+1.f}, txt.size, txt.spacing, txt.col);
                    }
                    DrawTextEx(f, str, pos, txt.size, txt.spacing, txt.col);
                };
                if (txt.effect == 0) DrawRichText(txt.text.c_str(), {txt.px, txt.py});
                else if (txt.effect == 3) {
                    int chars = (int)(framesElapsed * (txt.fxSpeed * 0.1f));
                    if (chars > (int)txt.text.length()) chars = (int)txt.text.length();
                    DrawRichText(txt.text.substr(0, chars).c_str(), {txt.px, txt.py});
                } else {
                    Vector2 cursor = {txt.px, txt.py};
                    for (size_t k = 0; k < txt.text.length(); k++) {
                        std::string ch(1, txt.text[k]);
                        Vector2 off = {0,0};
                        if (txt.effect == 1) off.y = sinf(dt * txt.fxSpeed + (k * 0.5f)) * txt.fxIntensity;
                        else if (txt.effect == 2) { off.x = (float)GetRandomValue(-(int)txt.fxIntensity, (int)txt.fxIntensity); off.y = (float)GetRandomValue(-(int)txt.fxIntensity, (int)txt.fxIntensity); }
                        DrawRichText(ch.c_str(), {cursor.x + off.x, cursor.y + off.y});
                        cursor.x += MeasureTextEx(f, ch.c_str(), txt.size, txt.spacing).x + txt.spacing;
                    }
                }
            }
        }
        if (hasMathPlayer && inZone && !cameraOverride) DrawCircle(1280/2, 720/2, 4.0f, Fade(WHITE, 0.5f));
        EndDrawing();
    }
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
