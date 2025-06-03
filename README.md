# FFmpeg 视频播放器

这是一个基于 C++ 和 Win32 API 的简单视频播放器，使用 FFmpeg 库进行视频解码和播放。

## 📁 项目结构

```
Advanced Program Design Term Work/
├── src/                        # 源代码目录
│   ├── main.cpp                # 主程序文件 - Win32 窗口和事件处理
│   ├── VideoPlayer.h           # 视频播放器头文件
│   └── VideoPlayer.cpp         # 视频播放器实现 - FFmpeg 集成
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
- ⏯️ 基本播放控制 (播放、暂停、停止)
- 🖼️ 实时视频帧渲染到 Win32 窗口
- 📐 自动保持视频宽高比的缩放显示
- ⌨️ 丰富的键盘快捷键支持
- 🎨 简洁友好的用户界面
- 🧵 多线程架构确保 UI 响应性

## 🛠️ 技术栈

- **编程语言**: C++
- **图形界面**: Win32 API
- **视频解码**: FFmpeg
- **编译器**: Visual Studio 2022 (MSVC)
- **平台**: Windows 10/11

## 🚀 快速开始

### 系统要求
- Windows 10/11 操作系统
- Visual Studio 2022 (已安装在 `D:\Programs\VisualStudio`)
- FFmpeg 动态链接库 (已包含)

### 编译步骤

1. **一键编译** (推荐)
   ```cmd
   双击运行 BuildVS2022.bat
   ```

2. **手动编译**
   ```cmd
   # 设置 Visual Studio 环境
   call "D:\Programs\VisualStudio\VC\Auxiliary\Build\vcvars64.bat"
   
   # 编译项目
   cl /EHsc /MD /O2 /W3 /DWIN32 /D_WINDOWS /DNDEBUG ^
       /I"ffmpeg-master-latest-win64-gpl-shared\include" ^
       "src\main.cpp" "src\VideoPlayer.cpp" ^
       /Fe:"build\VideoPlayer.exe" ^
       /link /LIBPATH:"ffmpeg-master-latest-win64-gpl-shared\lib" ^
       avformat.lib avcodec.lib avutil.lib swscale.lib ^
       user32.lib gdi32.lib comdlg32.lib kernel32.lib
   
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

### 键盘快捷键
| 按键 | 功能 |
|------|------|
| `空格` | 播放/暂停切换 |
| `ESC` | 停止播放 |
| `←` | 后退 5 秒 |
| `→` | 前进 5 秒 |

## 🔧 技术实现详解

### 核心架构

#### 1. VideoPlayer 类
- **职责**: 封装 FFmpeg 视频解码和渲染逻辑
- **特性**: 
  - 自动管理 FFmpeg 资源生命周期
  - 多线程视频解码
  - 像素格式转换 (YUV → BGR24)
  - 帧率控制和同步

#### 2. Win32 窗口系统
- **窗口管理**: 创建主窗口、处理窗口消息
- **菜单系统**: 文件操作和播放控制菜单
- **事件处理**: 键盘输入、鼠标操作、窗口调整
- **GDI 渲染**: 将视频帧绘制到窗口

#### 3. 多线程设计
```
主线程 (UI)          播放线程 (Decode)
    │                      │
    ├─ 窗口消息处理        ├─ 视频帧解码
    ├─ 用户交互响应        ├─ 帧率控制
    ├─ GDI 图像渲染        └─ 播放状态管理
    └─ 菜单操作处理
```

### 关键技术点

#### 1. FFmpeg 集成
```cpp
// 视频文件打开和解码器初始化
avformat_open_input(&m_formatContext, videoPath.c_str(), nullptr, nullptr);
avcodec_find_decoder(codecPar->codec_id);
avcodec_open2(m_codecContext, m_codec, nullptr);
```

#### 2. 像素格式转换
```cpp
// 使用 SwScale 进行 YUV 到 BGR24 转换
m_swsContext = sws_getContext(
    m_videoWidth, m_videoHeight, m_codecContext->pix_fmt,
    m_videoWidth, m_videoHeight, AV_PIX_FMT_BGR24,
    SWS_BILINEAR, nullptr, nullptr, nullptr
);
```

#### 3. Win32 GDI 渲染
```cpp
// 创建 DIB 位图并渲染到窗口
m_hBitmap = CreateDIBitmap(hdc, &m_bitmapInfo.bmiHeader, 
                          CBM_INIT, m_buffer, &m_bitmapInfo, DIB_RGB_COLORS);
StretchBlt(hdc, offsetX, offsetY, displayWidth, displayHeight,
           m_hdcMem, 0, 0, m_videoWidth, m_videoHeight, SRCCOPY);
```

#### 4. 内存管理
- **RAII 原则**: 构造函数分配资源，析构函数释放资源
- **异常安全**: 使用智能指针和异常处理
- **资源清理**: 分别管理 FFmpeg 和 GDI 资源

## 🧪 测试

项目包含测试视频文件:
- `demo_video/2.mp4` - H.264 编码测试视频
- `demo_video/test.mp4` - H.264 编码测试视频

## 🔍 故障排除

### 编译问题

**问题**: `fatal error C1083: 无法打开包括文件`
**解决**: 确保 Visual Studio 环境正确设置，运行 `BuildVS2022.bat`

**问题**: `无法找到链接库`
**解决**: 检查 FFmpeg 库文件是否完整存在

### 运行问题

**问题**: 程序启动失败
**解决**: 确保 `build` 目录包含所有 FFmpeg DLL 文件

**问题**: 无法播放视频
**解决**: 
1. 确认视频文件格式受支持
2. 检查视频文件是否损坏
3. 尝试使用提供的测试视频文件

## 🔮 未来扩展

- [ ] 🔊 添加音频播放支持
- [ ] 📊 实现进度条和时间显示
- [ ] 🔍 添加全屏播放模式
- [ ] 📝 支持字幕文件显示
- [ ] 📋 实现播放列表功能
- [ ] ℹ️ 添加视频信息显示面板
- [ ] 🎨 改进用户界面设计
- [ ] ⚡ 硬件加速解码支持

## 📄 许可证

本项目使用 FFmpeg GPL 许可证。请确保遵守相关的开源许可证条款。

---

**开发完成时间**: 2025年6月3日  
**编译器**: Visual Studio 2022 MSVC 19.36.32532  
**FFmpeg 版本**: master-latest-win64-gpl-shared
