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
    , m_swsContext(nullptr)
    , m_videoStreamIndex(-1)
    , m_duration(0.0)
    , m_currentTime(0.0)
    , m_frameRate(25.0)  // 默认帧率
    , m_isPlaying(false)
    , m_isPaused(false)
    , m_hwnd(nullptr)
    , m_hdcMem(nullptr)
    , m_hBitmap(nullptr)
    , m_useD3D9(true)  // 默认使用 D3D9
    , m_windowWidth(0)
    , m_windowHeight(0)
    , m_videoWidth(0)
    , m_videoHeight(0)
    , m_buffer(nullptr)
    , m_playThread(nullptr)
    , m_renderEvent(nullptr)    , m_shouldStop(false)
    , m_scalingMode(ScalingMode::FIT_TO_WINDOW)  // 默认适应窗口
    , m_currentFilter(FilterType::NONE)         // 默认无滤镜
    , m_mosaicSize(8)                          // 马赛克块大小
{
    // 初始化 FFmpeg
    av_log_set_level(AV_LOG_QUIET);
    
    // 初始化 D3D9 表面描述
    memset(&m_d3d9SurfaceDesc, 0, sizeof(m_d3d9SurfaceDesc));
    memset(&m_d3dpp, 0, sizeof(m_d3dpp));
    
    // 创建渲染事件
    m_renderEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

VideoPlayer::~VideoPlayer()
{
    Stop();
    CleanupFFmpeg();
    CleanupGDI();
    CleanupD3D9();
    
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
    
    // 设置渲染方式
    if (m_useD3D9)
    {
        if (!SetupD3D9())
        {
            std::cerr << "Failed to initialize D3D9, falling back to GDI" << std::endl;
            m_useD3D9 = false;
            if (!SetupGDI())
            {
                return false;
            }
        }
    }
    else
    {
        if (!SetupGDI())
        {
            return false;
        }
    }
      // 初始化音频播放器
    if (m_audioPlayer.Initialize(m_formatContext))
    {
        m_audioPlayer.Start();
        std::cout << "Audio player started successfully" << std::endl;
    }
    
    return true;
}

bool VideoPlayer::OpenVideo(const std::string& videoPath)
{
    // 首先清理之前的资源
    CleanupFFmpeg();
    
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
    AVPixelFormat targetFormat = m_useD3D9 ? AV_PIX_FMT_BGRA : AV_PIX_FMT_BGR24;
    int bytesPerPixel = m_useD3D9 ? 4 : 3;
    
    int numBytes = av_image_get_buffer_size(targetFormat, m_videoWidth, m_videoHeight, 1);
    m_buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
    
    av_image_fill_arrays(m_frameRGB->data, m_frameRGB->linesize, m_buffer, targetFormat, m_videoWidth, m_videoHeight, 1);    // 初始化图像转换上下文，使用高质量缩放参数
    m_swsContext = sws_getContext(
        m_videoWidth, m_videoHeight, m_codecContext->pix_fmt,
        m_videoWidth, m_videoHeight, targetFormat,
        SWS_LANCZOS | SWS_FULL_CHR_H_INT | SWS_FULL_CHR_H_INP | SWS_ACCURATE_RND, 
        nullptr, nullptr, nullptr  // 使用Lanczos缩放算法，启用全色度插值和精确舍入
    );
    
    // 如果使用 D3D9，创建离屏表面
    if (m_useD3D9 && m_d3d9Device) {
        // 释放现有表面
        if (m_d3d9Surface) {
            m_d3d9Surface.Reset();
        }
        memset(&m_d3d9SurfaceDesc, 0, sizeof(m_d3d9SurfaceDesc));

        // 创建离屏普通表面用于视频帧
        HRESULT hr = m_d3d9Device->CreateOffscreenPlainSurface(
            m_videoWidth,
            m_videoHeight,
            D3DFMT_X8R8G8B8, // 对应 AV_PIX_FMT_BGRA 格式
            D3DPOOL_DEFAULT,
            &m_d3d9Surface,
            nullptr
        );

        if (FAILED(hr)) {
            std::cerr << "Failed to create D3D9 offscreen surface." << std::endl;
            // 可以考虑回退到 GDI 或报告错误
        } else {
            m_d3d9Surface->GetDesc(&m_d3d9SurfaceDesc);
        }
    }
    
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

bool VideoPlayer::SetupD3D9()
{
    // 创建 Direct3D 9 对象
    m_d3d9 = Direct3DCreate9(D3D_SDK_VERSION);
    if (!m_d3d9)
    {
        std::cerr << "Failed to create Direct3D 9 object" << std::endl;
        return false;
    }
    
    std::cout << "Window size: " << m_windowWidth << "x" << m_windowHeight << std::endl;
    std::cout << "Video size: " << m_videoWidth << "x" << m_videoHeight << std::endl;
    
    // 检查多重采样抗锯齿支持
    DWORD msaaQuality = 0;
    D3DMULTISAMPLE_TYPE msaaType = D3DMULTISAMPLE_NONE;
    
    // 尝试检测最高可用的 MSAA 级别
    D3DMULTISAMPLE_TYPE msaaLevels[] = { D3DMULTISAMPLE_8_SAMPLES, D3DMULTISAMPLE_4_SAMPLES, D3DMULTISAMPLE_2_SAMPLES };
    for (int i = 0; i < 3; i++)
    {
        if (SUCCEEDED(m_d3d9->CheckDeviceMultiSampleType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, 
            D3DFMT_X8R8G8B8, TRUE, msaaLevels[i], &msaaQuality)))
        {
            msaaType = msaaLevels[i];
            std::cout << "MSAA " << (1 << (i + 1)) << "x supported with " << msaaQuality << " quality levels" << std::endl;
            break;
        }
    }
    
    // 设置展示参数
    D3DPRESENT_PARAMETERS d3dParams = {};
    d3dParams.Windowed = TRUE;
    d3dParams.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dParams.BackBufferFormat = D3DFMT_X8R8G8B8;  // 32位颜色
    d3dParams.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
    d3dParams.BackBufferWidth = m_windowWidth;
    d3dParams.BackBufferHeight = m_windowHeight;
    d3dParams.hDeviceWindow = m_hwnd;
    d3dParams.PresentationInterval = D3DPRESENT_INTERVAL_ONE; // 启用垂直同步
    d3dParams.MultiSampleType = msaaType;
    d3dParams.MultiSampleQuality = (msaaQuality > 0) ? msaaQuality - 1 : 0;
    
    // 保存参数以备后用
    m_d3dpp = d3dParams;
    
    // 创建 D3D9 设备
    HRESULT hr = m_d3d9->CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        m_hwnd,
        D3DCREATE_HARDWARE_VERTEXPROCESSING,
        &d3dParams,
        m_d3d9Device.GetAddressOf()
    );
    
    if (FAILED(hr))
    {
        std::cerr << "Failed to create D3D9 device with MSAA, trying without MSAA..." << std::endl;
        // 如果 MSAA 失败，尝试不使用 MSAA
        d3dParams.MultiSampleType = D3DMULTISAMPLE_NONE;
        d3dParams.MultiSampleQuality = 0;
        m_d3dpp = d3dParams;
        
        hr = m_d3d9->CreateDevice(
            D3DADAPTER_DEFAULT,
            D3DDEVTYPE_HAL,
            m_hwnd,
            D3DCREATE_HARDWARE_VERTEXPROCESSING,
            &d3dParams,
            m_d3d9Device.GetAddressOf()
        );
        
        if (FAILED(hr))
        {
            std::cerr << "Failed to create D3D9 device, HRESULT: 0x" << std::hex << hr << std::endl;
            return false;
        }
    }
    
    std::cout << "Direct3D 9 initialized successfully with " << 
        (msaaType == D3DMULTISAMPLE_NONE ? "no MSAA" : 
         (msaaType == D3DMULTISAMPLE_2_SAMPLES ? "2x MSAA" :
          (msaaType == D3DMULTISAMPLE_4_SAMPLES ? "4x MSAA" : "8x MSAA"))) << std::endl;
    std::cout << "Using D3D9 rendering with format: " << (m_useD3D9 ? "BGRA" : "BGR24") << std::endl;
    return true;
}

