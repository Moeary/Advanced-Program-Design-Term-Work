#include "AudioPlayer.h"
#include <iostream>
#include <cmath>

AudioPlayer::AudioPlayer(WORD nChannels, DWORD nSamplesPerSec)
    : m_nChannels(nChannels)
    , m_nSamplesPerSec(nSamplesPerSec)
    , m_maxSampleCount(0)
    , m_pwfx(nullptr)
    , m_flags(0)
    , m_audioCodecContext(nullptr)
    , m_audioCodec(nullptr)
    , m_swrContext(nullptr)
    , m_audioStreamIndex(-1)
    , m_isInitialized(false)
    , m_isPlaying(false)
    , m_volume(1.0f)
{
    // 初始化COM
    CoInitialize(nullptr);
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
    }

    // 获取渲染客户端
    hr = m_pAudioClient->GetService(__uuidof(IAudioRenderClient), (void**)&m_pRenderClient);
    if (FAILED(hr)) {
        std::cerr << "Failed to get render client" << std::endl;
        return hr;
    }

    m_maxSampleCount = m_pwfx->nSamplesPerSec;
    m_flags = 0;

    std::cout << "WASAPI initialized successfully" << std::endl;
    return hr;
}

HRESULT AudioPlayer::Start()
{
    if (m_pAudioClient)
    {
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
        memset(pData, 0, sampleCount * m_nChannels * sizeof(float));
    }

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
