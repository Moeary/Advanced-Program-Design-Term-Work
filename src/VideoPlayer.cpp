#include "VideoPlayer.h"
#include <iostream>
#include <algorithm>

VideoPlayer::VideoPlayer()
    : m_formatContext(nullptr)
    , m_codecContext(nullptr)
    , m_codec(nullptr)
    , m_frame(nullptr)
    , m_frameRGB(nullptr)
    , m_packet(nullptr)
    , m_swsContext(nullptr)    , m_videoStreamIndex(-1)
    , m_duration(0.0)
    , m_currentTime(0.0)
    , m_frameRate(25.0)  // 默认帧率
    , m_isPlaying(false)
    , m_isPaused(false)
    , m_hwnd(nullptr)
    , m_hdcMem(nullptr)
    , m_hBitmap(nullptr)
    , m_windowWidth(0)
    , m_windowHeight(0)
    , m_videoWidth(0)
    , m_videoHeight(0)
    , m_buffer(nullptr)
    , m_playThread(nullptr)
    , m_renderEvent(nullptr)
    , m_shouldStop(false)
{
    // 初始化 FFmpeg
    av_log_set_level(AV_LOG_QUIET);
    
    // 创建渲染事件
    m_renderEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

VideoPlayer::~VideoPlayer()
{
    Stop();
    CleanupFFmpeg();
    CleanupGDI();
    
    if (m_renderEvent)
    {
        CloseHandle(m_renderEvent);
    }
}

bool VideoPlayer::Initialize(HWND hwnd, const std::string& videoPath)
{
    m_hwnd = hwnd;
    
    // 获取窗口尺寸
    RECT rect;
    GetClientRect(hwnd, &rect);
    m_windowWidth = rect.right - rect.left;
    m_windowHeight = rect.bottom - rect.top;
    
    // 打开视频文件
    if (!OpenVideo(videoPath))
    {
        return false;
    }
      // 设置 GDI
    if (!SetupGDI())
    {
        return false;
    }
    
    // 初始化音频播放器
    m_audioPlayer.Initialize(m_formatContext);
    
    return true;
}

bool VideoPlayer::OpenVideo(const std::string& videoPath)
{
    // 分配格式上下文
    m_formatContext = avformat_alloc_context();
    if (!m_formatContext)
    {
        std::cerr << "Failed to allocate format context" << std::endl;
        return false;
    }
    
    // 打开输入文件
    if (avformat_open_input(&m_formatContext, videoPath.c_str(), nullptr, nullptr) != 0)
    {
        std::cerr << "Failed to open video file: " << videoPath << std::endl;
        return false;
    }
    
    // 获取流信息
    if (avformat_find_stream_info(m_formatContext, nullptr) < 0)
    {
        std::cerr << "Failed to find stream info" << std::endl;
        return false;
    }
    
    // 查找视频流
    m_videoStreamIndex = -1;
    for (unsigned int i = 0; i < m_formatContext->nb_streams; i++)
    {
        if (m_formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            m_videoStreamIndex = i;
            break;
        }
    }
      if (m_videoStreamIndex == -1)
    {
        std::cerr << "No video stream found" << std::endl;
        return false;
    }
    
    std::cout << "Found video stream at index: " << m_videoStreamIndex << std::endl;
    
    // 获取解码器参数
    AVCodecParameters* codecPar = m_formatContext->streams[m_videoStreamIndex]->codecpar;
      // 查找解码器
    m_codec = avcodec_find_decoder(codecPar->codec_id);
    if (!m_codec)
    {
        std::cerr << "Failed to find decoder for codec ID: " << codecPar->codec_id << std::endl;
        return false;
    }
    
    std::cout << "Using decoder: " << m_codec->name << std::endl;
    
    // 分配解码器上下文
    m_codecContext = avcodec_alloc_context3(m_codec);
    if (!m_codecContext)
    {
        return false;
    }
    
    // 复制解码器参数
    if (avcodec_parameters_to_context(m_codecContext, codecPar) < 0)
    {
        return false;
    }
    
    // 打开解码器
    if (avcodec_open2(m_codecContext, m_codec, nullptr) < 0)
    {
        return false;
    }    // 获取视频信息
    m_videoWidth = m_codecContext->width;
    m_videoHeight = m_codecContext->height;
    
    std::cout << "Video dimensions: " << m_videoWidth << "x" << m_videoHeight << std::endl;
    
    // 获取帧率
    AVRational frameRate = m_formatContext->streams[m_videoStreamIndex]->r_frame_rate;
    if (frameRate.num > 0 && frameRate.den > 0)
    {
        m_frameRate = (double)frameRate.num / frameRate.den;
    }
    else
    {
        // 如果没有帧率信息，使用时间基准来估算
        AVRational timeBase = m_formatContext->streams[m_videoStreamIndex]->time_base;
        m_frameRate = 1.0 / av_q2d(timeBase);
        if (m_frameRate > 60.0 || m_frameRate < 1.0)
        {
            m_frameRate = 25.0; // 默认值
        }
    }
    
    std::cout << "Frame rate: " << m_frameRate << " FPS" << std::endl;
    
    // 计算时长
    if (m_formatContext->duration != AV_NOPTS_VALUE)
    {
        m_duration = (double)m_formatContext->duration / AV_TIME_BASE;
    }
    
    // 分配帧
    m_frame = av_frame_alloc();
    m_frameRGB = av_frame_alloc();
    m_packet = av_packet_alloc();
    
    if (!m_frame || !m_frameRGB || !m_packet)
    {
        return false;
    }
    
    // 分配图像缓冲区
    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_BGR24, m_videoWidth, m_videoHeight, 1);
    m_buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
    
    av_image_fill_arrays(m_frameRGB->data, m_frameRGB->linesize, m_buffer, AV_PIX_FMT_BGR24, m_videoWidth, m_videoHeight, 1);
    
    // 初始化图像转换上下文
    m_swsContext = sws_getContext(
        m_videoWidth, m_videoHeight, m_codecContext->pix_fmt,
        m_videoWidth, m_videoHeight, AV_PIX_FMT_BGR24,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );
    
    return true;
}

