#include <windows.h>
#include <commdlg.h>
#include <string>
#include <iostream>
#include "VideoPlayer.h"
#include "ProgressBar.h"

// 窗口类名和标题
const char* g_className = "FFmpegVideoPlayer";
const char* g_windowTitle = "FFmpeg Video Player";

// 全局变量
VideoPlayer* g_player = nullptr;
ProgressBar* g_progressBar = nullptr;
HWND g_hwnd = nullptr;
UINT_PTR g_timerId = 0;

// 菜单ID
#define ID_FILE_OPEN 1001
#define ID_PLAY_PLAY 2001
#define ID_PLAY_PAUSE 2002
#define ID_PLAY_STOP 2003

// 窗口过程函数声明
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// 进度条回调函数
void OnProgressBarSeek(double position, void* userData)
{
    if (g_player)
    {
        g_player->Seek(position);
    }
}

// 更新进度条
void UpdateProgressBar()
{
    if (g_player && g_progressBar && g_player->IsPlaying())
    {
        double currentTime = g_player->GetCurrentTime();
        g_progressBar->SetPosition(currentTime);
    }
}

// 创建菜单
HMENU CreateMenuBar()
{
    HMENU hMenuBar = CreateMenu();
    
    // 文件菜单
    HMENU hFileMenu = CreatePopupMenu();
    AppendMenu(hFileMenu, MF_STRING, ID_FILE_OPEN, "&Open Video...");
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hFileMenu, "&File");
    
    // 播放菜单
    HMENU hPlayMenu = CreatePopupMenu();
    AppendMenu(hPlayMenu, MF_STRING, ID_PLAY_PLAY, "&Play");
    AppendMenu(hPlayMenu, MF_STRING, ID_PLAY_PAUSE, "&Pause");
    AppendMenu(hPlayMenu, MF_STRING, ID_PLAY_STOP, "&Stop");
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hPlayMenu, "&Playback");
    
    return hMenuBar;
}

// 打开文件对话框
std::string OpenFileDialog(HWND hwnd)
{
    OPENFILENAME ofn;
    char szFile[260] = { 0 };
    
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Video Files\0*.mp4;*.avi;*.mkv;*.mov;*.wmv;*.flv\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = "demo_video"; // 默认打开demo_video文件夹
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    
    if (GetOpenFileName(&ofn))
    {
        return std::string(szFile);
    }
    
    return "";
}

