#include "ProgressBar.h"
#include <windows.h>
#include <algorithm>
#include <string>
#include <sstream>
#include <iomanip>

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
    , m_lastMouseMoveTime(0)
    , m_autoHideEnabled(true)
    , m_isMouseOver(false)
    , m_showTimeDisplay(true)
    , m_memDC(nullptr)
    , m_memBitmap(nullptr)
    , m_oldBitmap(nullptr)
    , m_bufferWidth(0)
    , m_bufferHeight(0)
    , m_bufferValid(false)
    , m_lastDrawnPosition(-1.0)
{
    ZeroMemory(&m_rect, sizeof(RECT));
}

ProgressBar::~ProgressBar()
{
    DestroyBuffers();
}

bool ProgressBar::Initialize(HWND parent, int x, int y, int width, int height)
{
    m_hwndParent = parent;
    m_rect.left = x;
    m_rect.top = y;
    m_rect.right = x + width;
    m_rect.bottom = y + height;
    m_isVisible = true;
    
    // 重新创建缓冲区
    DestroyBuffers();
    CreateBuffers();
    
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
    double newPosition = max(m_minValue, min(m_maxValue, position));
    // 只有位置真正改变时才重绘，并且使用智能失效
    if (abs(newPosition - m_currentPosition) > 0.01)
    {
        m_currentPosition = newPosition;
        m_bufferValid = false; // 标记缓冲区需要更新
        if (m_hwndParent && m_isVisible)
        {
            InvalidateProgress(); // 使用智能失效区域
        }
    }
    else
    {
        m_currentPosition = newPosition;
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
    // 检查自动隐藏
    if (m_autoHideEnabled && !m_isDragging && !m_isMouseOver)
    {
        DWORD currentTime = GetTickCount();
        if (m_lastMouseMoveTime > 0 && (currentTime - m_lastMouseMoveTime) > 5000) // 5秒后隐藏
        {
            m_isVisible = false;
            return;
        }
    }
    
    if (!m_isVisible)
        return;
    
    // 确保缓冲区存在
    if (!m_memDC)
        CreateBuffers();
    
    // 如果缓冲区无效或位置改变，重新绘制到内存DC
    if (!m_bufferValid || abs(m_currentPosition - m_lastDrawnPosition) > 0.001)
    {
        if (m_memDC)
        {
            // 在内存DC中绘制进度条
            DrawProgress(m_memDC);
            
            // 如果显示时间，也在内存DC中绘制时间
            if (m_showTimeDisplay)
            {
                DrawTimeDisplay(m_memDC);
            }
            
            m_bufferValid = true;
            m_lastDrawnPosition = m_currentPosition;
        }
    }
    
    // 将内存DC内容拷贝到屏幕DC
    if (m_memDC)
    {
        // 拷贝进度条部分
        BitBlt(hdc, m_rect.left, m_rect.top, 
               m_rect.right - m_rect.left, m_rect.bottom - m_rect.top,
               m_memDC, 0, 0, SRCCOPY);
        
        // 如果显示时间，拷贝时间显示部分
        if (m_showTimeDisplay)
        {
            int timeX = m_rect.right + 10;
            int timeWidth = 150;
            BitBlt(hdc, timeX, m_rect.top, 
                   timeWidth, m_rect.bottom - m_rect.top,
                   m_memDC, m_rect.right - m_rect.left + 10, 0, SRCCOPY);
        }
    }
}

void ProgressBar::DrawProgress(HDC hdc)
{
    // 计算在内存DC中的绘制区域
    RECT drawRect;
    drawRect.left = 0;
    drawRect.top = 0;
    drawRect.right = m_rect.right - m_rect.left;
    drawRect.bottom = m_rect.bottom - m_rect.top;
    
    // 绘制背景
    HBRUSH bgBrush = CreateSolidBrush(m_backgroundColor);
    FillRect(hdc, &drawRect, bgBrush);
    DeleteObject(bgBrush);
    
    // 计算进度位置
    int progressWidth = 0;
    if (m_maxValue > m_minValue)
    {
        double progress = (m_currentPosition - m_minValue) / (m_maxValue - m_minValue);
        progressWidth = (int)((drawRect.right - drawRect.left) * progress);
    }
    
    // 绘制进度条
    if (progressWidth > 0)
    {
        RECT progressRect = drawRect;
        progressRect.right = progressRect.left + progressWidth;
        
        HBRUSH progressBrush = CreateSolidBrush(m_progressColor);
        FillRect(hdc, &progressRect, progressBrush);
        DeleteObject(progressBrush);
    }
    
    // 绘制进度条边框
    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(128, 128, 128));
    HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
    
    MoveToEx(hdc, drawRect.left, drawRect.top, nullptr);
    LineTo(hdc, drawRect.right, drawRect.top);
    LineTo(hdc, drawRect.right, drawRect.bottom);
    LineTo(hdc, drawRect.left, drawRect.bottom);
    LineTo(hdc, drawRect.left, drawRect.top);
    
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);
    
    // 绘制拖拽手柄
    int handleX = drawRect.left + progressWidth;
    int handleY = drawRect.top + (drawRect.bottom - drawRect.top) / 2;
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
    // 更新自动隐藏相关状态
    m_lastMouseMoveTime = GetTickCount();
    CheckMouseHover(x, y);
    
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
    // 更新自动隐藏相关状态
    CheckMouseHover(x, y);
    
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
        
        // 立即触发跳转，提高响应性
        if (m_seekCallback)
        {
            m_seekCallback(newPosition, m_callbackUserData);
        }
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

