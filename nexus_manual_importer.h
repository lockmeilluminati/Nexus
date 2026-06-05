#ifndef NEXUS_MANUAL_IMPORTER_H
#define NEXUS_MANUAL_IMPORTER_H

#include "raylib.h"
#include <string>
#include <vector>
#include <filesystem>
#include <map>

struct BrowserNode {
    std::string name;
    std::string path;
    bool isDirectory;
    Texture2D thumbnail; 
};

// Tracks how a texture was applied so we can undo safely
struct MappedTexture {
    std::string path;
    bool isAutoMapped;
};

class NexusManualImporter {
public:
    NexusManualImporter();
    ~NexusManualImporter();

    void OpenDialogAndLoad();
    void UpdateViewport(bool isOpen);
    void DrawUI(bool& isOpen);
    
    bool needsRescan; // Tells the main engine to refresh the Asset Browser

private:
    char customNameInput[128]; // Holds the text input for renaming assets

    void RefreshBrowser();
    void ApplyTextureToMaterial(int materialIndex, int mapType, const std::string& texturePath, bool isAuto);
    void SaveNxMat();
    int AutoMapTextures(); 
    void UndoAutoMap(); 
    void ReloadAndApplyManual(); 
    void ExtractTextures(); // Manual Extraction Trigger

    int currentToggleMode; 
    std::string currentAssetPath;
    std::string currentAssetDir;
    std::string currentBrowserDir; 
    std::string rawAssetName;
    
    Model previewModel;
    Camera3D previewCamera;
    RenderTexture2D renderTarget;
    Texture2D emptyTexture; 
    
    std::vector<BrowserNode> browserNodes;
    std::vector<Texture2D> loadedPreviewTextures; 
    
    std::map<int, std::map<int, MappedTexture>> materialAssignments;
    
    // UI & Camera Feedback
    std::string feedbackMessage;
    float feedbackTimer;
    bool isViewportHovered;
    bool isCameraDragging;
    
    // Raycast
    float viewportMouseU;
    float viewportMouseV;
    bool triggerRaycast;
    int targetMaterialID;
    int scrollToMaterialID;
    
    bool isLoaded;
};

#endif