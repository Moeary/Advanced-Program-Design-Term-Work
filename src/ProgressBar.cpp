#include "ProgressBar.h"
#include <windows.h>
#include <algorithm>

using std::min;
using std::max;

ProgressBar::ProgressBar()
    : m_hwndParent(nullptr)
    , m_minValue(0.0)
    , m_maxValue(100.0)
    , m_currentPosition(0.0)
    , m_isVisible(false)
    , m_isDragging(false)
    , m_dragStartX(0)
    , m_seekCallback(nullptr)
    , m_callbackUserData(nullptr)
    , m_backgroundColor(RGB(64, 64, 64))
    , m_progressColor(RGB(0, 120, 215))
    , m_handleColor(RGB(255, 255, 255))
{
    ZeroMemory(&m_rect, sizeof(RECT));
}

ProgressBar::~ProgressBar()
{
}

bool ProgressBar::Initialize(HWND parent, int x, int y, int width, int height)
{
    m_hwndParent = parent;
    m_rect.left = x;
    m_rect.top = y;
    m_rect.right = x + width;
    m_rect.bottom = y + height;
    m_isVisible = true;
    return true;
}

void ProgressBar::SetRange(double minValue, double maxValue)
{
    m_minValue = minValue;
    m_maxValue = maxValue;
    if (m_currentPosition < m_minValue)
        m_currentPosition = m_minValue;
    if (m_currentPosition > m_maxValue)
        m_currentPosition = m_maxValue;
}

void ProgressBar::SetPosition(double position)
{
    m_currentPosition = max(m_minValue, min(m_maxValue, position));
    if (m_hwndParent && m_isVisible)
    {
        InvalidateRect(m_hwndParent, &m_rect, FALSE);
    }
}

double ProgressBar::GetPosition() const
{
    return m_currentPosition;
}

void ProgressBar::Show(bool visible)
{
    m_isVisible = visible;
    if (m_hwndParent)
    {
        InvalidateRect(m_hwndParent, &m_rect, TRUE);
    }
}

void ProgressBar::OnPaint(HDC hdc)
{
    if (!m_isVisible)
        return;
    
    DrawProgress(hdc);
}

void ProgressBar::DrawProgress(HDC hdc)
{
    // 绘制背景
    HBRUSH bgBrush = CreateSolidBrush(m_backgroundColor);
    FillRect(hdc, &m_rect, bgBrush);
    DeleteObject(bgBrush);
    
    // 计算进度位置
    int progressWidth = 0;
    if (m_maxValue > m_minValue)
    {
        double progress = (m_currentPosition - m_minValue) / (m_maxValue - m_minValue);
        progressWidth = (int)((m_rect.right - m_rect.left) * progress);
    }
    
    // 绘制进度条
    if (progressWidth > 0)
    {
        RECT progressRect = m_rect;
        progressRect.right = progressRect.left + progressWidth;
        
        HBRUSH progressBrush = CreateSolidBrush(m_progressColor);
        FillRect(hdc, &progressRect, progressBrush);
        DeleteObject(progressBrush);
    }
    
    // 绘制进度条边框
    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(128, 128, 128));
    HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
    
    MoveToEx(hdc, m_rect.left, m_rect.top, nullptr);
    LineTo(hdc, m_rect.right, m_rect.top);
    LineTo(hdc, m_rect.right, m_rect.bottom);
    LineTo(hdc, m_rect.left, m_rect.bottom);
    LineTo(hdc, m_rect.left, m_rect.top);
    
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);
    
    // 绘制拖拽手柄
    int handleX = m_rect.left + progressWidth;
    int handleY = m_rect.top + (m_rect.bottom - m_rect.top) / 2;
    int handleSize = 8;
    
    HBRUSH handleBrush = CreateSolidBrush(m_handleColor);
    HPEN handlePen = CreatePen(PS_SOLID, 1, RGB(100, 100, 100));
    
    SelectObject(hdc, handleBrush);
    SelectObject(hdc, handlePen);
    
    Ellipse(hdc, handleX - handleSize/2, handleY - handleSize/2,
            handleX + handleSize/2, handleY + handleSize/2);
    
    DeleteObject(handleBrush);
    DeleteObject(handlePen);
}

void ProgressBar::OnMouseMove(int x, int y)
{
    if (!m_isVisible)
        return;
    
    if (m_isDragging)
    {
        double newPosition = PixelToPosition(x);
        SetPosition(newPosition);
    }
}

void ProgressBar::OnMouseDown(int x, int y)
{
    if (!m_isVisible)
        return;
    
    // 检查是否在进度条区域内
    if (x >= m_rect.left && x <= m_rect.right && y >= m_rect.top && y <= m_rect.bottom)
    {
        m_isDragging = true;
        m_dragStartX = x;
        SetCapture(m_hwndParent);
        
        double newPosition = PixelToPosition(x);
        SetPosition(newPosition);
    }
}

void ProgressBar::OnMouseUp(int x, int y)
{
    if (m_isDragging)
    {
        m_isDragging = false;
        ReleaseCapture();
        
        // 调用回调函数
        if (m_seekCallback)
        {
            m_seekCallback(m_currentPosition, m_callbackUserData);
        }
    }
}

void ProgressBar::SetSeekCallback(void(*callback)(double position, void* userData), void* userData)
{
    m_seekCallback = callback;
    m_callbackUserData = userData;
}

double ProgressBar::PixelToPosition(int x) const
{
    if (m_rect.right <= m_rect.left)
        return m_minValue;
    
    double ratio = (double)(x - m_rect.left) / (m_rect.right - m_rect.left);
    ratio = max(0.0, min(1.0, ratio));
    
    return m_minValue + ratio * (m_maxValue - m_minValue);
}

int ProgressBar::PositionToPixel(double position) const
{
    if (m_maxValue <= m_minValue)
        return m_rect.left;
    
    double ratio = (position - m_minValue) / (m_maxValue - m_minValue);
    ratio = max(0.0, min(1.0, ratio));
    
    return m_rect.left + (int)((m_rect.right - m_rect.left) * ratio);
}
