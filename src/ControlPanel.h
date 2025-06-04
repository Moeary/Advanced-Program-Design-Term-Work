#pragma once

#define NOMINMAX
#include <windows.h>
#include <commctrl.h>

// Control IDs
#define ID_AUDIO_OFFSET_SLIDER 1001
#define ID_VOLUME_SLIDER       1002
#define ID_MOSAIC_SIZE_SLIDER  1003
#define ID_RESET_BUTTON        1004

// Control types for callback
enum ControlType
{
    CONTROL_AUDIO_OFFSET,
    CONTROL_VOLUME,
    CONTROL_MOSAIC_SIZE
};

class ControlPanel
{
public:
    ControlPanel();
    ~ControlPanel();

    // Initialization
    bool Initialize(HWND parent, HINSTANCE hInstance);

    // Visibility
    void Show(HWND parent);
    void Hide();
    bool IsVisible() const;

    // Audio offset control (-2.0s to +2.0s)
    void SetAudioOffset(double offset);
    double GetAudioOffset() const;    // Volume control (0.0f to 1.0f)
    void SetVolume(float volume);
    float GetVolume() const;

    // Mosaic size control (2 to 32 pixels)
    void SetMosaicSize(int size);
    int GetMosaicSize() const;

    // Callback for value changes
    void SetCallback(void(*callback)(ControlType type, double value, void* userData), void* userData);

    // Message handling
    bool HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

private:    // Window handles
    HWND m_hwndParent;
    HWND m_hwndPanel;
    HWND m_hwndAudioOffsetSlider;
    HWND m_hwndVolumeSlider;
    HWND m_hwndMosaicSizeSlider;
    HWND m_hwndResetButton;
    HWND m_hwndAudioOffsetLabel;
    HWND m_hwndVolumeLabel;
    HWND m_hwndMosaicSizeLabel;
    HWND m_hwndAudioOffsetValue;
    HWND m_hwndVolumeValue;
    HWND m_hwndMosaicSizeValue;

    // Font
    HFONT m_hFont;    // State
    bool m_isVisible;
    bool m_isInitialized;
    double m_audioOffset;   // -2.0 to +2.0 seconds
    float m_volume;         // 0.0 to 1.0
    int m_mosaicSize;       // 2 to 32 pixels

    // Callback
    void(*m_changeCallback)(ControlType type, double value, void* userData);
    void* m_callbackUserData;

    // Internal methods
    void CreateControls();
    void UpdateValueDisplays();
    void OnSliderChange(int controlId);
    void OnResetButton();    // Conversion helpers
    int AudioOffsetToSliderPos(double offset) const;
    double SliderPosToAudioOffset(int pos) const;
    int VolumeToSliderPos(float volume) const;
    float SliderPosToVolume(int pos) const;
    int MosaicSizeToSliderPos(int size) const;
    int SliderPosToMosaicSize(int pos) const;

    // Window procedure
    static LRESULT CALLBACK PanelWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};