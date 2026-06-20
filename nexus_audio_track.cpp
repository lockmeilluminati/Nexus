#include "nexus_audio_track.h"
#include <cmath>

void NexusAudioSystem::AddAudioTrack(const std::string& path) {
    TimelineAudioTrack t;
    t.filePath = path;
    
    std::string name = path.substr(path.find_last_of("/\\") + 1);
    t.assetName = name.substr(0, name.find_last_of('.'));
    t.startFrame = 0;
    t.trimStartSecs = 0.0f;

    t.stream = LoadMusicStream(path.c_str());
    if (t.stream.ctxData != nullptr) {
        t.isLoaded = true;
        float lengthSecs = GetMusicTimeLength(t.stream);
        t.durationFrames = (int)(lengthSecs * 60.0f);
        tracks.push_back(t);
        TraceLog(LOG_INFO, "AUDIO: Track added '%s' (%.2fs)", t.assetName.c_str(), lengthSecs);
    } else {
        TraceLog(LOG_ERROR, "AUDIO: Failed to load stream %s", path.c_str());
    }
}

void NexusAudioSystem::UpdateAudioSync(int globalFrame, bool isPlaying) {
    for (auto& track : tracks) {
        if (!track.isLoaded) continue;

        // If the playhead is currently inside the audio clip's visible boundaries
        if (globalFrame >= track.startFrame && globalFrame < track.startFrame + track.durationFrames) {
            
            // THE FIX: Add the trimmed offset so it plays from the shaved point
            float targetTime = ((float)(globalFrame - track.startFrame) / 60.0f) + track.trimStartSecs;
            
            if (isPlaying) {
                if (!IsMusicStreamPlaying(track.stream)) ResumeMusicStream(track.stream);
                if (!IsMusicStreamPlaying(track.stream)) PlayMusicStream(track.stream);

                float current = GetMusicTimePlayed(track.stream);
                // Correct drift if stream gets out of sync with timeline
                if (std::abs(current - targetTime) > 0.15f) { 
                    SeekMusicStream(track.stream, targetTime);
                }
                UpdateMusicStream(track.stream);
            } else {
                if (IsMusicStreamPlaying(track.stream)) PauseMusicStream(track.stream);
                
                // Allow scrubbing while paused
                float current = GetMusicTimePlayed(track.stream);
                if (std::abs(current - targetTime) > 0.15f) {
                    SeekMusicStream(track.stream, targetTime);
                }
            }
        } else {
            // Stop playing if the playhead leaves the clip
            if (IsMusicStreamPlaying(track.stream)) StopMusicStream(track.stream);
        }
    }
}

void NexusAudioSystem::RemoveTrack(int index) {
    if (index >= 0 && index < (int)tracks.size()) {
        if (tracks[index].isLoaded) {
            UnloadMusicStream(tracks[index].stream);
        }
        tracks.erase(tracks.begin() + index);
    }
}

void NexusAudioSystem::UnloadAll() {
    for (auto& t : tracks) {
        if (t.isLoaded) UnloadMusicStream(t.stream);
    }
    tracks.clear();
}