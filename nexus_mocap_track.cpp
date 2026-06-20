#include "nexus_mocap_track.h"
#include <fstream>

void NexusMocapTrack::Evaluate(float localTimeSeconds, MocapFrame& outFrame) const {
    if (frames.empty()) return;
    
    // Calculate the exact frame index based on the timeline's local time
    int exactFrame = trimStartFrame + (int)(localTimeSeconds * fps);
    
    // Ensure we do not read past the end of the trimmed sequence
    int actualEnd = trimEndFrame > 0 ? trimEndFrame : (int)frames.size() - 1;
    if (actualEnd >= (int)frames.size()) actualEnd = (int)frames.size() - 1;

    // Clamp the frame to the valid range
    if (exactFrame < trimStartFrame) exactFrame = trimStartFrame;
    if (exactFrame > actualEnd) exactFrame = actualEnd;

    outFrame = frames[exactFrame];
}

void NexusMocapTrack::SaveToFile(const std::string& path) const {
    std::ofstream file(path);
    
    // THE FIX: Stop failing silently! Tell the developer exactly what broke.
    if (!file.is_open()) {
        TraceLog(LOG_ERROR, "MOCAP SAVE FAILED: Could not create file at [%s]. Ensure the 'Chars/' directory exists in your project folder!", path.c_str());
        return;
    }
    
    // Serialize Metadata
    file << "[NexusMoCap]\n";
    file << "FPS=" << fps << "\n";
    file << "Skin=" << skinID << "\n";
    file << "Pos=" << position.x << "," << position.y << "," << position.z << "\n";
    file << "Rot=" << rotation.x << "," << rotation.y << "," << rotation.z << "\n";
    file << "Scale=" << scale << "\n";
    file << "Snap=" << (snapToInteractiveObject ? "1" : "0") << "\n";
    
    // Serialize custom user C++ code
    file << "[CustomMathCode]\n" << customMathCode << "\n[EndCode]\n";
    
    // Serialize Raw Spatial Data
    file << "[Frames]\n";
    for (const auto& f : frames) {
        for (int i = 0; i < MOCAP_JOINT_COUNT; i++) {
            file << f.joints[i].x << "," << f.joints[i].y << "," << f.joints[i].z;
            if (i < MOCAP_JOINT_COUNT - 1) file << "|";
        }
        file << "\n";
    }
    
    file.close();
    TraceLog(LOG_INFO, "MOCAP TRACK: Saved successfully to %s", path.c_str());
}

void NexusMocapTrack::LoadFromFile(const std::string& path) {
    // Note: Deserialization logic will go here when you are ready 
    // to load raw .nxmocap files back into the Studio for re-trimming!
}