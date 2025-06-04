# FFmpeg 视频播放器

这是一个基于 C++ 和 Win32 API 的视频播放器，使用 FFmpeg 库进行视频解码、音频解码和播放，并支持 GDI 和 Direct3D 9 渲染。

## 📁 项目结构

```
Advanced Program Design Term Work/
├── src/                        # 源代码目录
│   ├── main.cpp                # 主程序文件 - Win32 窗口和事件处理
│   ├── VideoPlayer.h           # 视频播放器头文件
│   ├── VideoPlayer.cpp         # 视频播放器实现 - FFmpeg 集成, 渲染逻辑
│   ├── AudioPlayer.h           # 音频播放器头文件
│   ├── AudioPlayer.cpp         # 音频播放器实现 - WASAPI 音频播放与同步
│   ├── ProgressBar.h           # 进度条控件头文件
│   ├── ProgressBar.cpp         # 进度条控件实现 (双缓冲, 时间显示, 自动隐藏)
│   ├── ControlPanel.h          # 控制面板头文件
│   └── ControlPanel.cpp        # 控制面板实现 (音频偏移, 音量, 马赛克大小)
├── demo_video/                 # 示例视频文件
│   ├── 2.mp4                   # 测试视频文件
│   └── test.mp4                # 测试视频文件
├── ffmpeg-master-latest-win64-gpl-shared/  # FFmpeg 库文件
│   ├── bin/                    # 动态链接库 (DLL)
│   ├── include/                # 头文件
│   └── lib/                    # 静态链接库
├── build/                      # 编译输出目录 (自动创建)
│   ├── VideoPlayer.exe         # 编译后的可执行文件
│   └── *.dll                   # FFmpeg 运行时库
├── BuildVS2022.bat             # Visual Studio 2022 编译脚本 ⭐
└── README.md                   # 项目说明文档
```

## ✨ 功能特性

- 🎥 支持多种视频格式 (MP4, AVI, MKV, MOV, WMV, FLV 等)
- 🔊 **现代WASAPI音频系统** - 高性能低延迟音频播放，支持自动格式转换和高级音视频同步
- ⏯️ 基本播放控制 (播放、暂停、停止、跳转)
- ✨ **硬件加速反锯齿** - D3D9 MSAA多重采样抗锯齿 (8x/4x/2x)
- 🎯 **高质量视频缩放** - 双线性插值和硬件过滤，解决高清视频像素化问题
- 📊 **增强型播放进度条** - 时间显示、拖动跳转、鼠标悬停显示与自动隐藏、双缓冲平滑绘制
- 📐 智能视频缩放模式 (优先匹配窗口长边，保持宽高比，黑边填充)
- 🎨 视频滤镜效果 (黑白、马赛克 - 大小可调)
- ⚙️ **F6控制面板** - 实时调整音频偏移、音量、马赛克大小
- ⌨️ 丰富的键盘快捷键支持
- 🎨 简洁友好的用户界面
- 🧵 多线程架构确保 UI 响应性
- 🖼️ 可选 GDI 或 Direct3D 9 渲染后端(待实现)

## 🛠️ 技术栈

- **编程语言**: C++
- **图形界面**: Win32 API (自定义控件: ProgressBar, ControlPanel)
- **音频播放**: WASAPI (Windows Audio Session API) - 现代低延迟音频系统，高级音视频同步
- **图形API**: GDI, Direct3D 9 with MSAA Anti-aliasing
- **编译器**: Visual Studio 2022 (MSVC)
- **平台**: Windows 10/11

## 🚀 快速开始

### 系统要求
- Windows 10/11 操作系统
- Visual Studio 2022 (已安装在 `D:\\Programs\\VisualStudio`)
- FFmpeg 动态链接库 (已包含)
- Direct3D 9 运行时 (通常随 Windows 系统或 DirectX 更新安装)

### 编译步骤

1. **一键编译** (推荐)
   ```cmd
   双击运行 BuildVS2022.bat
   ```
   *注意: `BuildVS2022.bat` 脚本已更新，包含 d3d9.lib 和 comctl32.lib 的链接。*