bool VideoPlayer::SetupGDI()
{
    HDC hdc = GetDC(m_hwnd);
    m_hdcMem = CreateCompatibleDC(hdc);
    
    // 设置位图信息
    ZeroMemory(&m_bitmapInfo, sizeof(BITMAPINFO));
    m_bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    m_bitmapInfo.bmiHeader.biWidth = m_videoWidth;
    m_bitmapInfo.bmiHeader.biHeight = -m_videoHeight; // 负值表示从上到下
    m_bitmapInfo.bmiHeader.biPlanes = 1;
    m_bitmapInfo.bmiHeader.biBitCount = 24;
    m_bitmapInfo.bmiHeader.biCompression = BI_RGB;
    
    ReleaseDC(m_hwnd, hdc);
    
    return m_hdcMem != nullptr;
}

void VideoPlayer::Play()
{
    if (m_isPlaying)
        return;
    
    m_isPlaying = true;
    m_isPaused = false;
    m_shouldStop = false;
    
    // 启动音频播放
    m_audioPlayer.Play();
    
    // 创建播放线程
    m_playThread = CreateThread(nullptr, 0, PlayThreadProc, this, 0, nullptr);
}

void VideoPlayer::Pause()
{
    m_isPaused = !m_isPaused;
    m_audioPlayer.Pause();
}

void VideoPlayer::Stop()
{
    if (!m_isPlaying)
        return;
    
    m_shouldStop = true;
    m_isPlaying = false;
    m_isPaused = false;
    
    // 停止音频播放
    m_audioPlayer.Stop();
    
    // 等待播放线程结束
    if (m_playThread)
    {
        WaitForSingleObject(m_playThread, 2000);
        CloseHandle(m_playThread);
        m_playThread = nullptr;
    }
    
    // 重置到开始位置
    if (m_formatContext)
    {
        av_seek_frame(m_formatContext, m_videoStreamIndex, 0, AVSEEK_FLAG_BACKWARD);
    }
    m_currentTime = 0.0;
}

void VideoPlayer::Seek(double seconds)
{
    if (!m_formatContext || m_videoStreamIndex < 0)
        return;
    
    int64_t timestamp = (int64_t)(seconds * AV_TIME_BASE);
    av_seek_frame(m_formatContext, -1, timestamp, AVSEEK_FLAG_BACKWARD);
    m_currentTime = seconds;
}

DWORD WINAPI VideoPlayer::PlayThreadProc(LPVOID lpParam)
{
    VideoPlayer* player = static_cast<VideoPlayer*>(lpParam);
    player->PlayLoop();
    return 0;
}

