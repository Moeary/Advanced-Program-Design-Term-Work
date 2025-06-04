#include "AudioPlayer.h"
#include <iostream>
#include <cmath>
#include <algorithm>

// 定义常量
const double AudioPlayer::AV_NOSYNC_THRESHOLD = 10.0;
const int AudioPlayer::AUDIO_DIFF_AVG_NB = 20;
const int AudioPlayer::SAMPLE_CORRECTION_PERCENT_MAX = 10;

AudioPlayer::AudioPlayer(WORD nChannels, DWORD nSamplesPerSec)
    : m_nChannels(nChannels)
    , m_nSamplesPerSec(nSamplesPerSec)
    , m_maxSampleCount(0)
    , m_pwfx(nullptr)
    , m_flags(0)
    , m_audioCodecContext(nullptr)
    , m_audioCodec(nullptr)
    , m_swrContext(nullptr)
    , m_audioStreamIndex(-1)    , m_isInitialized(false)
    , m_isPlaying(false)
    , m_volume(1.0f)
    , m_audioOffset(0.0)
    , m_videoClock(0.0)
    , m_audioClock(0.0)
    , m_audioWriteTime(0.0)
    , m_audioDiffCum(0.0)
    , m_audioDiffAvgCoef(0.0)
    , m_audioDiffAvgCount(0)
    , m_audioDiffThreshold(0.0)
    , m_bufferFrameCount(0)
    , m_audioHwBufSize(0)
{
    // 初始化COM
    CoInitialize(nullptr);
    
    // 计算加权平均系数 (公比q)
    // audio_diff_avg_coef = exp(log(0.01) / AUDIO_DIFF_AVG_NB)
    m_audioDiffAvgCoef = exp(log(0.01) / AUDIO_DIFF_AVG_NB); // ≈ 0.79432
    
    InitWASAPI();
}

AudioPlayer::~AudioPlayer()
{
    CleanupAudio();
    CoUninitialize();
}

bool AudioPlayer::Initialize(AVFormatContext* formatContext)
{
    if (!formatContext)
        return false;
    
    return SetupAudioDecoder(formatContext);
}

bool AudioPlayer::SetupAudioDecoder(AVFormatContext* formatContext)
{
    // 查找音频流
    for (unsigned int i = 0; i < formatContext->nb_streams; i++)
    {
        if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            m_audioStreamIndex = i;
            break;
        }
    }
    
    if (m_audioStreamIndex == -1)
    {
        std::cout << "No audio stream found" << std::endl;
        return false;
    }
    
    std::cout << "Found audio stream at index: " << m_audioStreamIndex << std::endl;
    
    // 获取音频解码器参数
    AVCodecParameters* codecPar = formatContext->streams[m_audioStreamIndex]->codecpar;
    
    // 查找音频解码器
    m_audioCodec = avcodec_find_decoder(codecPar->codec_id);
    if (!m_audioCodec)
    {
        std::cerr << "Failed to find audio decoder" << std::endl;
        return false;
    }
    
    // 分配音频解码器上下文
    m_audioCodecContext = avcodec_alloc_context3(m_audioCodec);
    if (!m_audioCodecContext)
    {
        std::cerr << "Failed to allocate audio codec context" << std::endl;
        return false;
    }
    
    // 复制解码器参数
    if (avcodec_parameters_to_context(m_audioCodecContext, codecPar) < 0)
    {
        std::cerr << "Failed to copy audio codec parameters" << std::endl;
        return false;
    }
    
    // 打开音频解码器
    if (avcodec_open2(m_audioCodecContext, m_audioCodec, nullptr) < 0)
    {
        std::cerr << "Failed to open audio codec" << std::endl;
        return false;
    }
    
    // 初始化重采样器 - 转换为FLTP格式用于WASAPI
    m_swrContext = swr_alloc();
    if (!m_swrContext)
    {
        std::cerr << "Failed to allocate resampler" << std::endl;
        return false;
    }
    
    // 配置重采样器参数 - 输出FLTP格式供WASAPI使用
    AVChannelLayout in_ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    
    // 设置输入参数
    swr_alloc_set_opts2(&m_swrContext,
                        &out_ch_layout,                     // 输出声道布局
                        AV_SAMPLE_FMT_FLTP,                 // 输出采样格式 (FLTP)
                        m_nSamplesPerSec,                   // 输出采样率
                        &in_ch_layout,                      // 输入声道布局
                        m_audioCodecContext->sample_fmt,    // 输入采样格式
                        m_audioCodecContext->sample_rate,   // 输入采样率
                        0, nullptr);
    
    if (swr_init(m_swrContext) < 0)
    {
        std::cerr << "Failed to initialize resampler" << std::endl;
        swr_free(&m_swrContext);
        return false;
    }
    
    // WASAPI已在构造函数中初始化
    m_isInitialized = true;
    std::cout << "Audio player initialized successfully with WASAPI" << std::endl;
    return true;
}

