#pragma once

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
    HRESULT Stop();
    void Pause();
    void SetVolume(float volume); // 0.0 - 1.0
    
    bool IsInitialized() const { return m_isInitialized; }
    
    // WASAPI缓冲区操作
    BYTE* GetBuffer(UINT32 wantFrames);
    HRESULT ReleaseBuffer(UINT32 writtenFrames);
    
    // FLTP格式音频写入 - 左右声道分开处理
    HRESULT WriteFLTP(float* left, float* right, UINT32 sampleCount);
    
    // 播放正弦波测试
    HRESULT PlaySinWave(int nb_samples);
    
    // 访问器方法供VideoPlayer使用
    int GetAudioStreamIndex() const { return m_audioStreamIndex; }
    AVCodecContext* GetAudioCodecContext() const { return m_audioCodecContext; }
    
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
    int m_audioStreamIndex;
      // 状态
    bool m_isInitialized;
    bool m_isPlaying;
    float m_volume;
    
    // 私有方法
    HRESULT InitWASAPI();
    bool SetupAudioDecoder(AVFormatContext* formatContext);
    void CleanupAudio();
};