2. **手动编译**
   ```powershell
   # 设置 Visual Studio 环境
   call "D:\\Programs\\VisualStudio\\VC\\Auxiliary\\Build\\vcvars64.bat"
   
   # 定义环境变量 (根据你的实际路径修改)
   $env:FFMPEG_DIR = "d:\\Web_Project\\Advanced Program Design Term Work\\ffmpeg-master-latest-win64-gpl-shared"
   $env:SRC_DIR = "d:\\Web_Project\\Advanced Program Design Term Work\\src"
   $env:BUILD_DIR = "d:\\Web_Project\\Advanced Program Design Term Work\\build"

   # 创建build目录 (如果不存在)
   if (-not (Test-Path $env:BUILD_DIR)) { New-Item -ItemType Directory -Path $env:BUILD_DIR }

   # 编译项目
   cl /EHsc /MD /O2 /W3 /DWIN32 /D_WINDOWS /DNDEBUG \`
       /I"$env:FFMPEG_DIR\\include" \`
       "$env:SRC_DIR\\main.cpp" "$env:SRC_DIR\\VideoPlayer.cpp" "$env:SRC_DIR\\AudioPlayer.cpp" "$env:SRC_DIR\\ProgressBar.cpp" "$env:SRC_DIR\\ControlPanel.cpp" \`
       /Fe:"$env:BUILD_DIR\\VideoPlayer.exe" \`
       /link /LIBPATH:"$env:FFMPEG_DIR\\lib" \`
       avformat.lib avcodec.lib avutil.lib swscale.lib swresample.lib \`
       user32.lib gdi32.lib comdlg32.lib kernel32.lib winmm.lib d3d9.lib ole32.lib oleaut32.lib comctl32.lib
   
   # 复制 DLL 文件
   Copy-Item "$env:FFMPEG_DIR\\bin\\*.dll" "$env:BUILD_DIR\\"
   ```

### 运行程序

```powershell
cd build
.\\VideoPlayer.exe
```

## 📖 使用说明

### 菜单操作
- **File → Open Video...**: 选择并打开视频文件
- **Playback → Play**: 开始播放
- **Playback → Pause**: 暂停播放
- **Playback → Stop**: 停止播放
- **Scaling → Fit to Window**: 视频适应窗口大小，保持宽高比并填充黑边
- **Scaling → Original Size**: 视频按原始尺寸显示
- **Filter → None**: 关闭滤镜
- **Filter → Grayscale**: 应用黑白滤镜
- **Filter → Mosaic**: 应用马赛克滤镜 (大小可通过F6控制面板调节)

### 键盘快捷键
| 按键 | 功能 |
|------|------|
| `空格` | 播放/暂停切换 |
| `ESC` | 停止播放 |
| `←` | 后退 5 秒 |
| `→` | 前进 5 秒 |
| `F6` | 切换控制面板 (音频偏移, 音量, 马赛克大小) |
| `G`  | 切换黑白滤镜 (如果实现为快捷键) |
| `M`  | 切换马赛克滤镜 (如果实现为快捷键) |
| `S`  | 切换缩放模式 (如果实现为快捷键) |
| `F1` | (示例) 适应窗口缩放模式 |
| `F2` | (示例) 原始尺寸缩放模式 |
| `1/2/3`| (示例) 切换滤镜 (无/灰度/马赛克) |


## 🔧 技术实现详解

### 核心架构

#### 1. VideoPlayer 类
- **职责**: 封装 FFmpeg 视频解码、渲染逻辑 (GDI/D3D9)、滤镜应用和播放控制。集成实验性硬件加速解码框架。
- **特性**: 
  - 自动管理 FFmpeg 资源生命周期
  - 多线程视频解码
  - 像素格式转换 (YUV → BGRA32 for D3D9/GDI), 支持硬件帧到软件帧的转换
  - 帧率控制和与音频的精确同步
  - 滤镜处理 (包括可调马赛克大小)

#### 2. AudioPlayer 类
- **职责**: 封装 FFmpeg 音频解码和使用现代 WASAPI 进行低延迟音频播放，实现高级音视频同步。
- **特性**:
  - 音频流解码与 FLTP (Float Planar) 格式处理
  - WASAPI 音频设备初始化和自动格式转换
  - 低延迟缓冲区管理和音频数据流式传输
  - 基于视频主时钟的音频样本补偿和同步算法
  - 可调音频偏移量