// 新增方法实现
void ProgressBar::UpdateAutoHide()
{
    if (m_autoHideEnabled && !m_isDragging && !m_isMouseOver)
    {
        DWORD currentTime = GetTickCount();
        if (m_lastMouseMoveTime > 0 && (currentTime - m_lastMouseMoveTime) > 5000)
        {
            if (m_isVisible)
            {
                m_isVisible = false;
                if (m_hwndParent)
                {
                    InvalidateRect(m_hwndParent, &m_rect, TRUE);
                }
            }
        }
    }
}

void ProgressBar::CheckMouseHover(int mouseX, int mouseY)
{
    bool wasMouseOver = m_isMouseOver;
    m_isMouseOver = (mouseX >= m_rect.left && mouseX <= m_rect.right && 
                     mouseY >= m_rect.top && mouseY <= m_rect.bottom);
    
    if (m_isMouseOver && !wasMouseOver)
    {
        // 鼠标进入进度条区域，显示进度条
        if (!m_isVisible)
        {
            m_isVisible = true;
            if (m_hwndParent)
            {
                InvalidateRect(m_hwndParent, &m_rect, FALSE);
            }
        }
    }
}

void ProgressBar::SetTimeDisplay(bool enabled)
{
    m_showTimeDisplay = enabled;
    if (m_hwndParent && m_isVisible)
    {
        InvalidateRect(m_hwndParent, &m_rect, FALSE);
    }
}

