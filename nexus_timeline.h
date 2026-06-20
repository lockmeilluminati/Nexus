#pragma once
#include <string>
#include <vector>
#include "raylib.h"
#include "nexus_camera_track.h"
#include "nexus_audio_track.h" 
#include "nexus_text_fx.h" 
#include "nexus_timeline_scroll.h" // DELEGATED SCROLLING SCRIPT

struct TimelineTrack {
    std::string assetName    = "";
    int startFrame           = 0;
    int endFrame             = 0;
    int currentAnimIndex     = 0;
    int sourceDuration       = 0;
    int trimStartFrames      = 0; 
    bool isEnvironment       = false;
    bool stayOnScene         = false;
};

struct GameZone {
    std::string label        = "Game Zone";
    int  startFrame          = 0;
    int  endFrame            = 300;
    bool infiniteTime        = false;
    float timeLimitSecs      = 10.0f;
};

class NexusTimeline {
public:
    std::vector<TimelineTrack> tracks;
    std::vector<GameZone>      gameZones;
    bool isPlaying             = false;
    int  maxTimelineFrames     = 1800;

    // --- DELEGATED SCROLL MANAGER ---
    NexusTimelineScroll scroller; 

    std::vector<NexusCameraTrack> cameraTracks;
    bool SyncCameraToTimeline(int globalFrame, Camera3D& editorCam);

    NexusAudioSystem audioManager;
    std::vector<CinematicText> textTracks;

    void AddTrack(const std::string& name, int animFrames, bool isEnv);
    void AddGameZone(int startFrame, int endFrame);
    void AddTextTrack();

    const GameZone* GetActiveGameZone(int globalFrame) const;

    void Draw(int& globalFrame, float browserWidth, int& deleteIndex,
              bool& playModeRequested, float& playTimeLimitOut,
              int& selectedObjectIndex, int& selectedTextIndex);
};