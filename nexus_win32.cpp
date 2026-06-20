#include "nexus_win32.h"
#include <windows.h>
#include <commdlg.h>

std::string SaveNexusFileDialog()
{
    OPENFILENAMEA ofn;
    CHAR szFile[260] = { 0 };

    ZeroMemory(&ofn, sizeof(OPENFILENAME));

    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);

    ofn.lpstrFilter =
        "Nexus3D Project (*.nexus3d)\0*.nexus3d\0"
        "All Files (*.*)\0*.*\0";

    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST |
                OFN_OVERWRITEPROMPT |
                OFN_NOCHANGEDIR;

    ofn.lpstrDefExt = "nexus3d";

    if (GetSaveFileNameA(&ofn))
        return ofn.lpstrFile;

    return "";
}

std::string OpenNexusFileDialog()
{
    OPENFILENAMEA ofn;
    CHAR szFile[260] = { 0 };

    ZeroMemory(&ofn, sizeof(OPENFILENAME));

    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);

    ofn.lpstrFilter =
        "Nexus3D Project (*.nexus3d)\0*.nexus3d\0"
        "All Files (*.*)\0*.*\0";

    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST |
                OFN_FILEMUSTEXIST |
                OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn))
        return ofn.lpstrFile;

    return "";
}

std::string OpenAudioFileDialog()
{
    OPENFILENAMEA ofn;
    CHAR szFile[MAX_PATH] = { 0 };

    ZeroMemory(&ofn, sizeof(OPENFILENAME));

    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;

    ofn.lpstrFilter =
        "Audio Files\0*.wav;*.ogg;*.mp3;*.flac\0"
        "All Files\0*.*\0";

    ofn.nFilterIndex = 1;
    ofn.Flags =
        OFN_PATHMUSTEXIST |
        OFN_FILEMUSTEXIST |
        OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn))
        return ofn.lpstrFile;

    return "";
}