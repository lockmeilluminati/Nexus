@echo off
echo [1/2] Compiling ImGui Library (Third-Party, Warnings Ignored)...
g++ -c rlImGui.cpp imgui.cpp imgui_draw.cpp imgui_tables.cpp imgui_widgets.cpp imgui_impl_raylib.cpp -O2 -std=c++17 -I include

echo [2/2] Compiling Nexus 3D Engine (Strict Mode)...
g++ main.cpp file_dialog.cpp nexus_3dviewportcontrols.cpp nexus_environment.cpp nexus_character.cpp nexus_texture_extractor.cpp nexus_timeline.cpp nexus_project.cpp rlImGui.o imgui.o imgui_draw.o imgui_tables.o imgui_widgets.o imgui_impl_raylib.o -o nexus3d.exe -O2 -Wall -std=c++17 -I include -L lib -lraylib -lopengl32 -lgdi32 -lwinmm -lcomdlg32

echo Build Complete!