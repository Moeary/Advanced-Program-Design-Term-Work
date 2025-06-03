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
│   ├── AudioPlayer.cpp         # 音频播放器实现 - Windows Multimedia API
│   ├── ProgressBar.h           # 进度条控件头文件
│   └── ProgressBar.cpp         # 进度条控件实现
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
├── prompt.md                   # 项目要求说明
└── README.md                   # 项目说明文档
```

## ✨ 功能特性

- 🎥 支持多种视频格式 (MP4, AVI, MKV, MOV, WMV, FLV 等)
- 🔊 音频播放支持，并可调节音量
- ⏯️ 基本播放控制 (播放、暂停、停止、跳转)
- 🖼️ 可选 GDI 或 Direct3D 9 渲染后端
- 📊 播放进度条显示与拖动跳转功能
- 📐 智能视频缩放模式 (优先匹配窗口长边，保持宽高比，黑边填充)
- 🎨 视频滤镜效果 (黑白、马赛克)
- ⌨️ 丰富的键盘快捷键支持
- 🎨 简洁友好的用户界面
- 🧵 多线程架构确保 UI 响应性

## 🛠️ 技术栈

- **编程语言**: C++
- **图形界面**: Win32 API
- **视频/音频解码**: FFmpeg
- **音频播放**: Windows Multimedia API (WaveOut)
- **图形API**: GDI, Direct3D 9
- **编译器**: Visual Studio 2022 (MSVC)
- **平台**: Windows 10/11

## 🚀 快速开始

### 系统要求
- Windows 10/11 操作系统
- Visual Studio 2022 (已安装在 `D:\Programs\VisualStudio`)
- FFmpeg 动态链接库 (已包含)
- Direct3D 9 运行时 (通常随 Windows 系统或 DirectX 更新安装)

### 编译步骤

1. **一键编译** (推荐)
   ```cmd
   双击运行 BuildVS2022.bat
   ```
   *注意: `BuildVS2022.bat` 脚本已更新，包含 d3d9.lib 的链接。*

2. **手动编译**
   ```cmd
   # 设置 Visual Studio 环境
   call "D:\Programs\VisualStudio\VC\Auxiliary\Build\vcvars64.bat"
   
   # 编译项目
   cl /EHsc /MD /O2 /W3 /DWIN32 /D_WINDOWS /DNDEBUG ^
       /I"ffmpeg-master-latest-win64-gpl-shared\include" ^
       "src\main.cpp" "src\VideoPlayer.cpp" "src\AudioPlayer.cpp" "src\ProgressBar.cpp" ^
       /Fe:"build\VideoPlayer.exe" ^
       /link /LIBPATH:"ffmpeg-master-latest-win64-gpl-shared\lib" ^
       avformat.lib avcodec.lib avutil.lib swscale.lib swresample.lib ^
       user32.lib gdi32.lib comdlg32.lib kernel32.lib winmm.lib d3d9.lib
   
   # 复制 DLL 文件
   copy "ffmpeg-master-latest-win64-gpl-shared\bin\*.dll" "build\"
   ```

### 运行程序

```cmd
cd build
VideoPlayer.exe
```

## 📖 使用说明

### 菜单操作
- **File → Open Video...**: 选择并打开视频文件
- **Playback → Play**: 开始播放
- **Playback → Pause**: 暂停播放
- **Playback → Stop**: 停止播放
- **Scaling → Fit to Window**: 视频适应窗口大小，保持宽高比并填充黑边 (PotPlayer类似效果)
- **Scaling → Original Size**: 视频按原始尺寸显示
- **Filter → None**: 关闭滤镜
- **Filter → Grayscale**: 应用黑白滤镜
- **Filter → Mosaic**: 应用马赛克滤镜
- *(可能有用于切换 GDI/D3D9 渲染的菜单，具体取决于实现)*

### 键盘快捷键
| 按键 | 功能 |
|------|------|
| `空格` | 播放/暂停切换 |
| `ESC` | 停止播放 |
| `←` | 后退 5 秒 |
| `→` | 前进 5 秒 |
| `G`  | 切换黑白滤镜 (如果实现) |
| `M`  | 切换马赛克滤镜 (如果实现) |
| `S`  | 切换缩放模式 (如果实现) |

## 🔧 技术实现详解

### 核心架构

#### 1. VideoPlayer 类
- **职责**: 封装 FFmpeg 视频解码、渲染逻辑 (GDI/D3D9)、滤镜应用和播放控制。
- **特性**: 
  - 自动管理 FFmpeg 资源生命周期
  - 多线程视频解码
  - 像素格式转换 (YUV → BGRA32 for D3D9/GDI)
  - 帧率控制和同步 (与音频同步)
  - 滤镜处理

#### 2. AudioPlayer 类
- **职责**: 封装 FFmpeg 音频解码和使用 Windows Multimedia API (WaveOut) 进行播放。
- **特性**:
  - 音频流解码
  - 音频重采样
  - 缓冲区管理和音频设备交互
  - 音量控制

#### 3. ProgressBar 类
- **职责**: 实现一个可交互的进度条控件。
- **特性**:
  - UI绘制
  - 鼠标点击/拖动事件处理
  - 回调机制通知主程序跳转请求

#### 4. Win32 窗口系统
- **窗口管理**: 创建主窗口、处理窗口消息
- **菜单系统**: 文件操作、播放控制、缩放、滤镜等菜单
- **事件处理**: 键盘输入、鼠标操作、窗口调整
- **渲染调用**: 根据选择调用 GDI 或 Direct3D 9 渲染方法

#### 5. 多线程设计
```
主线程 (UI)          播放线程 (Decode & Sync)  音频播放线程 (AudioPlayer internal)
    │                      │                          │
    ├─ 窗口消息处理        ├─ 视频/音频帧解码         └─ 音频数据提交给声卡
    ├─ 用户交互响应        ├─ 视频帧应用滤镜
    ├─ 图像渲染 (GDI/D3D9) ├─ 音视频同步
    ├─ 菜单/进度条操作     ├─ 帧率控制
    └─ 播放状态管理        └─ 更新当前播放时间
