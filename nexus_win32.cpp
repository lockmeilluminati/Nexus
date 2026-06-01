#include "nexus_win32.h"
#include <windows.h>

std::string SaveNexusFileDialog() {
    OPENFILENAMEA ofn;
    CHAR szFile[260] = {0};
    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Nexus3D Project (*.nexus3d)\0*.nexus3d\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    // THE FIX: OFN_NOCHANGEDIR prevents Windows from hijacking the engine's working directory
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = "nexus3d";

    if (GetSaveFileNameA(&ofn) == TRUE) return ofn.lpstrFile;
    return "";
}

std::string OpenNexusFileDialog() {
    OPENFILENAMEA ofn;
    CHAR szFile[260] = {0};
    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Nexus3D Project (*.nexus3d)\0*.nexus3d\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    // THE FIX: OFN_NOCHANGEDIR prevents Windows from hijacking the engine's working directory
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn) == TRUE) return ofn.lpstrFile;
    return "";
}