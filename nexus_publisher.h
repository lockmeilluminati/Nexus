#pragma once
#include "nexus_camera_track.h"
#include "nexus_core.h"
#include "nexus_timeline.h"
#include "nexus_player.h"
#include <string>
#include <vector>

struct NexusPublishSettings {
    std::string outputName    = "MyProject";
    std::string outputDir     = "Published";
    bool        includeCameraTrack = true;
    bool        exportTimeline     = true;
    bool        exportWaypoints    = true;
    bool        compileWindowsExe  = true; 
    int         targetFPS          = 60;
};

class NexusPublisher {
public:
    NexusPublishSettings settings;
    bool showUI = false;
    bool buildComplete = false;
    std::string lastBuildPath = "";

    // THE FIX: Both functions now demand the sceneRotations array!
    void Draw(bool& isOpen, NexusCameraTrack& camTrack, const std::vector<SceneObject>& scene, const std::vector<Vector3>& sceneRotations, const NexusTimeline& timeline, const NexusPlayer& player, bool useMathController);
    void Publish(NexusCameraTrack& camTrack, const std::vector<SceneObject>& scene, const std::vector<Vector3>& sceneRotations, const NexusTimeline& timeline, const NexusPlayer& player, bool useMathController);
};