#### 3. ProgressBar 类
- **职责**: 实现一个可交互的、功能增强的进度条控件。
- **特性**:
  - **双缓冲绘制**: 减少闪烁，提供平滑视觉效果。
  - **时间显示**: 显示当前播放时间和视频总时长。
  - **自动隐藏**: 鼠标移开后5秒自动隐藏，悬停时重新显示。
  - UI绘制，鼠标点击/拖动事件处理，回调机制通知主程序跳转请求。

#### 4. ControlPanel 类
- **职责**: 提供一个浮动窗口，用于实时调整播放器参数。
- **特性**:
  - 使用标准Win32控件 (Trackbar, Static text) 构建用户界面。
  - 实时调整音频偏移量、音量和马赛克大小。
  - 通过回调机制将参数变更通知给主播放器逻辑。
  - F6快捷键触发显示/隐藏。

#### 5. Win32 窗口系统
- **窗口管理**: 创建主窗口、控制面板窗口，处理窗口消息。
- **菜单系统**: 文件操作、播放控制、缩放、滤镜等菜单。
- **事件处理**: 键盘输入 (包括F6等快捷键)、鼠标操作、窗口调整。
- **渲染调用**: 根据选择调用 GDI 或 Direct3D 9 渲染方法。

#### 6. 多线程设计
```
主线程 (UI)          播放线程 (Decode & Sync)  音频播放线程 (AudioPlayer internal)
    │                      │                          │
    ├─ 窗口消息处理        ├─ 视频/音频帧解码         └─ 音频数据提交给声卡 (WASAPI)
    ├─ 用户交互响应        ├─ (实验性)硬件帧处理      ├─ 音频样本补偿与同步
    ├─ 图像渲染 (GDI/D3D9) ├─ 视频帧应用滤镜
    ├─ 菜单/进度条/控制面板操作 ├─ 音视频同步 (基于主时钟)
    └─ 播放状态管理        ├─ 帧率控制
                           └─ 更新当前播放时间
```

### 关键技术点

#### 1. FFmpeg 集成 (音视频与实验性硬件加速)
```cpp
// 视频文件打开和解码器初始化
avformat_open_input(&m_formatContext, videoPath.c_str(), nullptr, nullptr);
// ... 寻找视频和音频流，初始化对应解码器 ...
avcodec_find_decoder(codecPar->codec_id);
avcodec_open2(m_codecContext, m_codec, nullptr);
// ... 音频类似 ...

// 尝试初始化硬件设备上下文 (示例: DXVA2)
// enum AVHWDeviceType type = AV_HWDEVICE_TYPE_DXVA2; // 或 CUDA, QSV, D3D11VA
// av_hwdevice_ctx_create(&m_hwDeviceCtx, type, nullptr, nullptr, 0);
// if (m_hwDeviceCtx) {
//     avcodec_get_hw_config(m_codec, i); // 检查硬件像素格式
//     m_codecContext->hw_device_ctx = av_buffer_ref(m_hwDeviceCtx);
// }
```

#### 2. 像素格式转换与滤镜
```cpp
// 使用 SwScale 进行 YUV 到 BGRA 转换，现在使用高质量 SWS_BICUBIC 缩放
m_swsContext = sws_getContext(
    m_videoWidth, m_videoHeight, m_codecContext->pix_fmt,
    m_videoWidth, m_videoHeight, AV_PIX_FMT_BGRA, // 目标格式
    SWS_BICUBIC, nullptr, nullptr, nullptr  // 高质量双三次插值缩放
);
// 马赛克滤镜应用时，根据 m_mosaicSize 调整块大小
// ... ApplyFilter 调用具体滤镜函数直接修改 BGRA 数据 ...
```

#### 3. Win32 GDI 渲染（带反锯齿）
```cpp
// 创建 DIB 位图并使用 StretchBlt 渲染到窗口，启用 HALFTONE 抗锯齿
SetStretchBltMode(hdc, HALFTONE);  // 启用高质量拉伸模式，减少锯齿
SetDIBits(m_hdcMem, m_hBitmap, 0, m_videoHeight, m_frameRGB->data[0], &m_bitmapInfo, DIB_RGB_COLORS);
StretchBlt(hdc, offsetX, offsetY, displayWidth, displayHeight,
           m_hdcMem, 0, 0, m_videoWidth, m_videoHeight, SRCCOPY);
// ProgressBar 使用双缓冲: CreateCompatibleDC, CreateCompatibleBitmap, BitBlt
```