void VideoPlayer::Play()
{
    if (m_isPlaying)
        return;
    
    m_isPlaying = true;
    m_isPaused = false;
    m_shouldStop = false;
      // 启动音频播放
    m_audioPlayer.Start();
    
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
        }        if (m_packet->stream_index == m_videoStreamIndex)
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
                    
                    // 设置视频时钟作为主时钟
                    m_audioPlayer.SetVideoTime(m_currentTime);
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
        }        else if (m_audioPlayer.IsInitialized() && m_packet->stream_index == m_audioPlayer.GetAudioStreamIndex())
        {
            // 处理音频帧
            ret = avcodec_send_packet(m_audioPlayer.GetAudioCodecContext(), m_packet);
            if (ret == 0)
            {
                AVFrame* audioFrame = av_frame_alloc();
                ret = avcodec_receive_frame(m_audioPlayer.GetAudioCodecContext(), audioFrame);
                if (ret == 0)
                {
                    // 使用新的音频处理方法（带音视频同步）
                    m_audioPlayer.ProcessAudioFrame(audioFrame);
                }
                av_frame_free(&audioFrame);
            }
        }
        
        av_packet_unref(m_packet);
    }
    
    m_isPlaying = false;
}

void VideoPlayer::Render()
{
    if (!m_buffer)
        return;
    
    if (m_useD3D9)
    {
        RenderWithD3D9();
    }
    else
    {
        RenderWithGDI();
    }
}