void ProgressBar::DrawTimeDisplay(HDC hdc)
{
    if (!m_showTimeDisplay || m_maxValue <= m_minValue)
        return;
    
    // 格式化当前时间和总时间
    std::string currentTimeStr = FormatTime(m_currentPosition);
    std::string totalTimeStr = FormatTime(m_maxValue);
    std::string timeDisplay = currentTimeStr + " / " + totalTimeStr;
    
    // 设置文本颜色和背景
    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkMode(hdc, TRANSPARENT);
    
    // 选择字体
    HFONT hFont = CreateFont(
        14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial"
    );
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    
    // 计算文本位置（在内存DC中的位置）
    RECT textRect;
    textRect.left = (m_rect.right - m_rect.left) + 10; // 在内存DC中的相对位置
    textRect.top = 0;
    textRect.right = textRect.left + 150;
    textRect.bottom = m_rect.bottom - m_rect.top;
    
    // 清除时间显示区域的背景
    HBRUSH bgBrush = CreateSolidBrush(RGB(0, 0, 0)); // 黑色背景
    FillRect(hdc, &textRect, bgBrush);
    DeleteObject(bgBrush);
    
    // 绘制文本
    DrawTextA(hdc, timeDisplay.c_str(), -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    
    // 恢复字体
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
}

std::string ProgressBar::FormatTime(double seconds) const
{
    int totalSeconds = (int)seconds;
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int secs = totalSeconds % 60;
    
    std::ostringstream oss;
    if (hours > 0)
    {
        oss << std::setfill('0') << std::setw(2) << hours << ":"
            << std::setfill('0') << std::setw(2) << minutes << ":"
            << std::setfill('0') << std::setw(2) << secs;
    }
    else
    {
        oss << std::setfill('0') << std::setw(2) << minutes << ":"
            << std::setfill('0') << std::setw(2) << secs;
    }
      return oss.str();
}

// 新增：双缓冲绘制方法
void ProgressBar::CreateBuffers()
{
    if (!m_hwndParent)
        return;
    
    HDC hdc = GetDC(m_hwndParent);
    if (!hdc)
        return;
    
    int width = m_rect.right - m_rect.left;
    int height = m_rect.bottom - m_rect.top;
    
    // 为时间显示预留额外空间
    if (m_showTimeDisplay)
        width += 160; // 进度条宽度 + 时间显示宽度
    
    if (width > 0 && height > 0)
    {
        m_memDC = CreateCompatibleDC(hdc);
        if (m_memDC)
        {
            m_memBitmap = CreateCompatibleBitmap(hdc, width, height);
            if (m_memBitmap)
            {
                m_oldBitmap = (HBITMAP)SelectObject(m_memDC, m_memBitmap);
                m_bufferWidth = width;
                m_bufferHeight = height;
                m_bufferValid = false;
            }
            else
            {
                DeleteDC(m_memDC);
                m_memDC = nullptr;
            }
        }
    }
    
    ReleaseDC(m_hwndParent, hdc);
}

void ProgressBar::DestroyBuffers()
{
    if (m_memDC)
    {
        if (m_oldBitmap)
        {
            SelectObject(m_memDC, m_oldBitmap);
            m_oldBitmap = nullptr;
        }
        if (m_memBitmap)
        {
            DeleteObject(m_memBitmap);
            m_memBitmap = nullptr;
        }
        DeleteDC(m_memDC);
        m_memDC = nullptr;
    }
    m_bufferValid = false;
    m_bufferWidth = 0;
    m_bufferHeight = 0;
}

void ProgressBar::InvalidateProgress()
{
    if (!m_hwndParent)
        return;
    
    // 只失效需要更新的区域，而不是整个进度条
    int currentPixel = PositionToPixel(m_currentPosition);
    int lastPixel = PositionToPixel(m_lastDrawnPosition);
    
    if (m_lastDrawnPosition < 0)
    {
        // 第一次绘制，失效整个区域
        RECT invalidRect = m_rect;
        if (m_showTimeDisplay)
        {
            invalidRect.right += 160; // 包含时间显示区域
        }
        InvalidateRect(m_hwndParent, &invalidRect, FALSE);
    }
    else
    {
        // 只失效变化的区域
        RECT invalidRect;
        invalidRect.left = m_rect.left + min(currentPixel, lastPixel) - 10; // 留点余量
        invalidRect.right = m_rect.left + max(currentPixel, lastPixel) + 10;
        invalidRect.top = m_rect.top;
        invalidRect.bottom = m_rect.bottom;
        
        // 确保不超出边界
        invalidRect.left = max(invalidRect.left, m_rect.left);
        invalidRect.right = min(invalidRect.right, m_rect.right);
        
        if (m_showTimeDisplay)
        {
            // 同时失效时间显示区域
            RECT timeRect;
            timeRect.left = m_rect.right + 10;
            timeRect.top = m_rect.top;
            timeRect.right = timeRect.left + 150;
            timeRect.bottom = m_rect.bottom;
            
            InvalidateRect(m_hwndParent, &invalidRect, FALSE);
            InvalidateRect(m_hwndParent, &timeRect, FALSE);
        }
        else
        {
            InvalidateRect(m_hwndParent, &invalidRect, FALSE);
        }
    }
}
