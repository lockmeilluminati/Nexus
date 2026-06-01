#include "nexus_texture_extractor.h"
#include <direct.h>    
#include <cstdlib>     
#include <fstream>     

std::string IngestAsset(const std::string& sourceFilePath, const std::string& category) {
    // 1. Ensure master category folder exists ("Envs" or "Chars")
    _mkdir(category.c_str());

    std::string fileName = sourceFilePath.substr(sourceFilePath.find_last_of("/\\") + 1);
    std::string rawName = fileName.substr(0, fileName.find_last_of('.'));
    
    // 2. Create the specific asset folder
    std::string localDir = category + "/" + rawName;
    _mkdir(localDir.c_str());

    // 3. SECURE THE GLB: Copy the original file into our new folder
    std::string localGlbPath = localDir + "/" + fileName;
    if (!FileExists(localGlbPath.c_str())) {
        std::ifstream src(sourceFilePath, std::ios::binary);
        std::ofstream dst(localGlbPath, std::ios::binary);
        if (src && dst) dst << src.rdbuf();
    }

    // 4. TRIGGER EXTRACTION: 
    // FIXED: Removed the .py extension from the glbdump target
    std::string command = "cd \"" + localDir + "\" && py \"../../tools/glbdump/glbdump\" -i \"" + fileName + "\"";
    
    int result = std::system(command.c_str());
    
    if (result == 0) {
        TraceLog(LOG_INFO, TextFormat("PIPELINE: Textures ripped and archived in %s", localDir.c_str()));
    } else {
        TraceLog(LOG_ERROR, "PIPELINE: glbdump failed to extract images.");
    }

    // 5. Return the path to the copied .glb 
    return localGlbPath;
}