void VideoPlayer::OnResize(int width, int height)
{
    // 安全检查：确保窗口尺寸有效
    if (width <= 0 || height <= 0)
    {
        return;
    }
    
    m_windowWidth = width;
    m_windowHeight = height;
    
    // 如果使用 D3D9，需要重新设置设备
    if (m_useD3D9 && m_d3d9Device)
    {
        // 重新设置 D3D9 设备
        SetupD3D9();
    }
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

void VideoPlayer::CleanupD3D9()
{
    m_d3d9Surface.Reset();
    m_swapChain.Reset();
    m_d3d9Device.Reset();
    m_d3d9.Reset();
}

void VideoPlayer::SetVolume(float volume)
{
    m_audioPlayer.SetVolume(volume);
}

bool VideoPlayer::HasAudio() const
{
    return m_audioPlayer.IsInitialized();
}

void VideoPlayer::SetScalingMode(ScalingMode mode)
{
    m_scalingMode = mode;
}

void VideoPlayer::SetFilter(FilterType filter)
{
    m_currentFilter = filter;
}

void VideoPlayer::CalculateDisplayRect(int& displayWidth, int& displayHeight, int& offsetX, int& offsetY)
{
    // 安全检查：确保窗口和视频尺寸有效
    if (m_windowWidth <= 0 || m_windowHeight <= 0 || m_videoWidth <= 0 || m_videoHeight <= 0)
    {
        displayWidth = 0;
        displayHeight = 0;
        offsetX = 0;
        offsetY = 0;
        return;
    }
    
    switch (m_scalingMode)
    {
    case ScalingMode::FIT_TO_WINDOW:
        {
            // 保持宽高比，适应窗口，用黑边填充
            double scaleX = (double)m_windowWidth / m_videoWidth;
            double scaleY = (double)m_windowHeight / m_videoHeight;
            double scale = (scaleX < scaleY) ? scaleX : scaleY; // 选择较小的缩放比例以确保视频完全显示
            
            // 确保缩放不会产生无效尺寸
            if (scale <= 0.0)
            {
                displayWidth = 0;
                displayHeight = 0;
                offsetX = 0;
                offsetY = 0;
                return;
            }
            
            displayWidth = (int)(m_videoWidth * scale);
            displayHeight = (int)(m_videoHeight * scale);
            
            // 确保计算结果不为负
            if (displayWidth < 0) displayWidth = 0;
            if (displayHeight < 0) displayHeight = 0;
            
            offsetX = (m_windowWidth - displayWidth) / 2;
            offsetY = (m_windowHeight - displayHeight) / 2;
        }
        break;
          case ScalingMode::ORIGINAL_SIZE:
        {
            // 原始尺寸，居中显示
            displayWidth = m_videoWidth;
            displayHeight = m_videoHeight;
            
            // 确保视频不会超出窗口边界
            if (displayWidth > m_windowWidth) {
                displayWidth = m_windowWidth;
            }
            if (displayHeight > m_windowHeight) {
                displayHeight = m_windowHeight;
            }
            
            offsetX = (m_windowWidth - displayWidth) / 2;
            offsetY = (m_windowHeight - displayHeight) / 2;
            
            // 确保偏移不为负
            if (offsetX < 0) offsetX = 0;
            if (offsetY < 0) offsetY = 0;
        }
        break;
    }
}

void VideoPlayer::ApplyFilter(uint8_t* buffer, int width, int height, int bytesPerPixel)
{
    switch (m_currentFilter)
    {
    case FilterType::NONE:
        // 不应用任何滤镜
        break;
    case FilterType::GRAYSCALE:
        ApplyGrayscaleFilter(buffer, width, height, bytesPerPixel);
        break;
    case FilterType::MOSAIC:
        ApplyMosaicFilter(buffer, width, height, bytesPerPixel);
        break;
    }
}

void VideoPlayer::ApplyGrayscaleFilter(uint8_t* buffer, int width, int height, int bytesPerPixel)
{
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int idx = (y * width + x) * bytesPerPixel;
            
            if (bytesPerPixel == 4) // BGRA
            {
                uint8_t b = buffer[idx + 0];
                uint8_t g = buffer[idx + 1];
                uint8_t r = buffer[idx + 2];
                
                // 使用标准灰度公式: 0.299*R + 0.587*G + 0.114*B
                uint8_t gray = (uint8_t)(0.299 * r + 0.587 * g + 0.114 * b);
                
                buffer[idx + 0] = gray; // B
                buffer[idx + 1] = gray; // G
                buffer[idx + 2] = gray; // R
                // Alpha 通道保持不变
            }
            else if (bytesPerPixel == 3) // BGR
            {
                uint8_t b = buffer[idx + 0];
                uint8_t g = buffer[idx + 1];
                uint8_t r = buffer[idx + 2];
                
                uint8_t gray = (uint8_t)(0.299 * r + 0.587 * g + 0.114 * b);
                
                buffer[idx + 0] = gray; // B
                buffer[idx + 1] = gray; // G
                buffer[idx + 2] = gray; // R
            }
        }
    }
}