```

### 关键技术点

#### 1. FFmpeg 集成 (音视频)
```cpp
// 视频文件打开和解码器初始化
avformat_open_input(&m_formatContext, videoPath.c_str(), nullptr, nullptr);
// ... 寻找视频和音频流，初始化对应解码器 ...
avcodec_find_decoder(codecPar->codec_id);
avcodec_open2(m_codecContext, m_codec, nullptr);
// ... 音频类似 ...
```

#### 2. 像素格式转换与滤镜
```cpp
// 使用 SwScale 进行 YUV 到 BGRA 转换 (适用于D3D9和GDI)
m_swsContext = sws_getContext(
    m_videoWidth, m_videoHeight, m_codecContext->pix_fmt,
    m_videoWidth, m_videoHeight, AV_PIX_FMT_BGRA, // 目标格式
    SWS_BILINEAR, nullptr, nullptr, nullptr
);
// ... ApplyFilter 调用具体滤镜函数直接修改 BGRA 数据 ...
```

#### 3. Win32 GDI 渲染
```cpp
// 创建 DIB 位图并使用 StretchBlt 渲染到窗口，处理黑边
SetDIBits(m_hdcMem, m_hBitmap, 0, m_videoHeight, m_frameRGB->data[0], &m_bitmapInfo, DIB_RGB_COLORS);
StretchBlt(hdc, offsetX, offsetY, displayWidth, displayHeight,
           m_hdcMem, 0, 0, m_videoWidth, m_videoHeight, SRCCOPY);
```

#### 4. Direct3D 9 渲染
```cpp
// 初始化 D3D9 设备和交换链
Direct3DCreate9(D3D_SDK_VERSION);
// ... 创建设备和交换链 ...
// 渲染循环中：
m_d3d9Device->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
m_d3d9Device->BeginScene();
// ... 创建纹理或表面，锁定，拷贝视频帧数据 (BGRA) ...
// ... 使用 StretchRect 将视频表面拉伸到后备缓冲区的目标矩形 ...
m_d3d9Device->StretchRect(videoSurface, nullptr, backBuffer, &destRect, D3DTEXF_LINEAR);
m_d3d9Device->EndScene();
m_d3d9Device->Present(nullptr, nullptr, m_hwnd, nullptr);
```

#### 5. 音频播放 (WaveOut)
```cpp
// 初始化 WAVEFORMATEX
// 打开音频设备 waveOutOpen
// 准备 WAVEHDR 缓冲区，填充解码后的音频数据
// 使用 waveOutWrite 提交缓冲区进行播放
// 通过回调 WaveOutProc 管理缓冲区队列
```

#### 6. 内存管理与同步
- **RAII 原则**: 构造函数分配资源，析构函数释放资源。
- **线程同步**: 使用事件、互斥锁等确保线程安全访问共享数据。
- **资源清理**: 分别管理 FFmpeg、GDI、D3D9 和音频资源。

## 🧪 测试

项目包含测试视频文件:
- `demo_video/2.mp4` - H.264 编码测试视频
- `demo_video/test.mp4` - H.264 编码测试视频

## 🔍 故障排除

### 编译问题

**问题**: `fatal error C1083: 无法打开包括文件`
**解决**: 确保 Visual Studio 环境正确设置，运行 `BuildVS2022.bat`

**问题**: `无法找到链接库 (如 d3d9.lib)`
**解决**: 检查 FFmpeg 库文件是否完整存在，确保 `BuildVS2022.bat` 或手动编译命令中包含了所有必需的库 (`d3d9.lib`, `winmm.lib` 等)。

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

## 🔮 未来扩展

- [ ] 🔍 添加全屏播放模式
- [ ] 📝 支持字幕文件显示
- [ ] 📋 实现播放列表功能
- [ ] ℹ️ 添加视频详细信息显示面板 (分辨率, 编码等)
- [ ] 🎨 进一步改进用户界面设计 (例如使用更现代的UI库或自定义绘制控件)
- [ ] ⚡ FFmpeg 硬件加速解码支持 (如 DXVA2, D3D11VA)
- [ ] ⚙️ 更高级的渲染选项 (如自定义着色器)

## 📄 许可证

本项目使用 FFmpeg GPL 许可证。请确保遵守相关的开源许可证条款。

---

**开发完成时间**: 2025年6月3日  
**编译器**: Visual Studio 2022 MSVC 19.36.32532  
**FFmpeg 版本**: master-latest-win64-gpl-shared