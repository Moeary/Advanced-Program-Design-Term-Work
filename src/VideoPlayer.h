#pragma once

#include <windows.h>
#include <string>
#include <memory>
#include "AudioPlayer.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "libavutil/time.h"
}

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
    
    // 私有方法
    bool OpenVideo(const std::string& videoPath);
    void CleanupFFmpeg();
    void CleanupGDI();
    bool SetupGDI();
    void UpdateBitmap();
    
    // 静态线程函数
    static DWORD WINAPI PlayThreadProc(LPVOID lpParam);
    void PlayLoop();
};