void VideoPlayer::ApplyMosaicFilter(uint8_t* buffer, int width, int height, int bytesPerPixel)
{
    for (int y = 0; y < height; y += m_mosaicSize)
    {
        for (int x = 0; x < width; x += m_mosaicSize)
        {
            // 计算马赛克块的平均颜色
            int totalR = 0, totalG = 0, totalB = 0;
            int count = 0;
            
            // 采样块内的像素
            for (int dy = 0; dy < m_mosaicSize && (y + dy) < height; dy++)
            {
                for (int dx = 0; dx < m_mosaicSize && (x + dx) < width; dx++)
                {
                    int idx = ((y + dy) * width + (x + dx)) * bytesPerPixel;
                    
                    if (bytesPerPixel == 4) // BGRA
                    {
                        totalB += buffer[idx + 0];
                        totalG += buffer[idx + 1];
                        totalR += buffer[idx + 2];
                    }
                    else if (bytesPerPixel == 3) // BGR
                    {
                        totalB += buffer[idx + 0];
                        totalG += buffer[idx + 1];
                        totalR += buffer[idx + 2];
                    }
                    count++;
                }
            }
            
            if (count > 0)
            {
                uint8_t avgR = totalR / count;
                uint8_t avgG = totalG / count;
                uint8_t avgB = totalB / count;
                
                // 将平均颜色应用到整个块
                for (int dy = 0; dy < m_mosaicSize && (y + dy) < height; dy++)
                {
                    for (int dx = 0; dx < m_mosaicSize && (x + dx) < width; dx++)
                    {
                        int idx = ((y + dy) * width + (x + dx)) * bytesPerPixel;
                        
                        buffer[idx + 0] = avgB; // B
                        buffer[idx + 1] = avgG; // G
                        buffer[idx + 2] = avgR; // R
                    }
                }
            }
        }
    }
}