HRESULT AudioPlayer::InitWASAPI()
{
    constexpr auto REFTIMES_PER_SEC = 10000000; // 1秒的缓冲区

    HRESULT hr;

    // 创建设备枚举器
    hr = m_pEnumerator.CoCreateInstance(__uuidof(MMDeviceEnumerator));
    if (FAILED(hr)) {
        std::cerr << "Failed to create device enumerator" << std::endl;
        return hr;
    }

    // 获取默认音频端点
    hr = m_pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_pDevice);
    if (FAILED(hr)) {
        std::cerr << "Failed to get default audio endpoint" << std::endl;
        return hr;
    }

    // 激活音频客户端
    hr = m_pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&m_pAudioClient);
    if (FAILED(hr)) {
        std::cerr << "Failed to activate audio client" << std::endl;
        return hr;
    }

    // 获取音频会话管理器和音量控制
    CComPtr<IAudioSessionManager> pAudioSessionManager;
    hr = m_pDevice->Activate(__uuidof(IAudioSessionManager), CLSCTX_INPROC_SERVER,
                            NULL, (void**)&pAudioSessionManager);
    if (FAILED(hr)) {
        std::cerr << "Failed to get audio session manager" << std::endl;
        return hr;
    }

    hr = pAudioSessionManager->GetSimpleAudioVolume(&GUID_NULL, 0, &m_pSimpleAudioVolume);
    if (FAILED(hr)) {
        std::cerr << "Failed to get simple audio volume" << std::endl;
        return hr;
    }

    // 获取混合格式
    hr = m_pAudioClient->GetMixFormat(&m_pwfx);
    if (FAILED(hr)) {
        std::cerr << "Failed to get mix format" << std::endl;
        return hr;
    }

    // 设置音频格式
    m_pwfx->nSamplesPerSec = m_nSamplesPerSec;
    m_pwfx->nChannels = m_nChannels;
    m_pwfx->nAvgBytesPerSec = m_pwfx->nSamplesPerSec * m_nChannels * (m_pwfx->wBitsPerSample / 8);
    m_pwfx->wFormatTag = WAVE_FORMAT_EXTENSIBLE;

    // 初始化音频客户端
    hr = m_pAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
        REFTIMES_PER_SEC,
        0,
        m_pwfx,
        NULL);
    if (FAILED(hr)) {
        std::cerr << "Failed to initialize audio client" << std::endl;
        return hr;
    }    // 获取渲染客户端
    hr = m_pAudioClient->GetService(__uuidof(IAudioRenderClient), (void**)&m_pRenderClient);
    if (FAILED(hr)) {
        std::cerr << "Failed to get render client" << std::endl;
        return hr;
    }

    // 获取缓冲区大小信息（用于音视频同步）
    hr = m_pAudioClient->GetBufferSize(&m_bufferFrameCount);
    if (FAILED(hr)) {
        std::cerr << "Failed to get buffer size" << std::endl;
        return hr;
    }
    
    // 计算硬件缓冲区大小（字节）
    m_audioHwBufSize = m_bufferFrameCount * m_pwfx->nChannels * (m_pwfx->wBitsPerSample / 8);
    
    // 计算音频同步阈值（一次回调的时间间隔）
    m_audioDiffThreshold = (double)m_audioHwBufSize / (m_pwfx->nSamplesPerSec * m_pwfx->nChannels * (m_pwfx->wBitsPerSample / 8));
    
    std::cout << "Audio buffer size: " << m_bufferFrameCount << " frames" << std::endl;
    std::cout << "Audio diff threshold: " << m_audioDiffThreshold << " seconds" << std::endl;

    m_maxSampleCount = m_pwfx->nSamplesPerSec;
    m_flags = 0;

    std::cout << "WASAPI initialized successfully" << std::endl;
    return hr;
}