#### 4. Direct3D 9 渲染（带硬件反锯齿）
```cpp
// 初始化 D3D9 设备和交换链，启用 MSAA 多重采样抗锯齿
Direct3DCreate9(D3D_SDK_VERSION);
// 检测并启用 MSAA 支持 (8x -> 4x -> 2x 降级)
D3DPRESENT_PARAMETERS presentParams = {};
presentParams.MultiSampleType = D3DMULTISAMPLE_8_SAMPLES;  // 尝试8x MSAA
if (FAILED(m_d3d9->CreateDevice(..., &presentParams, ...))) {
    presentParams.MultiSampleType = D3DMULTISAMPLE_4_SAMPLES;  // 降级到4x
    if (FAILED(m_d3d9->CreateDevice(...))) {
        presentParams.MultiSampleType = D3DMULTISAMPLE_2_SAMPLES;  // 最低2x
    }
}

// 渲染循环中：
m_d3d9Device->BeginScene();
// 使用硬件过滤的 StretchRect 替代简单复制，减少像素化
m_d3d9Device->StretchRect(m_videoSurface, nullptr, m_renderSurface, &destRect, 
                          D3DTEXF_LINEAR);  // 硬件双线性过滤
// 软件双线性插值回退（如果硬件StretchRect失败）
// if (FAILED(hr)) { 
//     ApplyBilinearInterpolation(sourceData, targetData, srcWidth, srcHeight, 
//                               destWidth, destHeight); 
// }
m_d3d9Device->EndScene();
m_d3d9Device->Present(nullptr, nullptr, m_hwnd, nullptr);
```

#### 5. 音频播放 (WASAPI 与高级同步)
```cpp
// 初始化 WASAPI 音频设备
HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, 
                             CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&m_deviceEnumerator);
hr = m_deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_device);
hr = m_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&m_audioClient);

// 配置音频格式和缓冲区
WAVEFORMATEX* mixFormat;
hr = m_audioClient->GetMixFormat(&mixFormat);
hr = m_audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST, 
                              10000000, 0, mixFormat, nullptr); // 1秒缓冲, 事件回调

// 获取渲染客户端
hr = m_audioClient->GetService(__uuidof(IAudioRenderClient), (void**)&m_renderClient);

// 处理 FLTP 音频数据
void WriteFLTP(const float* leftChannel, const float* rightChannel, int frames) {
    // 获取可用缓冲区并写入交错音频数据
    BYTE* buffer;
    hr = m_renderClient->GetBuffer(frames, &buffer);
    // 转换 FLTP -> 交错格式并写入 WASAPI 缓冲区
    hr = m_renderClient->ReleaseBuffer(frames, 0);
}

// 音视频同步核心逻辑 (AudioPlayer::SynchronizeAudio)
// double diff = m_videoClock - m_audioClock - m_audioOffset;
// if (fabs(avg_diff) >= m_audioDiffThreshold) {
//     wanted_nb_samples = nb_samples + (int)(diff * m_audioCodecContext->sample_rate);
//     // ... 限制调整幅度 ...
//     swr_set_compensation(m_swrContext, ...); // 通过重采样调整音频速度
// }
// m_audioClock += (double)wanted_nb_samples / m_audioCodecContext->sample_rate;
```

#### 6. 自定义控件 (ProgressBar, ControlPanel)
- **ProgressBar**: `WM_PAINT`中实现双缓冲绘制，`WM_MOUSEMOVE`, `WM_LBUTTONDOWN`, `WM_LBUTTONUP`处理交互和自动隐藏逻辑。
- **ControlPanel**: `DialogProc`或`WndProc`处理消息，`WM_HSCROLL`响应Trackbar事件，通过回调函数更新`VideoPlayer`和`AudioPlayer`的参数。

#### 7. 内存管理与同步
- **RAII 原则**: 构造函数分配资源，析构函数释放资源。
- **线程同步**: 使用事件、互斥锁等确保线程安全访问共享数据。
- **资源清理**: 分别管理 FFmpeg、GDI、D3D9、WASAPI和自定义控件资源。

## 🧪 测试

项目包含测试视频文件:
- `demo_video/2.mp4` - H.264 编码测试视频
- `demo_video/test.mp4` - H.264 编码测试视频

