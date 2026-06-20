@echo off
echo Building Standalone Executable...
g++ game_main.cpp -o MyProject.exe -O2 -Wall -Wno-missing-braces -I ../include -L ../lib -lraylib -lopengl32 -lgdi32 -lwinmm -mwindows
if %ERRORLEVEL% EQU 0 (
    echo Build Success! Executable created: MyProject.exe
) else (
    echo Build Failed.
    pause
)