// 程序入口点
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // 注册窗口类
    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = g_className;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
    
    if (!RegisterClassEx(&wc))
    {
        MessageBox(nullptr, "Window Registration Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }
    
    // 创建窗口
    g_hwnd = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        g_className,
        g_windowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1024, 768,
        nullptr, nullptr, hInstance, nullptr
    );
    
    if (g_hwnd == nullptr)
    {
        MessageBox(nullptr, "Window Creation Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }
    
    // 设置菜单
    HMENU hMenu = CreateMenuBar();
    SetMenu(g_hwnd, hMenu);    // 创建视频播放器
    g_player = new VideoPlayer();
    
    // 创建进度条
    g_progressBar = new ProgressBar();
    
    // 显示窗口
    ShowWindow(g_hwnd, nCmdShow);
    UpdateWindow(g_hwnd);
    
    // 初始化进度条位置和大小
    RECT clientRect;
    GetClientRect(g_hwnd, &clientRect);
    int progressHeight = 20;
    int margin = 10;
    g_progressBar->Initialize(g_hwnd, margin, clientRect.bottom - progressHeight - margin, 
                            clientRect.right - 2 * margin, progressHeight);
    g_progressBar->Show(true);
    
    // 设置定时器更新进度条
    g_timerId = SetTimer(g_hwnd, 1, 100, nullptr); // 每100ms更新一次
    
    // 消息循环
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }    // 清理
    if (g_timerId)
    {
        KillTimer(g_hwnd, g_timerId);
    }
    delete g_progressBar;
    delete g_player;
    g_progressBar = nullptr;
    g_player = nullptr;
    
    return (int)msg.wParam;
}

// 窗口过程函数
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {        case ID_FILE_OPEN:
        {
            std::string filename = OpenFileDialog(hwnd);
            if (!filename.empty() && g_player)
            {
                g_player->Stop();
                if (g_player->Initialize(hwnd, filename))
                {
                    SetWindowText(hwnd, (g_windowTitle + std::string(" - ") + filename).c_str());
                    
                    // 设置进度条范围
                    if (g_progressBar)
                    {
                        g_progressBar->SetRange(0.0, g_player->GetDuration());
                        g_progressBar->SetSeekCallback(OnProgressBarSeek, nullptr);
                    }
                    
                    // 自动开始播放
                    g_player->Play();
                    
                    std::cout << "Video loaded and playing automatically" << std::endl;
                }
                else
                {
                    MessageBox(hwnd, "Failed to open video file!", "Error", MB_ICONERROR | MB_OK);
                }
            }
            break;
        }
        case ID_PLAY_PLAY:
            if (g_player)
            {
                g_player->Play();
            }
            break;
        case ID_PLAY_PAUSE:
            if (g_player)
            {
                g_player->Pause();
            }
            break;
        case ID_PLAY_STOP:
            if (g_player)
            {
                g_player->Stop();
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            break;
        }
        break;
    }    case WM_SIZE:
    {
        if (g_player)
        {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            g_player->OnResize(width, height);
            
            // 重新定位进度条
            if (g_progressBar)
            {
                int progressHeight = 20;
                int margin = 10;
                g_progressBar->Initialize(hwnd, margin, height - progressHeight - margin, 
                                        width - 2 * margin, progressHeight);
            }
        }
        break;
    }
    case WM_TIMER:
    {
        if (wParam == 1) // 进度条更新定时器
        {
            UpdateProgressBar();
        }
        break;
    }
    case WM_LBUTTONDOWN:
    {
        if (g_progressBar)
        {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            g_progressBar->OnMouseDown(x, y);
        }
        break;
    }
    case WM_LBUTTONUP:
    {
        if (g_progressBar)
        {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            g_progressBar->OnMouseUp(x, y);
        }
        break;
    }
    case WM_MOUSEMOVE:
    {
        if (g_progressBar)
        {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            g_progressBar->OnMouseMove(x, y);
        }
        break;
    }
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
          if (g_player)
        {
            g_player->Render();
        }
        else
        {
            // 绘制默认背景
            RECT rect;
            GetClientRect(hwnd, &rect);
            FillRect(hdc, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH));
            
            // 显示提示文本
            SetTextColor(hdc, RGB(255, 255, 255));
            SetBkMode(hdc, TRANSPARENT);
            DrawText(hdc, "Open a video file to start playing", -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        
        // 绘制进度条
        if (g_progressBar && g_progressBar->IsVisible())
        {
            g_progressBar->OnPaint(hdc);
        }
        
        EndPaint(hwnd, &ps);
        break;
    }
    case WM_KEYDOWN:
    {
        if (g_player)
        {
            switch (wParam)
            {
            case VK_SPACE:
                if (g_player->IsPlaying())
                    g_player->Pause();
                else
                    g_player->Play();
                break;
            case VK_ESCAPE:
                g_player->Stop();
                InvalidateRect(hwnd, nullptr, TRUE);
                break;
            case VK_LEFT:
                // 后退5秒
                {
                    double currentTime = g_player->GetCurrentTime();
                    g_player->Seek(max(0, currentTime - 5.0));
                }
                break;
            case VK_RIGHT:
                // 前进5秒
                {
                    double currentTime = g_player->GetCurrentTime();
                    double duration = g_player->GetDuration();
                    g_player->Seek(min(duration, currentTime + 5.0));
                }
                break;
            }
        }
        break;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}