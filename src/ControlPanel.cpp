#include "ControlPanel.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

// Slider range definitions
const int AUDIO_OFFSET_RANGE = 400;  // -2.0s to +2.0s, precision 0.01s
const int VOLUME_RANGE = 100;        // 0% to 100%

ControlPanel::ControlPanel()
    : m_hwndParent(nullptr)
    , m_hwndPanel(nullptr)
    , m_hwndAudioOffsetSlider(nullptr)
    , m_hwndVolumeSlider(nullptr)
    , m_hwndResetButton(nullptr)
    , m_hwndAudioOffsetLabel(nullptr)
    , m_hwndVolumeLabel(nullptr)
    , m_hwndAudioOffsetValue(nullptr)
    , m_hwndVolumeValue(nullptr)
    , m_hFont(nullptr)
    , m_isVisible(false)
    , m_isInitialized(false)
    , m_audioOffset(0.0)
    , m_volume(1.0f)
    , m_changeCallback(nullptr)
    , m_callbackUserData(nullptr)
{
}

ControlPanel::~ControlPanel()
{
    if (m_hFont)
    {
        DeleteObject(m_hFont);
    }
    if (m_hwndPanel)
    {
        DestroyWindow(m_hwndPanel);
    }
}

bool ControlPanel::Initialize(HWND parent, HINSTANCE hInstance)
{
    if (m_isInitialized)
        return true;
    
    m_hwndParent = parent;
    
    // Initialize common controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);
    
    // Register window class
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = PanelWndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
    wc.lpszClassName = "ControlPanelClass";
    RegisterClassEx(&wc);
    
    // Create control panel window
    m_hwndPanel = CreateWindowEx(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        "ControlPanelClass",
        "Audio Video Control Panel",
        WS_CAPTION | WS_SYSMENU,
        100, 100, 350, 200,
        parent, nullptr, hInstance, this
    );
    
    if (!m_hwndPanel)
        return false;
    
    CreateControls();
    UpdateValueDisplays();
    
    m_isInitialized = true;
    return true;
}

void ControlPanel::CreateControls()
{
    m_hFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");
    
    int yPos = 20;
    int spacing = 50;
    
    // Audio offset control
    m_hwndAudioOffsetLabel = CreateWindow("STATIC", "Audio Offset:",
        WS_VISIBLE | WS_CHILD,
        20, yPos, 100, 20,
        m_hwndPanel, nullptr, GetModuleHandle(nullptr), nullptr);
    SendMessage(m_hwndAudioOffsetLabel, WM_SETFONT, (WPARAM)m_hFont, TRUE);
    
    m_hwndAudioOffsetValue = CreateWindow("STATIC", "0.00s",
        WS_VISIBLE | WS_CHILD | SS_RIGHT,
        250, yPos, 80, 20,
        m_hwndPanel, nullptr, GetModuleHandle(nullptr), nullptr);
    SendMessage(m_hwndAudioOffsetValue, WM_SETFONT, (WPARAM)m_hFont, TRUE);
    
    m_hwndAudioOffsetSlider = CreateWindow(TRACKBAR_CLASS, "",
        WS_VISIBLE | WS_CHILD | TBS_HORZ | TBS_AUTOTICKS,
        20, yPos + 25, 310, 30,
        m_hwndPanel, (HMENU)ID_AUDIO_OFFSET_SLIDER, GetModuleHandle(nullptr), nullptr);
    
    SendMessage(m_hwndAudioOffsetSlider, TBM_SETRANGE, TRUE, MAKELONG(0, AUDIO_OFFSET_RANGE));
    SendMessage(m_hwndAudioOffsetSlider, TBM_SETPOS, TRUE, AUDIO_OFFSET_RANGE / 2); // Middle position = 0 seconds
    SendMessage(m_hwndAudioOffsetSlider, TBM_SETTICFREQ, AUDIO_OFFSET_RANGE / 10, 0);
    
    yPos += spacing;
    
    // Volume control
    m_hwndVolumeLabel = CreateWindow("STATIC", "Volume:",
        WS_VISIBLE | WS_CHILD,
        20, yPos, 80, 20,
        m_hwndPanel, nullptr, GetModuleHandle(nullptr), nullptr);
    SendMessage(m_hwndVolumeLabel, WM_SETFONT, (WPARAM)m_hFont, TRUE);
    
    m_hwndVolumeValue = CreateWindow("STATIC", "100%",
        WS_VISIBLE | WS_CHILD | SS_RIGHT,
        250, yPos, 80, 20,
        m_hwndPanel, nullptr, GetModuleHandle(nullptr), nullptr);
    SendMessage(m_hwndVolumeValue, WM_SETFONT, (WPARAM)m_hFont, TRUE);
    
    m_hwndVolumeSlider = CreateWindow(TRACKBAR_CLASS, "",
        WS_VISIBLE | WS_CHILD | TBS_HORZ | TBS_AUTOTICKS,
        20, yPos + 25, 310, 30,
        m_hwndPanel, (HMENU)ID_VOLUME_SLIDER, GetModuleHandle(nullptr), nullptr);
    
    SendMessage(m_hwndVolumeSlider, TBM_SETRANGE, TRUE, MAKELONG(0, VOLUME_RANGE));
    SendMessage(m_hwndVolumeSlider, TBM_SETPOS, TRUE, VOLUME_RANGE); // 100%
    SendMessage(m_hwndVolumeSlider, TBM_SETTICFREQ, VOLUME_RANGE / 10, 0);
    
    yPos += spacing;
    
    // Reset button
    m_hwndResetButton = CreateWindow("BUTTON", "Reset to Defaults",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        130, yPos, 120, 30,
        m_hwndPanel, (HMENU)ID_RESET_BUTTON, GetModuleHandle(nullptr), nullptr);
    SendMessage(m_hwndResetButton, WM_SETFONT, (WPARAM)m_hFont, TRUE);
}