void VideoPlayer::PlayLoop()
{
    LARGE_INTEGER frequency, lastTime, currentTime;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&lastTime);
    
    while (!m_shouldStop)
    {
        if (m_isPaused)
        {
            Sleep(10);
            QueryPerformanceCounter(&lastTime);
            continue;
        }
        
        // 读取数据包
        int ret = av_read_frame(m_formatContext, m_packet);
        if (ret < 0)
        {
            // 文件结束或错误
            break;
        }
          if (m_packet->stream_index == m_videoStreamIndex)
        {
            // 发送数据包到解码器
            ret = avcodec_send_packet(m_codecContext, m_packet);
            if (ret < 0)
            {
                av_packet_unref(m_packet);
                continue;
            }
            
            // 接收解码后的帧
            ret = avcodec_receive_frame(m_codecContext, m_frame);
            if (ret == 0)
            {
                // 转换像素格式
                sws_scale(m_swsContext, m_frame->data, m_frame->linesize, 0, m_videoHeight,
                         m_frameRGB->data, m_frameRGB->linesize);
                
                // 更新当前时间
                if (m_packet->pts != AV_NOPTS_VALUE)
                {
                    m_currentTime = m_packet->pts * av_q2d(m_formatContext->streams[m_videoStreamIndex]->time_base);
                }
                  // 触发渲染
                InvalidateRect(m_hwnd, nullptr, FALSE);
                
                // 控制播放速度 - 使用实际帧率
                QueryPerformanceCounter(&currentTime);
                double elapsed = (double)(currentTime.QuadPart - lastTime.QuadPart) / frequency.QuadPart;
                double frameTime = 1.0 / m_frameRate;
                
                if (elapsed < frameTime)
                {
                    DWORD sleepTime = (DWORD)((frameTime - elapsed) * 1000);
                    if (sleepTime > 0 && sleepTime < 100) // 避免过长的睡眠时间
                    {
                        Sleep(sleepTime);
                    }
                }
                
                QueryPerformanceCounter(&lastTime);
            }
        }
        
        av_packet_unref(m_packet);
    }
    
    m_isPlaying = false;
}

void VideoPlayer::Render()
{
    if (!m_hdcMem || !m_buffer)
        return;
    
    HDC hdc = GetDC(m_hwnd);
    
    // 直接从缓冲区创建位图并绘制（避免每次都创建删除）
    HBITMAP hTempBitmap = CreateDIBitmap(hdc, &m_bitmapInfo.bmiHeader, CBM_INIT, m_buffer, &m_bitmapInfo, DIB_RGB_COLORS);
    
    if (hTempBitmap)
    {
        HDC hdcTemp = CreateCompatibleDC(hdc);
        HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcTemp, hTempBitmap);
        
        // 计算缩放比例以保持宽高比
        double scaleX = (double)m_windowWidth / m_videoWidth;
        double scaleY = (double)m_windowHeight / m_videoHeight;
        double scale = min(scaleX, scaleY);
        
        int displayWidth = (int)(m_videoWidth * scale);
        int displayHeight = (int)(m_videoHeight * scale);
        int offsetX = (m_windowWidth - displayWidth) / 2;
        int offsetY = (m_windowHeight - displayHeight) / 2;
        
        // 清除背景
        RECT rect = { 0, 0, m_windowWidth, m_windowHeight };
        FillRect(hdc, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH));
        
        // 绘制视频帧（使用COLORONCOLOR可能比HALFTONE更快）
        SetStretchBltMode(hdc, COLORONCOLOR);
        StretchBlt(hdc, offsetX, offsetY, displayWidth, displayHeight,
                   hdcTemp, 0, 0, m_videoWidth, m_videoHeight, SRCCOPY);
        
        SelectObject(hdcTemp, hOldBitmap);
        DeleteDC(hdcTemp);
        DeleteObject(hTempBitmap);
    }
    
    ReleaseDC(m_hwnd, hdc);
}

void VideoPlayer::OnResize(int width, int height)
{
    m_windowWidth = width;
    m_windowHeight = height;
}

void VideoPlayer::CleanupFFmpeg()
{
    if (m_swsContext)
    {
        sws_freeContext(m_swsContext);
        m_swsContext = nullptr;
    }
    
    if (m_buffer)
    {
        av_free(m_buffer);
        m_buffer = nullptr;
    }
    
    if (m_frame)
    {
        av_frame_free(&m_frame);
    }
    
    if (m_frameRGB)
    {
        av_frame_free(&m_frameRGB);
    }
    
    if (m_packet)
    {
        av_packet_free(&m_packet);
    }
    
    if (m_codecContext)
    {
        avcodec_free_context(&m_codecContext);
    }
    
    if (m_formatContext)
    {
        avformat_close_input(&m_formatContext);
    }
}

void VideoPlayer::CleanupGDI()
{
    if (m_hBitmap)
    {
        DeleteObject(m_hBitmap);
        m_hBitmap = nullptr;
    }
    
    if (m_hdcMem)
    {
        DeleteDC(m_hdcMem);
        m_hdcMem = nullptr;
    }
}

void VideoPlayer::SetVolume(float volume)
{
    m_audioPlayer.SetVolume(volume);
}

bool VideoPlayer::HasAudio() const
{
    return m_audioPlayer.IsInitialized();
}
