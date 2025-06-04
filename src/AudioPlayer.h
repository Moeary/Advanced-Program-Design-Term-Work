#pragma once

#define NOMINMAX
#include <windows.h>
#include <atlcomcli.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <audiopolicy.h>
#include <memory>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswresample/swresample.h"
#include "libavutil/opt.h"
#include "libavutil/channel_layout.h"
}

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

class AudioPlayer {
public:
    AudioPlayer(WORD nChannels = 2, DWORD nSamplesPerSec = 44100);
    ~AudioPlayer();
    
    bool Initialize(AVFormatContext* formatContext);
    HRESULT Start();
    HRESULT Stop();    void Pause();
    void SetVolume(float volume); // 0.0 - 1.0
    
    // 音频偏移控制
    void SetAudioOffset(double offset);
    double GetAudioOffset() const;
    
    bool IsInitialized() const { return m_isInitialized; }
    
    // WASAPI缓冲区操作
    BYTE* GetBuffer(UINT32 wantFrames);
    HRESULT ReleaseBuffer(UINT32 writtenFrames);
      // FLTP格式音频写入 - 左右声道分开处理
    HRESULT WriteFLTP(float* left, float* right, UINT32 sampleCount);
    
    // 新增：带音视频同步的音频帧处理
    HRESULT ProcessAudioFrame(AVFrame* frame);
    
    // 播放正弦波测试
    HRESULT PlaySinWave(int nb_samples);
      // 访问器方法供VideoPlayer使用
    int GetAudioStreamIndex() const { return m_audioStreamIndex; }
    AVCodecContext* GetAudioCodecContext() const { return m_audioCodecContext; }
    
    // 新增：音视频同步功能
    void SetVideoTime(double videoTime);
    double GetAudioClock() const;
    void UpdateAudioSync();
    int SynchronizeAudio(AVFrame* frame, int wantedNbSamples);
    
private:
    // WASAPI相关
    WORD m_nChannels;
    DWORD m_nSamplesPerSec;
    int m_maxSampleCount; // 缓冲区大小（样本数）
    
    WAVEFORMATEX* m_pwfx;
    CComPtr<IMMDeviceEnumerator> m_pEnumerator;
    CComPtr<IMMDevice> m_pDevice;
    CComPtr<IAudioClient> m_pAudioClient;
    CComPtr<IAudioRenderClient> m_pRenderClient;
    CComPtr<ISimpleAudioVolume> m_pSimpleAudioVolume;
    
    DWORD m_flags;
    
    // FFmpeg 音频相关
    AVCodecContext* m_audioCodecContext;
    const AVCodec* m_audioCodec;
    SwrContext* m_swrContext;
    int m_audioStreamIndex;    // 状态
    bool m_isInitialized;
    bool m_isPlaying;
    float m_volume;
    double m_audioOffset;   // 音频偏移量（秒）
    
    // 新增：音视频同步相关变量
    double m_videoClock;        // 视频时钟（主时钟）
    double m_audioClock;        // 音频时钟
    double m_audioWriteTime;    // 音频写入时间
    
    // 音频同步算法相关
    double m_audioDiffCum;          // 累计音视频差异（加权总和）
    double m_audioDiffAvgCoef;      // 加权平均系数（公比q）
    int m_audioDiffAvgCount;        // 差异计数
    double m_audioDiffThreshold;    // 音频同步阈值
    
    // 音频缓冲区信息
    UINT32 m_bufferFrameCount;      // 音频缓冲区帧数
    UINT32 m_audioHwBufSize;        // 硬件缓冲区大小
    
    // 常量定义
    static const double AV_NOSYNC_THRESHOLD;      // 10.0秒
    static const int AUDIO_DIFF_AVG_NB;           // 20次
    static const int SAMPLE_CORRECTION_PERCENT_MAX; // 10%
    
    // 私有方法
    HRESULT InitWASAPI();
    bool SetupAudioDecoder(AVFormatContext* formatContext);
    void CleanupAudio();
};
