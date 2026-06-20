@echo off
cls
echo [ NEXUS ENGINE COMPILER ]
echo Compiling...

g++ *.cpp -o NexusEngine.exe -O3 -I include -L lib -lraylib -lopengl32 -lgdi32 -lwinmm -lcomdlg32 -pthread

if %errorlevel% neq 0 (
    echo.
    echo [!] COMPILATION FAILED.
    pause
    exit /b %errorlevel%
)

echo [!] BUILD SUCCESSFUL. LAUNCHING ENGINE...
echo.
NexusEngine.exe