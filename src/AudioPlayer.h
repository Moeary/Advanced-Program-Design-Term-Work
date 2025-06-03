#pragma once

#include <windows.h>
#include <mmsystem.h>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswresample/swresample.h"
#include "libavutil/opt.h"
#include "libavutil/channel_layout.h"
}

#pragma comment(lib, "winmm.lib")

class AudioPlayer {
public:
    AudioPlayer();
    ~AudioPlayer();
    
    bool Initialize(AVFormatContext* formatContext);
    void Play();
    void Pause();
    void Stop();
    void SetVolume(float volume); // 0.0 - 1.0
    
    bool IsInitialized() const { return m_isInitialized; }
    
private:
    // FFmpeg 音频相关
    AVCodecContext* m_audioCodecContext;
    const AVCodec* m_audioCodec;
    SwrContext* m_swrContext;
    int m_audioStreamIndex;
    
    // Windows Audio 相关
    HWAVEOUT m_hWaveOut;
    WAVEFORMATEX m_waveFormat;
    
    // 音频缓冲区
    static const int BUFFER_COUNT = 4;
    static const int BUFFER_SIZE = 4096;
    WAVEHDR m_waveHeaders[BUFFER_COUNT];
    uint8_t* m_audioBuffers[BUFFER_COUNT];
    int m_currentBuffer;
    
    // 状态
    bool m_isInitialized;
    bool m_isPlaying;
    float m_volume;
    
    // 私有方法
    bool SetupAudioDecoder(AVFormatContext* formatContext);
    void CleanupAudio();
    static void CALLBACK WaveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2);
};