HRESULT AudioPlayer::Start()
{
    if (m_pAudioClient)
    {
        // 重置音视频同步状态
        m_videoClock = 0.0;
        m_audioClock = 0.0;
        m_audioWriteTime = 0.0;
        m_audioDiffCum = 0.0;
        m_audioDiffAvgCount = 0;
        
        m_isPlaying = true;
        return m_pAudioClient->Start();
    }
    return E_FAIL;
}

HRESULT AudioPlayer::Stop()
{
    if (m_pAudioClient)
    {
        m_isPlaying = false;
        
        // 重置音视频同步状态
        m_videoClock = 0.0;
        m_audioClock = 0.0;
        m_audioWriteTime = 0.0;
        m_audioDiffCum = 0.0;
        m_audioDiffAvgCount = 0;
        
        return m_pAudioClient->Stop();
    }
    return E_FAIL;
}

void AudioPlayer::Pause()
{
    if (m_pAudioClient)
    {
        m_isPlaying = !m_isPlaying;
        if (m_isPlaying)
        {
            m_pAudioClient->Start();
        }
        else
        {
            m_pAudioClient->Stop();
        }
    }
}

void AudioPlayer::SetVolume(float volume)
{
    m_volume = (volume < 0.0f) ? 0.0f : (volume > 1.0f) ? 1.0f : volume;
    if (m_pSimpleAudioVolume)
    {
        m_pSimpleAudioVolume->SetMasterVolume(m_volume, NULL);
    }
}

BYTE* AudioPlayer::GetBuffer(UINT32 wantFrames)
{
    if (m_pRenderClient)
    {
        BYTE* buffer;
        HRESULT hr = m_pRenderClient->GetBuffer(wantFrames, &buffer);
        if (SUCCEEDED(hr))
        {
            return buffer;
        }
    }
    return nullptr;
}

HRESULT AudioPlayer::ReleaseBuffer(UINT32 writtenFrames)
{
    if (m_pRenderClient)
    {
        return m_pRenderClient->ReleaseBuffer(writtenFrames, m_flags);
    }
    return E_FAIL;
}

HRESULT AudioPlayer::WriteFLTP(float* left, float* right, UINT32 sampleCount)
{
    if (!m_pAudioClient || !m_pRenderClient)
        return E_FAIL;

    // 检查缓冲区填充情况
    UINT32 padding;
    HRESULT hr = m_pAudioClient->GetCurrentPadding(&padding);
    if (FAILED(hr))
        return hr;

    // 如果缓冲区满了，重置以避免延迟累积
    if ((m_maxSampleCount - padding) < sampleCount)
    {
        m_pAudioClient->Stop();
        m_pAudioClient->Reset();
        m_pAudioClient->Start();
    }

    // 获取缓冲区
    auto pData = GetBuffer(sampleCount);
    if (!pData)
        return E_FAIL;

    if (left && right)
    {
        // 立体声：交替存储左右声道
        for (UINT32 i = 0; i < sampleCount; i++)
        {
            int p = i * 2;
            ((float*)pData)[p] = left[i];
            ((float*)pData)[p + 1] = right[i];
        }
    }
    else if (left)
    {
        // 单声道：左声道复制到右声道
        for (UINT32 i = 0; i < sampleCount; i++)
        {
            int p = i * 2;
            ((float*)pData)[p] = left[i];
            ((float*)pData)[p + 1] = left[i];
        }
    }
    else
    {
        // 静音
        memset(pData, 0, sampleCount * m_nChannels * sizeof(float));    }

    // 更新音频写入时间（用于音频时钟计算）
    m_audioWriteTime += (double)sampleCount / m_pwfx->nSamplesPerSec;
    
    return ReleaseBuffer(sampleCount);
}

