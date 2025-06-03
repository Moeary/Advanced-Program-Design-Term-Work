#pragma once

#include <windows.h>

class ProgressBar {
public:
    ProgressBar();
    ~ProgressBar();
    
    bool Initialize(HWND parent, int x, int y, int width, int height);
    
    void SetRange(double minValue, double maxValue);
    void SetPosition(double position);
    double GetPosition() const;
    
    void Show(bool visible);
    void OnPaint(HDC hdc);
    void OnMouseMove(int x, int y);
    void OnMouseDown(int x, int y);
    void OnMouseUp(int x, int y);
    
    bool IsVisible() const { return m_isVisible; }
    bool IsDragging() const { return m_isDragging; }
    
    // 获取进度条区域
    RECT GetRect() const { return m_rect; }
    
    // 设置回调函数，当用户拖拽进度条时调用
    void SetSeekCallback(void(*callback)(double position, void* userData), void* userData);
    
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
    
    // 私有方法
    double PixelToPosition(int x) const;
    int PositionToPixel(double position) const;
    void DrawProgress(HDC hdc);
};
