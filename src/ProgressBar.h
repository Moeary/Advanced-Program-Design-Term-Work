#pragma once

#include <windows.h>
#include <string>

class ProgressBar {
public:
    ProgressBar();
    ~ProgressBar();
    
    bool Initialize(HWND parent, int x, int y, int width, int height);
    
    void SetRange(double minValue, double maxValue);
    void SetPosition(double position);
    double GetPosition() const;
    
    void Show(bool visible);
    void OnPaint(HDC hdc);    void OnMouseMove(int x, int y);
    void OnMouseDown(int x, int y);
    void OnMouseUp(int x, int y);
    
    bool IsVisible() const { return m_isVisible; }
    bool IsDragging() const { return m_isDragging; }
    
    // 获取进度条区域
    RECT GetRect() const { return m_rect; }
    
    // 设置回调函数，当用户拖拽进度条时调用
    void SetSeekCallback(void(*callback)(double position, void* userData), void* userData);
    
    // 新增：自动隐藏功能
    void UpdateAutoHide();
    void CheckMouseHover(int mouseX, int mouseY);
    
    // 新增：时间显示功能
    void SetTimeDisplay(bool enabled);
    bool GetTimeDisplay() const { return m_showTimeDisplay; }
    
private:
    HWND m_hwndParent;
    RECT m_rect;
    
    double m_minValue;
    double m_maxValue;
    double m_currentPosition;
    
    bool m_isVisible;
    bool m_isDragging;
    int m_dragStartX;
    
    // 回调函数
    void(*m_seekCallback)(double position, void* userData);
    void* m_callbackUserData;
      // 颜色
    COLORREF m_backgroundColor;
    COLORREF m_progressColor;
    COLORREF m_handleColor;
      // 新增：自动隐藏相关
    DWORD m_lastMouseMoveTime;
    bool m_autoHideEnabled;
    bool m_isMouseOver;
    
    // 新增：时间显示
    bool m_showTimeDisplay;
    
    // 新增：双缓冲绘制
    HDC m_memDC;
    HBITMAP m_memBitmap;
    HBITMAP m_oldBitmap;
    int m_bufferWidth;
    int m_bufferHeight;
    bool m_bufferValid;
    double m_lastDrawnPosition;
    
    // 私有方法
    double PixelToPosition(int x) const;
    int PositionToPixel(double position) const;
    void DrawProgress(HDC hdc);
    void DrawTimeDisplay(HDC hdc);  // 新增：绘制时间显示
    std::string FormatTime(double seconds) const;  // 新增：格式化时间
    void CreateBuffers();  // 新增：创建双缓冲区
    void DestroyBuffers(); // 新增：销毁双缓冲区
    void InvalidateProgress(); // 新增：智能失效区域
};