HRESULT AudioPlayer::PlaySinWave(int nb_samples)
{
    static double m_time = 0.0;
    double m_deltaTime = 1.0 / nb_samples;

    auto pData = GetBuffer(nb_samples);
    if (!pData)
        return E_FAIL;    for (int sample = 0; sample < nb_samples; ++sample)
    {
        float value = 0.05f * (float)std::sin(5000.0 * m_time);
        int p = sample * m_nChannels;
        ((float*)pData)[p] = value;
        ((float*)pData)[p + 1] = value;
        m_time += m_deltaTime;
    }

    return ReleaseBuffer(nb_samples);
}

// 新增：音视频同步功能实现

void AudioPlayer::SetVideoTime(double videoTime)
{
    // Apply audio offset for synchronization
    m_videoClock = videoTime + m_audioOffset;
}

double AudioPlayer::GetAudioClock() const
{
    return m_audioClock;
}

void AudioPlayer::UpdateAudioSync()
{
    // 更新音频时钟
    if (m_isPlaying && m_pAudioClient)
    {
        UINT32 numFramesPadding;
        HRESULT hr = m_pAudioClient->GetCurrentPadding(&numFramesPadding);
        if (SUCCEEDED(hr))
        {
            // 计算音频时钟 = 写入时间 - 缓冲区中剩余的播放时间
            double bufferTime = (double)numFramesPadding / m_pwfx->nSamplesPerSec;
            m_audioClock = m_audioWriteTime - bufferTime;
        }
    }
}

int AudioPlayer::SynchronizeAudio(AVFrame* frame, int wantedNbSamples)
{
    if (!frame || !m_isPlaying)
        return wantedNbSamples;
    
    int nbSamples = frame->nb_samples;
    
    // 计算当前音视频时间差
    double diff = m_audioClock - m_videoClock;
    
    // 如果差异太大（超过10秒），不进行同步
    if (fabs(diff) >= AV_NOSYNC_THRESHOLD)
    {
        return nbSamples;
    }
    
    // 计算加权平均数
    // audio_diff_cum = diff + audio_diff_avg_coef * audio_diff_cum
    m_audioDiffCum = diff + m_audioDiffAvgCoef * m_audioDiffCum;
    
    // 增加差异计数
    if (m_audioDiffAvgCount < AUDIO_DIFF_AVG_NB)
    {
        m_audioDiffAvgCount++;
    }
    
    // 计算加权平均差异
    // avg_diff = audio_diff_cum * (1.0 - audio_diff_avg_coef)
    double avgDiff = m_audioDiffCum * (1.0 - m_audioDiffAvgCoef);
    
    // 如果加权平均差异超过阈值，进行样本数调整
    if (fabs(avgDiff) >= m_audioDiffThreshold)
    {
        // 计算需要的样本数
        // wanted_nb_samples = nb_samples + (int)(diff * sample_rate)
        wantedNbSamples = nbSamples + (int)(diff * m_audioCodecContext->sample_rate);
        
        // 限制调整幅度不超过10%
        int minNbSamples = (nbSamples * (100 - SAMPLE_CORRECTION_PERCENT_MAX)) / 100;
        int maxNbSamples = (nbSamples * (100 + SAMPLE_CORRECTION_PERCENT_MAX)) / 100;
        
        // 使用av_clip限制范围
        wantedNbSamples = (wantedNbSamples < minNbSamples) ? minNbSamples : 
                         (wantedNbSamples > maxNbSamples) ? maxNbSamples : wantedNbSamples;
        
        std::cout << "Audio sync: diff=" << diff << ", avg_diff=" << avgDiff 
                  << ", samples=" << nbSamples << "->" << wantedNbSamples << std::endl;
    }
    else
    {
        wantedNbSamples = nbSamples;
    }
    
    return wantedNbSamples;
}