void VideoPlayer::RenderWithD3D9()
{
    if (!m_d3d9Device)
    {
        std::cerr << "D3D9 device is null" << std::endl;
        return;
    }
    
    if (!m_buffer)
    {
        std::cerr << "Video buffer is null" << std::endl;
        return;
    }
    
    // 应用滤镜到缓冲区
    ApplyFilter(m_buffer, m_videoWidth, m_videoHeight, 4);
    
    // 获取后备缓冲区
    ComPtr<IDirect3DSurface9> backBuffer;
    HRESULT hr = m_d3d9Device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, backBuffer.GetAddressOf());
    if (FAILED(hr))
    {
        std::cerr << "Failed to get back buffer, HRESULT: 0x" << std::hex << hr << std::endl;
        return;
    }
    
    // 使用新的缩放计算
    int displayWidth, displayHeight, offsetX, offsetY;
    CalculateDisplayRect(displayWidth, displayHeight, offsetX, offsetY);
    
    // 如果计算的尺寸无效，直接返回
    if (displayWidth <= 0 || displayHeight <= 0)
    {
        return;
    }
    
    // 清除后备缓冲区为黑色
    m_d3d9Device->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
    
    // 如果有离屏表面可用，使用它进行高质量缩放
    if (m_d3d9Surface)
    {
        // 锁定离屏表面并更新视频数据
        D3DLOCKED_RECT lockedRect;
        hr = m_d3d9Surface->LockRect(&lockedRect, nullptr, 0);
        if (SUCCEEDED(hr))
        {
            // 将视频数据复制到离屏表面
            uint8_t* srcPtr = m_buffer;
            uint8_t* dstPtr = (uint8_t*)lockedRect.pBits;
            
            for (int y = 0; y < m_videoHeight; y++)
            {
                memcpy(dstPtr + y * lockedRect.Pitch, srcPtr + y * m_videoWidth * 4, m_videoWidth * 4);
            }
            
            m_d3d9Surface->UnlockRect();
            
            // 使用 StretchRect 进行高质量缩放
            RECT srcRect = { 0, 0, m_videoWidth, m_videoHeight };
            RECT dstRect = { offsetX, offsetY, offsetX + displayWidth, offsetY + displayHeight };
            
            hr = m_d3d9Device->StretchRect(m_d3d9Surface.Get(), &srcRect, backBuffer.Get(), &dstRect, D3DTEXF_LINEAR);
            if (FAILED(hr))
            {
                std::cerr << "Failed to stretch rect, HRESULT: 0x" << std::hex << hr << std::endl;
            }
        }
        else
        {
            std::cerr << "Failed to lock offscreen surface" << std::endl;
        }
    }
    else
    {
        // 回退到直接像素拷贝的方法（原有实现）
        D3DLOCKED_RECT lockedRect;
        hr = backBuffer->LockRect(&lockedRect, nullptr, D3DLOCK_DISCARD);
        if (FAILED(hr))
        {
            std::cerr << "Failed to lock back buffer" << std::endl;
            return;
        }
        
        // 清除背景为黑色
        if (m_windowHeight > 0)
        {
            memset(lockedRect.pBits, 0, lockedRect.Pitch * m_windowHeight);
        }
        
        // 使用双线性插值进行软件缩放（比最近邻插值质量更好）
        if (m_videoWidth > 0 && m_videoHeight > 0 && m_buffer && 
            offsetX >= 0 && offsetY >= 0 && 
            offsetX + displayWidth <= m_windowWidth && 
            offsetY + displayHeight <= m_windowHeight)
        {
            uint8_t* srcPtr = m_buffer;
            uint8_t* dstPtr = (uint8_t*)lockedRect.pBits + offsetY * lockedRect.Pitch + offsetX * 4;
            
            // 计算缩放比例
            double scaleX = (double)m_videoWidth / displayWidth;
            double scaleY = (double)m_videoHeight / displayHeight;
            
            // 双线性插值缩放
            for (int y = 0; y < displayHeight; y++)
            {
                double srcY = y * scaleY;
                int srcY1 = (int)srcY;
                int srcY2 = (srcY1 + 1 < m_videoHeight) ? srcY1 + 1 : m_videoHeight - 1;
                double deltaY = srcY - srcY1;
                
                uint8_t* dstLinePtr = dstPtr + y * lockedRect.Pitch;
                
                for (int x = 0; x < displayWidth; x++)
                {
                    double srcX = x * scaleX;
                    int srcX1 = (int)srcX;
                    int srcX2 = (srcX1 + 1 < m_videoWidth) ? srcX1 + 1 : m_videoWidth - 1;
                    double deltaX = srcX - srcX1;
                    
                    // 获取四个相邻像素
                    uint8_t* p11 = srcPtr + (srcY1 * m_videoWidth + srcX1) * 4;
                    uint8_t* p21 = srcPtr + (srcY1 * m_videoWidth + srcX2) * 4;
                    uint8_t* p12 = srcPtr + (srcY2 * m_videoWidth + srcX1) * 4;
                    uint8_t* p22 = srcPtr + (srcY2 * m_videoWidth + srcX2) * 4;
                    
                    // 双线性插值计算每个颜色分量
                    for (int c = 0; c < 3; c++) // B, G, R
                    {
                        double top = p11[c] * (1.0 - deltaX) + p21[c] * deltaX;
                        double bottom = p12[c] * (1.0 - deltaX) + p22[c] * deltaX;
                        double result = top * (1.0 - deltaY) + bottom * deltaY;
                        // 手动实现 clamp 功能
                        int clampedResult = (int)result;
                        if (clampedResult < 0) clampedResult = 0;
                        if (clampedResult > 255) clampedResult = 255;
                        dstLinePtr[x * 4 + c] = (uint8_t)clampedResult;
                    }
                    dstLinePtr[x * 4 + 3] = 255; // Alpha
                }
            }
        }
        
        backBuffer->UnlockRect();
    }
    
    // 呈现到屏幕
    hr = m_d3d9Device->Present(nullptr, nullptr, nullptr, nullptr);
    if (FAILED(hr))
    {
        std::cerr << "Failed to present frame" << std::endl;
    }
}

