#pragma once

#include <windows.h>
#include <string>
#include <memory>
#include <d3d9.h>
#include <wrl.h>
#include "AudioPlayer.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "libavutil/time.h"
}

using Microsoft::WRL::ComPtr;

// 缩放模式枚举
enum class ScalingMode {
    FIT_TO_WINDOW,      // 适应窗口（保持宽高比，黑边填充）
    ORIGINAL_SIZE       // 原始尺寸
};

// 滤镜类型枚举
enum class FilterType {
    NONE,           // 无滤镜
    GRAYSCALE,      // 黑白
    MOSAIC          // 马赛克
};

class VideoPlayer {
public:
    VideoPlayer();
    ~VideoPlayer();
    
    // 初始化播放器
    bool Initialize(HWND hwnd, const std::string& videoPath);
    
    // 播放控制
    void Play();
    void Pause();
    void Stop();
    void Seek(double seconds);
      // 获取状态
    bool IsPlaying() const { return m_isPlaying; }
    double GetDuration() const { return m_duration; }
    double GetCurrentTime() const { return m_currentTime; }
    double GetFrameRate() const { return m_frameRate; }
    
    // 音频控制
    void SetVolume(float volume);
    bool HasAudio() const;
    
    // 渲染当前帧
    void Render();
    
    // 窗口尺寸改变时调用
    void OnResize(int width, int height);
    
    // 缩放模式控制
    void SetScalingMode(ScalingMode mode);
    ScalingMode GetScalingMode() const { return m_scalingMode; }
    
    // 滤镜控制
    void SetFilter(FilterType filter);
    FilterType GetCurrentFilter() const { return m_currentFilter; }

private:    // FFmpeg 相关
    AVFormatContext* m_formatContext;
    AVCodecContext* m_codecContext;
    const AVCodec* m_codec;
    AVFrame* m_frame;
    AVFrame* m_frameRGB;
    AVPacket* m_packet;
    struct SwsContext* m_swsContext;
      // 视频信息
    int m_videoStreamIndex;
    double m_duration;
    double m_currentTime;
    double m_frameRate;  // 添加帧率信息
    
    // 播放状态
    bool m_isPlaying;
    bool m_isPaused;
      // Win32 相关
    HWND m_hwnd;
    HDC m_hdcMem;
    HBITMAP m_hBitmap;
    BITMAPINFO m_bitmapInfo;
      // Direct3D 9 相关
    ComPtr<IDirect3D9> m_d3d9;
    ComPtr<IDirect3DDevice9> m_d3d9Device;
    ComPtr<IDirect3DSwapChain9> m_swapChain;
    ComPtr<IDirect3DSurface9> m_d3d9Surface; // 离屏表面用于视频帧
    D3DSURFACE_DESC m_d3d9SurfaceDesc;       // 离屏表面描述
    D3DPRESENT_PARAMETERS m_d3dpp;
    bool m_useD3D9;
    
    // 显示相关
    int m_windowWidth;
    int m_windowHeight;
    int m_videoWidth;
    int m_videoHeight;
    uint8_t* m_buffer;
      // 线程相关
    HANDLE m_playThread;
    HANDLE m_renderEvent;
    bool m_shouldStop;
    
    // 音频播放器
    AudioPlayer m_audioPlayer;
      // 新增成员变量
    ScalingMode m_scalingMode;
    FilterType m_currentFilter;
    int m_mosaicSize;  // 马赛克块大小
    
    // 私有方法
    bool OpenVideo(const std::string& videoPath);
    void CleanupFFmpeg();
    void CleanupGDI();
    void CleanupD3D9();
    bool SetupGDI();
    bool SetupD3D9();
    void RenderWithGDI();
    void RenderWithD3D9();
    void UpdateBitmap();
    void CalculateDisplayRect(int& displayWidth, int& displayHeight, int& offsetX, int& offsetY);    void ApplyFilter(uint8_t* buffer, int width, int height, int bytesPerPixel);
    void ApplyGrayscaleFilter(uint8_t* buffer, int width, int height, int bytesPerPixel);
    void ApplyMosaicFilter(uint8_t* buffer, int width, int height, int bytesPerPixel);
    
    // 静态线程函数
    static DWORD WINAPI PlayThreadProc(LPVOID lpParam);
    void PlayLoop();
};