HRESULT AudioPlayer::ProcessAudioFrame(AVFrame* frame)
{
    if (!frame || !m_swrContext || !m_isPlaying)
        return E_FAIL;
    
    // 更新音频时钟
    UpdateAudioSync();
    
    // 进行音视频同步，获取调整后的样本数
    int wantedNbSamples = SynchronizeAudio(frame, frame->nb_samples);
    
    // 如果需要样本补偿，使用swr_set_compensation
    if (wantedNbSamples != frame->nb_samples)
    {
        int compensation = (wantedNbSamples - frame->nb_samples) * m_pwfx->nSamplesPerSec / frame->sample_rate;
        int out_count = wantedNbSamples * m_pwfx->nSamplesPerSec / frame->sample_rate;
        
        if (swr_set_compensation(m_swrContext, compensation, out_count) < 0)
        {
            std::cerr << "swr_set_compensation() failed" << std::endl;
            return E_FAIL;
        }
    }
    
    // 分配输出缓冲区
    uint8_t* output[2] = {nullptr};
    int out_samples = av_rescale_rnd(swr_get_delay(m_swrContext, frame->sample_rate) + frame->nb_samples,
                                    m_pwfx->nSamplesPerSec, frame->sample_rate, AV_ROUND_UP);
    
    if (av_samples_alloc(output, nullptr, 2, out_samples, AV_SAMPLE_FMT_FLTP, 0) < 0)
    {
        std::cerr << "Failed to allocate output samples" << std::endl;
        return E_FAIL;
    }
    
    // 重采样
    int converted_samples = swr_convert(m_swrContext, output, out_samples,
                                       (const uint8_t**)frame->data, frame->nb_samples);
    
    if (converted_samples <= 0)
    {
        av_freep(&output[0]);
        return E_FAIL;
    }
    
    // 写入音频数据
    HRESULT hr = WriteFLTP((float*)output[0], (float*)output[1], converted_samples);
    
    // 释放缓冲区
    av_freep(&output[0]);
    
    return hr;
}

void AudioPlayer::CleanupAudio()
{
    // 停止音频播放
    if (m_pAudioClient)
    {
        m_pAudioClient->Stop();
        m_pAudioClient->Reset();
    }

    // 释放WASAPI资源
    m_pRenderClient.Release();
    m_pSimpleAudioVolume.Release();
    m_pAudioClient.Release();
    m_pDevice.Release();
    m_pEnumerator.Release();

    if (m_pwfx)
    {
        CoTaskMemFree(m_pwfx);
        m_pwfx = nullptr;
    }

    // 清理FFmpeg资源
    if (m_swrContext)
    {
        swr_free(&m_swrContext);
    }

    if (m_audioCodecContext)
    {
        avcodec_free_context(&m_audioCodecContext);
    }

    m_isInitialized = false;
    m_isPlaying = false;
}

// Audio offset control methods
void AudioPlayer::SetAudioOffset(double offset)
{
    // Clamp offset to reasonable range: -2.0s to +2.0s
    m_audioOffset = (std::max)(-2.0, (std::min)(2.0, offset));
}

double AudioPlayer::GetAudioOffset() const
{
    return m_audioOffset;
}
