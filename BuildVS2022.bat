@echo off
echo FFmpeg Video Player - Visual Studio 2022 Build
SETLOCAL ENABLEDELAYEDEXPANSION

REM 设置路径
SET "PROJECT_DIR=%~dp0"
SET "FFMPEG_DIR=%PROJECT_DIR%ffmpeg-master-latest-win64-gpl-shared"
SET "SRC_DIR=%PROJECT_DIR%src"
SET "BUILD_DIR=%PROJECT_DIR%build"

echo Project Directory: %PROJECT_DIR%
echo FFmpeg Directory: %FFMPEG_DIR%
echo Source Directory: %SRC_DIR%
echo Build Directory: %BUILD_DIR%

REM 检查 FFmpeg 是否存在
if not exist "%FFMPEG_DIR%\include\libavformat\avformat.h" (
    echo Error: FFmpeg headers not found!
    echo Please ensure FFmpeg is properly extracted.
    pause
    exit /b 1
)

REM 创建构建目录
echo Creating build directory...
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

REM 设置 Visual Studio 环境
SET "VS_PATH=D:\Programs\VisualStudio"
SET "VCVARS_PATH=%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat"

if not exist "%VCVARS_PATH%" (
    echo Error: Visual Studio vcvars64.bat not found!
    echo Expected at: %VCVARS_PATH%
    echo Please check Visual Studio installation path.
    pause
    exit /b 1
)

echo Setting up Visual Studio environment...
call "%VCVARS_PATH%"

if %ERRORLEVEL% NEQ 0 (
    echo Error: Failed to setup Visual Studio environment!
    pause
    exit /b 1
)

echo.
echo Compiling with Visual Studio 2022...
cl /EHsc /MD /O2 /W3 /DWIN32 /D_WINDOWS /DNDEBUG ^
    /I"%FFMPEG_DIR%\include" ^
    "%SRC_DIR%\main.cpp" "%SRC_DIR%\VideoPlayer.cpp" "%SRC_DIR%\AudioPlayer.cpp" "%SRC_DIR%\ProgressBar.cpp" "%SRC_DIR%\ControlPanel.cpp" ^
    /Fe:"%BUILD_DIR%\VideoPlayer.exe" ^    /link ^
    /LIBPATH:"%FFMPEG_DIR%\lib" ^
    avformat.lib avcodec.lib avutil.lib swscale.lib swresample.lib ^
    user32.lib gdi32.lib comdlg32.lib kernel32.lib winmm.lib d3d9.lib comctl32.lib

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Compilation failed!
    echo Please check the error messages above.
    pause
    exit /b 1
)

echo.
echo Copying FFmpeg DLLs...
copy "%FFMPEG_DIR%\bin\*.dll" "%BUILD_DIR%\" > nul

if %ERRORLEVEL% NEQ 0 (
    echo Warning: Failed to copy some DLL files
)

echo.
echo ========================================
echo Build successful!
echo ========================================
echo Executable: %BUILD_DIR%\VideoPlayer.exe
echo.
echo To run the video player:
echo   cd build
echo   VideoPlayer.exe
echo.
echo Usage:
echo 1. Use File menu to open a video file
echo 2. Use Playback menu to control playback
echo 3. Keyboard shortcuts:
echo    - Space: Play/Pause
echo    - ESC: Stop
echo    - Left Arrow: Seek back 5 seconds
echo    - Right Arrow: Seek forward 5 seconds
echo    - F6: Toggle Control Panel (Audio Offset and Volume)
echo    - F1: Fit to Window scale mode
echo    - F2: Original Size scale mode
echo    - 1/2/3: Filter modes (None/Grayscale/Mosaic)
echo.
pause