void ControlPanel::Show(HWND parent)
{
    if (!m_isInitialized)
    {
        Initialize(parent, GetModuleHandle(nullptr));
    }
    
    if (m_hwndPanel)
    {
        ShowWindow(m_hwndPanel, SW_SHOW);
        m_isVisible = true;
    }
}

void ControlPanel::Hide()
{
    if (m_hwndPanel)
    {
        ShowWindow(m_hwndPanel, SW_HIDE);
        m_isVisible = false;
    }
}

bool ControlPanel::IsVisible() const
{
    return m_isVisible;
}

void ControlPanel::SetAudioOffset(double offset)
{
    m_audioOffset = (std::max)(-2.0, (std::min)(2.0, offset));
    if (m_hwndAudioOffsetSlider)
    {
        int pos = AudioOffsetToSliderPos(m_audioOffset);
        SendMessage(m_hwndAudioOffsetSlider, TBM_SETPOS, TRUE, pos);
        UpdateValueDisplays();
    }
}

double ControlPanel::GetAudioOffset() const
{
    return m_audioOffset;
}

void ControlPanel::SetVolume(float volume)
{
    m_volume = (std::max)(0.0f, (std::min)(1.0f, volume));
    if (m_hwndVolumeSlider)
    {
        int pos = VolumeToSliderPos(m_volume);
        SendMessage(m_hwndVolumeSlider, TBM_SETPOS, TRUE, pos);
        UpdateValueDisplays();
    }
}

float ControlPanel::GetVolume() const
{
    return m_volume;
}

void ControlPanel::SetCallback(void(*callback)(ControlType type, double value, void* userData), void* userData)
{
    m_changeCallback = callback;
    m_callbackUserData = userData;
}

bool ControlPanel::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_HSCROLL:
    {
        HWND hwndSlider = (HWND)lParam;
        if (hwndSlider == m_hwndAudioOffsetSlider)
        {
            OnSliderChange(ID_AUDIO_OFFSET_SLIDER);
            return true;
        }
        else if (hwndSlider == m_hwndVolumeSlider)
        {
            OnSliderChange(ID_VOLUME_SLIDER);
            return true;        }
        break;
    }
    case WM_COMMAND:
    {
        if (LOWORD(wParam) == ID_RESET_BUTTON && HIWORD(wParam) == BN_CLICKED)
        {
            OnResetButton();
            return true;
        }
        break;
    }
    }
    return false;
}

void ControlPanel::UpdateValueDisplays()
{
    if (m_hwndAudioOffsetValue)
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << m_audioOffset << "s";
        SetWindowText(m_hwndAudioOffsetValue, oss.str().c_str());
    }
    
    if (m_hwndVolumeValue)
    {
        std::ostringstream oss;
        oss << (int)(m_volume * 100) << "%";
        SetWindowText(m_hwndVolumeValue, oss.str().c_str());
    }
}

void ControlPanel::OnSliderChange(int controlId)
{
    switch (controlId)
    {
    case ID_AUDIO_OFFSET_SLIDER:
    {
        int pos = (int)SendMessage(m_hwndAudioOffsetSlider, TBM_GETPOS, 0, 0);
        m_audioOffset = SliderPosToAudioOffset(pos);
        if (m_changeCallback)
        {
            m_changeCallback(CONTROL_AUDIO_OFFSET, m_audioOffset, m_callbackUserData);
        }
        break;
    }
    case ID_VOLUME_SLIDER:
    {
        int pos = (int)SendMessage(m_hwndVolumeSlider, TBM_GETPOS, 0, 0);
        m_volume = SliderPosToVolume(pos);
        if (m_changeCallback)
        {
            m_changeCallback(CONTROL_VOLUME, (double)(m_volume * 100.0f), m_callbackUserData);
        }
        break;
    }
    }
    
    UpdateValueDisplays();
}

void ControlPanel::OnResetButton()
{
    SetAudioOffset(0.0);
    SetVolume(1.0f);
    
    // Trigger callbacks
    if (m_changeCallback)
    {
        m_changeCallback(CONTROL_AUDIO_OFFSET, 0.0, m_callbackUserData);
        m_changeCallback(CONTROL_VOLUME, 100.0, m_callbackUserData);
    }
}

int ControlPanel::AudioOffsetToSliderPos(double offset) const
{
    // -2.0s to +2.0s mapped to 0 to AUDIO_OFFSET_RANGE
    return (int)((offset + 2.0) * AUDIO_OFFSET_RANGE / 4.0);
}

double ControlPanel::SliderPosToAudioOffset(int pos) const
{
    // 0 to AUDIO_OFFSET_RANGE mapped to -2.0s to +2.0s
    return (double)pos * 4.0 / AUDIO_OFFSET_RANGE - 2.0;
}

int ControlPanel::VolumeToSliderPos(float volume) const
{
    return (int)(volume * VOLUME_RANGE);
}

float ControlPanel::SliderPosToVolume(int pos) const
{
    return (float)pos / VOLUME_RANGE;
}

LRESULT CALLBACK ControlPanel::PanelWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    ControlPanel* pThis = nullptr;
    
    if (uMsg == WM_NCCREATE)
    {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (ControlPanel*)pCreate->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    }
    else
    {
        pThis = (ControlPanel*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }
    
    if (pThis)
    {
        switch (uMsg)
        {
        case WM_HSCROLL:
        case WM_COMMAND:
            if (pThis->HandleMessage(uMsg, wParam, lParam))
                return 0;
            break;
        case WM_CLOSE:
            pThis->Hide();
            return 0;
        }
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}