## 🔍 故障排除

### 编译问题

**问题**: `fatal error C1083: 无法打开包括文件`
**解决**: 确保 Visual Studio 环境正确设置，运行 `BuildVS2022.bat`

**问题**: `无法找到链接库 (如 d3d9.lib, comctl32.lib)`
**解决**: 检查 FFmpeg 库文件是否完整存在，确保 `BuildVS2022.bat` 或手动编译命令中包含了所有必需的库。

### 运行问题

**问题**: 程序启动失败 (DLL 缺失)
**解决**: 确保 `build` 目录包含所有 FFmpeg DLL 文件 (`avcodec-XX.dll`, `avformat-XX.dll` 等)。

**问题**: 无法播放视频或没有声音
**解决**: 
1. 确认视频文件格式受支持且文件未损坏。
2. 检查系统音频设备是否正常。
3. 尝试使用提供的测试视频文件。
4. 如果使用 D3D9 渲染黑屏，检查显卡驱动和 DirectX 版本。

**问题**: 缩小窗口时程序崩溃
**解决**: 确保渲染逻辑 (GDI 和 D3D9) 和尺寸计算 (`CalculateDisplayRect`, `OnResize`) 能够正确处理零或负尺寸，并有充分的边界检查。

**问题**: 控制面板不显示或功能异常
**解决**: 确保`comctl32.lib`已链接，`InitCommonControlsEx`已调用，窗口类已正确注册，消息处理正确。

**问题**: 音视频不同步或音频卡顿
**解决**: 尝试使用F6控制面板调整音频偏移量。检查控制台是否有关于音视频同步的调试输出。

## 🆕 最新功能更新 (v1.2)

### 🎨 控制与交互增强
- **F6控制面板**: 新增浮动控制面板，可实时调整：
    - **音频偏移量**: 精细校准音画同步。
    - **音量**: 方便快捷的音量控制。
    - **马赛克大小**: 自定义马赛克滤镜的颗粒度。
- **增强型进度条**:
    - **时间显示**: 清晰展示当前播放进度和总时长。
    - **自动隐藏**: 界面更清爽，鼠标悬停时即时响应。
    - **双缓冲优化**: 彻底告别闪烁，操作更流畅。

### 🎧 音视频核心优化
- **高级音视频同步**: 改进了基于视频主时钟的音频同步算法，减少卡顿和不同步现象。
- **(实验性)硬件加速解码框架**:
    - 为 FFmpeg 的硬件加速解码 (DXVA2, D3D11VA, QSV, CUDA) 集成了基础支持框架。
    - 目标是利用GPU处理视频解码，降低CPU负载，提升高分辨率视频播放性能。
    - *完整启用依赖用户本地硬件和SDK环境配置。*

### ✨ 其他改进
- **代码结构优化**: 引入`ControlPanel`类，模块化管理UI交互逻辑。
- **构建脚本更新**: 自动链接`comctl32.lib`以支持控制面板控件。
- **WASAPI事件回调**: 为音频播放添加了事件回调机制，为更精确的缓冲管理和低延迟响应打下基础。

## 🔮 未来扩展

- [ ] 🔍 添加全屏播放模式
- [ ] 📝 支持字幕文件显示 (如 .srt, .ass)
- [ ] 📋 实现播放列表功能
- [ ] ℹ️ 添加视频详细信息显示面板 (分辨率, 编码, 帧率, 比特率等)
- [ ] 🎨 进一步改进用户界面设计 (例如使用更现代的UI库或自定义绘制控件)
- [ ] ⚡ **完善硬件加速解码**: 稳定并全面支持 DXVA2, D3D11VA, QSV, CUDA，并提供用户可选的加速方式。
- [ ] ⚙️ 更高级的渲染选项 (如自定义着色器，色彩空间转换)
- [ ] 💾 保存用户设置 (如音量、音频偏移默认值)
- [ ] 🎧 **WASAPI低延迟优化**: 基于事件回调进一步优化音频缓冲策略，实现更低的播放延迟。

## 📄 许可证

本项目使用 FFmpeg GPL 许可证。请确保遵守相关的开源许可证条款。

---

**开发迭代至**: 2025年6月4日  
**编译器**: Visual Studio 2022 MSVC 19.36.32532  
**FFmpeg 版本**: master-latest-win64-gpl-shared