void VideoPlayer::RenderWithGDI()
{
    if (!m_buffer || !m_hwnd)
        return;

    HDC hdc = GetDC(m_hwnd);
    if (!hdc)
        return;

    int displayWidth, displayHeight, offsetX, offsetY;
    CalculateDisplayRect(displayWidth, displayHeight, offsetX, offsetY);

    // 安全检查：如果计算出的显示尺寸无效，则不进行渲染
    if (displayWidth <= 0 || displayHeight <= 0)
    {
        // 可以选择填充整个窗口为黑色
        RECT clientRect;
        GetClientRect(m_hwnd, &clientRect);
        FillRect(hdc, &clientRect, (HBRUSH)GetStockObject(BLACK_BRUSH));
        ReleaseDC(m_hwnd, hdc);
        return;
    }

    // 更新GDI位图，如果视频帧已准备好
    // 注意：m_frameRGB->data[0] 应该包含最新的视频帧数据
    // UpdateBitmap() 应该使用 m_frameRGB->data[0] 来更新 m_hBitmap
    // 这里假设 UpdateBitmap 内部会处理 m_frameRGB 的有效性
    // 并且 m_buffer 是指向 m_frameRGB->data[0] 的指针，或者 UpdateBitmap 使用 m_frameRGB
    
    // 确保 m_frameRGB 和其数据有效
    if (!m_frameRGB || !m_frameRGB->data[0]) {
        ReleaseDC(m_hwnd, hdc);
        return;
    }
    
    // 更新GDI位图 (假设 UpdateBitmap 使用 m_frameRGB->data[0])
    // 或者直接在这里准备数据
    // 为了清晰，我们假设 UpdateBitmap 负责从 m_frameRGB 更新 m_hBitmap
    // 如果 UpdateBitmap 使用 m_buffer，确保 m_buffer 是最新的
    // UpdateBitmap(); // 这行可能需要，取决于 UpdateBitmap 的实现

    // 如果 UpdateBitmap 还没有被调用，或者需要在这里直接操作数据
    // 我们需要确保 m_hBitmap 是最新的
    // 这里我们假设 m_frameRGB->data[0] 包含了解码和滤镜处理后的数据
    // 并且 m_bitmapInfo 描述的是这个数据的格式 (BGRA)

    // 填充黑边
    RECT clientRect;
    GetClientRect(m_hwnd, &clientRect);

    // 绘制顶部黑边
    if (offsetY > 0)
    {
        RECT topBlackBar = { 0, 0, clientRect.right, offsetY };
        FillRect(hdc, &topBlackBar, (HBRUSH)GetStockObject(BLACK_BRUSH));
    }
    // 绘制底部黑边
    if (offsetY + displayHeight < clientRect.bottom)
    {
        RECT bottomBlackBar = { 0, offsetY + displayHeight, clientRect.right, clientRect.bottom };
        FillRect(hdc, &bottomBlackBar, (HBRUSH)GetStockObject(BLACK_BRUSH));
    }
    // 绘制左边黑边
    if (offsetX > 0)
    {
        RECT leftBlackBar = { 0, offsetY, offsetX, offsetY + displayHeight };
        FillRect(hdc, &leftBlackBar, (HBRUSH)GetStockObject(BLACK_BRUSH));
    }
    // 绘制右边黑边
    if (offsetX + displayWidth < clientRect.right)
    {
        RECT rightBlackBar = { offsetX + displayWidth, offsetY, clientRect.right, offsetY + displayHeight };
        FillRect(hdc, &rightBlackBar, (HBRUSH)GetStockObject(BLACK_BRUSH));
    }
    
    // 创建或更新GDI位图
    if (!m_hBitmap || m_bitmapInfo.bmiHeader.biWidth != m_videoWidth || m_bitmapInfo.bmiHeader.biHeight != -m_videoHeight)
    {
        CleanupGDI(); // 清理旧的GDI资源
        if (!SetupGDI()) // 重新创建GDI资源以匹配视频尺寸
        {
            ReleaseDC(m_hwnd, hdc);
            return; // 如果创建失败则返回
        }
    }

    // 将视频帧数据复制到GDI位图的缓冲区
    // m_frameRGB->data[0] 包含 BGRA 数据
    // m_bitmapInfo.bmiHeader.biHeight 是负数，表示顶向下位图
    // SetDIBitsToDevice 直接使用视频帧数据进行绘制，如果视频尺寸和目标区域尺寸一致
    // StretchDIBits 用于缩放绘制
    
    // 将 m_frameRGB->data[0] 的内容设置到 m_hBitmap
    // 注意：m_frameRGB 的行大小可能与 GDI 位图期望的不同，需要小心处理
    // SetBitmapBits(m_hBitmap, m_videoHeight * m_videoWidth * 4, m_frameRGB->data[0]); // 这是一个简化，实际可能需要按行拷贝

    // 使用 StretchDIBits 进行缩放和绘制
    // m_frameRGB->data[0] 包含的是原始视频尺寸的图像数据
    // 我们需要将其缩放到 displayWidth, displayHeight 并绘制到 (offsetX, offsetY)
    
    // 确保 m_swsContext 用于将解码帧转换为 m_frameRGB (AV_PIX_FMT_BGRA)
    // 并且 m_frameRGB->linesize[0] 是正确的行字节数

    if (m_hBitmap && m_frameRGB && m_frameRGB->data[0])
    {
        // 更新位图数据
        SetDIBits(m_hdcMem, m_hBitmap, 0, m_videoHeight, m_frameRGB->data[0], &m_bitmapInfo, DIB_RGB_COLORS);
          // 使用 StretchBlt 进行缩放绘制
        // 从内存DC (m_hdcMem) 中的位图 (m_hBitmap) 绘制到窗口DC (hdc)
        SetStretchBltMode(hdc, HALFTONE); // 使用 HALFTONE 模式以获得更好的抗锯齿效果
        SetBrushOrgEx(hdc, 0, 0, nullptr); // HALFTONE 模式需要设置画刷原点
        StretchBlt(hdc, offsetX, offsetY, displayWidth, displayHeight,
            m_hdcMem, 0, 0, m_videoWidth, m_videoHeight, SRCCOPY);
    }

    ReleaseDC(m_hwnd, hdc);
}
