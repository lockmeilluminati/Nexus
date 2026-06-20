#pragma once

enum class CreatorMode {
    MODE_HUB_SELECT,
    MODE_THIRD_PERSON,
    MODE_FIRST_PERSON
};

class NexusCreatorMenu {
public:
    CreatorMode currentMode = CreatorMode::MODE_HUB_SELECT;
    bool globalClearRequested = false;

    void DrawMainMenu(bool& isOpen);
    void TriggerGlobalReset();
};