#include "AudioPlayer.h"
#include <iostream>

AudioPlayer::AudioPlayer()
    : m_audioCodecContext(nullptr)
    , m_audioCodec(nullptr)
    , m_swrContext(nullptr)
    , m_audioStreamIndex(-1)
    , m_hWaveOut(nullptr)
    , m_currentBuffer(0)
    , m_isInitialized(false)
    , m_isPlaying(false)
    , m_volume(1.0f)
{
    ZeroMemory(&m_waveFormat, sizeof(WAVEFORMATEX));
    ZeroMemory(m_waveHeaders, sizeof(m_waveHeaders));
    ZeroMemory(m_audioBuffers, sizeof(m_audioBuffers));
}

AudioPlayer::~AudioPlayer()
{
    CleanupAudio();
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
    
    // 设置音频格式
    m_waveFormat.wFormatTag = WAVE_FORMAT_PCM;
    m_waveFormat.nChannels = 2; // 立体声
    m_waveFormat.nSamplesPerSec = 44100;
    m_waveFormat.wBitsPerSample = 16;
    m_waveFormat.nBlockAlign = m_waveFormat.nChannels * m_waveFormat.wBitsPerSample / 8;
    m_waveFormat.nAvgBytesPerSec = m_waveFormat.nSamplesPerSec * m_waveFormat.nBlockAlign;
    m_waveFormat.cbSize = 0;
      // 初始化重采样器 - 使用兼容的FFmpeg API
    m_swrContext = swr_alloc();
    if (!m_swrContext)
    {
        std::cerr << "Failed to allocate resampler" << std::endl;
        return false;
    }
    
    // 配置重采样器参数 - 使用新的FFmpeg API
    AVChannelLayout in_ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    
    // 设置输入参数
    swr_alloc_set_opts2(&m_swrContext,
                        &out_ch_layout,           // 输出声道布局
                        AV_SAMPLE_FMT_S16,        // 输出采样格式
                        44100,                    // 输出采样率
                        &in_ch_layout,            // 输入声道布局
                        m_audioCodecContext->sample_fmt,    // 输入采样格式
                        m_audioCodecContext->sample_rate,   // 输入采样率
                        0, nullptr);
    
    if (swr_init(m_swrContext) < 0)
    {
        std::cerr << "Failed to initialize resampler" << std::endl;
        swr_free(&m_swrContext);
        return false;
    }
    
    // 打开音频设备
    MMRESULT result = waveOutOpen(&m_hWaveOut, WAVE_MAPPER, &m_waveFormat, 
                                  (DWORD_PTR)WaveOutProc, (DWORD_PTR)this, CALLBACK_FUNCTION);
    if (result != MMSYSERR_NOERROR)
    {
        std::cerr << "Failed to open audio device: " << result << std::endl;
        return false;
    }
    
    // 分配音频缓冲区
    for (int i = 0; i < BUFFER_COUNT; i++)
    {
        m_audioBuffers[i] = new uint8_t[BUFFER_SIZE];
        m_waveHeaders[i].lpData = (LPSTR)m_audioBuffers[i];
        m_waveHeaders[i].dwBufferLength = BUFFER_SIZE;
        m_waveHeaders[i].dwFlags = 0;
        m_waveHeaders[i].dwLoops = 0;
        
        waveOutPrepareHeader(m_hWaveOut, &m_waveHeaders[i], sizeof(WAVEHDR));
    }
    
    m_isInitialized = true;
    std::cout << "Audio player initialized successfully" << std::endl;
    return true;
}

void AudioPlayer::Play()
{
    if (m_isInitialized && !m_isPlaying)
    {
        m_isPlaying = true;
        // 这里可以添加音频播放逻辑
    }
}

void AudioPlayer::Pause()
{
    if (m_isInitialized)
    {
        m_isPlaying = !m_isPlaying;
        if (m_isPlaying)
        {
            waveOutRestart(m_hWaveOut);
        }
        else
        {
            waveOutPause(m_hWaveOut);
        }
    }
}

void AudioPlayer::Stop()
{
    if (m_isInitialized && m_isPlaying)
    {
        m_isPlaying = false;
        waveOutReset(m_hWaveOut);
    }
}

void AudioPlayer::SetVolume(float volume)
{
    m_volume = max(0.0f, min(1.0f, volume));
    if (m_hWaveOut)
    {
        DWORD vol = (DWORD)(m_volume * 0xFFFF);
        waveOutSetVolume(m_hWaveOut, MAKELONG(vol, vol));
    }
}

void AudioPlayer::CleanupAudio()
{
    if (m_hWaveOut)
    {
        waveOutReset(m_hWaveOut);
        
        // 清理缓冲区
        for (int i = 0; i < BUFFER_COUNT; i++)
        {
            if (m_waveHeaders[i].dwFlags & WHDR_PREPARED)
            {
                waveOutUnprepareHeader(m_hWaveOut, &m_waveHeaders[i], sizeof(WAVEHDR));
            }
            delete[] m_audioBuffers[i];
            m_audioBuffers[i] = nullptr;
        }
        
        waveOutClose(m_hWaveOut);
        m_hWaveOut = nullptr;
    }
    
    if (m_swrContext)
    {
        swr_free(&m_swrContext);
    }
    
    if (m_audioCodecContext)
    {
        avcodec_free_context(&m_audioCodecContext);
    }
    
    m_isInitialized = false;
}

void CALLBACK AudioPlayer::WaveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, 
                                       DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    if (uMsg == WOM_DONE)
    {
        // 音频缓冲区播放完成，可以在这里添加更多音频数据
        AudioPlayer* player = (AudioPlayer*)dwInstance;
        // 这里可以添加音频数据处理逻辑
    }
}
