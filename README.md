# mini-player

一个基于 **C++20 + FFmpeg + SDL2** 从零实现的极简音视频播放器。

本项目不是调用现成播放器组件的外壳，而是手写播放器核心流程：解封装、音视频解码、多线程队列、音频播放、视频渲染、音视频同步、暂停、seek、资源释放与调试验证。

项目目标是通过一个可运行的播放器内核，系统学习 C++ 音视频客户端 / 播放器 SDK / 多媒体系统层开发中的核心工程能力。

---

## 当前状态

当前版本已经完成一个基础可用的本地播放器内核。

已完成：

* 本地视频文件打开与流信息解析
* FFmpeg 解封装
* 视频解码
* 音频解码
* 音频重采样
* SDL2 视频渲染
* SDL2 音频播放
* Demuxer / VideoDecoder / AudioDecoder / AudioPlayer / Renderer 模块拆分
* 基于有界阻塞队列的多线程生产者消费者模型
* 音频主时钟驱动的视频同步
* 视频帧等待 / 丢帧策略
* 暂停 / 继续
* 左右方向键 seek
* seek 后 packet queue / frame queue / decoder / audio fifo 清理
* seek 后 audio clock 重置到目标时间，避免同步器误判导致大量丢帧
* RAII 封装 FFmpeg 资源
* ThreadSanitizer 数据竞争检查
* Valgrind Memcheck 内存检查

暂未实现：

* 可视化进度条
* 鼠标点击进度条 seek
* 硬件解码
* 字幕渲染
* 网络流播放
* Windows / Qt 图形界面版本

---

## 技术栈

* 语言：C++20
* 构建系统：CMake
* 音视频库：FFmpeg

  * libavformat
  * libavcodec
  * libavutil
  * libswscale
  * libswresample
* 图形与音频输出：SDL2
* 并发组件：

  * `std::jthread`
  * `std::stop_token`
  * `std::mutex`
  * `std::condition_variable`
  * `std::atomic`
  * 自定义有界阻塞队列
* 调试与验证：

  * GDB
  * ThreadSanitizer
  * AddressSanitizer
  * Valgrind Memcheck

---

## 测试环境

当前主要开发与测试环境：

* Ubuntu 24.04
* VirtualBox 虚拟机
* CMake
* GCC / G++
* FFmpeg 8.1.1
* SDL2 2.30.0

---

## 功能说明

### 播放能力

当前支持本地视频文件播放。

播放器会完成：

1. Demuxer 读取音视频 packet
2. 视频 packet 进入视频队列
3. 音频 packet 进入音频队列
4. VideoDecoder 解码视频帧
5. AudioDecoder 解码音频帧
6. AudioResampler 将音频转换为 SDL 可播放格式
7. AudioPlayer 使用 SDL 音频回调播放 PCM
8. Renderer 使用 SDL 渲染视频帧
9. SyncController 根据 audio clock 决定视频帧显示、等待或丢弃

---

## 键盘控制

| 按键      | 功能      |
| ------- | ------- |
| `Space` | 暂停 / 继续 |
| `Left`  | 后退 seek |
| `Right` | 前进 seek |
| `Q`     | 退出      |
| `Esc`   | 退出      |

当前版本方向键 seek 步长为 3 秒。

---

## 架构图

```text
                    ┌──────────────────────┐
                    │      输入媒体文件      │
                    └──────────┬───────────┘
                               │
                               ▼
                    ┌──────────────────────┐
                    │       Demuxer         │
                    │   av_read_frame       │
                    └───────┬────────┬─────┘
                            │        │
                 video packet        audio packet
                            │        │
                            ▼        ▼
              ┌────────────────┐   ┌────────────────┐
              │ video_packet_q │   │ audio_packet_q │
              └───────┬────────┘   └───────┬────────┘
                      │                    │
                      ▼                    ▼
              ┌────────────────┐   ┌────────────────┐
              │  VideoDecoder  │   │  AudioDecoder  │
              └───────┬────────┘   └───────┬────────┘
                      │                    │
                      ▼                    ▼
              ┌────────────────┐   ┌────────────────┐
              │ video_frame_q  │   │ audio_frame_q  │
              └───────┬────────┘   └───────┬────────┘
                      │                    │
                      ▼                    ▼
              ┌────────────────┐   ┌────────────────┐
              │    Renderer    │   │  AudioPlayer   │
              │ SDL video out  │   │ SDL audio out  │
              └────────────────┘   └───────┬────────┘
                                           │
                                           ▼
                                  ┌────────────────┐
                                  │   AudioClock   │
                                  └───────┬────────┘
                                          │
                                          ▼
                                  ┌────────────────┐
                                  │ SyncController │
                                  └────────────────┘
```

---

## 核心模块

### Demuxer

负责打开媒体文件、读取 packet，并按 stream index 将音频和视频 packet 分发到不同队列。

主要职责：

* 打开输入文件
* 查找视频流和音频流
* 读取 `AVPacket`
* 音视频 packet 分流
* 处理 seek 请求
* 调用 `avformat_seek_file`
* 清空 packet queue
* 通知 decoder flush
* 清空 audio fifo
* 重置 seek 后的 audio clock

---

### VideoDecoder

负责从视频 packet queue 取出压缩 packet，解码为视频 `AVFrame`。

主要职责：

* 消费视频 packet
* 调用 `avcodec_send_packet`
* 调用 `avcodec_receive_frame`
* 管理视频 `AVFrame` 生命周期
* seek 后 flush 解码器
* seek 后丢弃目标时间之前的旧帧

---

### AudioDecoder

负责从音频 packet queue 取出压缩 packet，解码为音频 `AVFrame`。

主要职责：

* 消费音频 packet
* 解码音频 frame
* 处理音频时间戳
* seek 后 flush 解码器
* seek 后丢弃目标时间之前的旧音频帧

---

### AudioResampler

负责将 FFmpeg 解码出的原始音频格式转换为 SDL2 可以播放的 PCM 格式。

例如将 AAC 常见的 `FLTP` 转换为 SDL 使用的 `S16 packed stereo`。

主要职责：

* 音频采样格式转换
* planar / packed 布局转换
* 采样率转换
* 声道数转换

---

### AudioPlayer

负责 SDL2 音频设备初始化、音频 FIFO 缓冲与播放。

主要职责：

* 打开 SDL audio device
* 维护 audio fifo
* 在 SDL audio callback 中提供 PCM 数据
* 暂停 / 继续音频输出
* seek 后清空 audio fifo

---

### Renderer

负责 SDL2 窗口、纹理、视频帧渲染与键盘事件处理。

主要职责：

* 创建 SDL window
* 创建 SDL renderer / texture
* 渲染视频帧
* 处理键盘输入
* 将 SDL 事件转换成播放器命令

---

### SeekController

负责协调 seek 请求。

主线程不直接调用 `avformat_seek_file`，而是向 `SeekController` 发出 seek 请求。Demuxer 线程在自己的读包循环中取出请求，并在 Demuxer 线程内执行真正的 seek。

这样可以避免主线程和 Demuxer 线程同时操作 `AVFormatContext`。

---

### AudioClock

负责维护音频主时钟。

当前播放器采用音频主时钟策略：音频按声卡节奏播放，视频根据音频时间决定等待、显示或丢帧。

seek 后，audio clock 会被重置到目标 seek 时间，而不是重置为 0。

例如 seek 到 27 秒：

```text
错误做法：
video_pts ≈ 27s
audio_clock = 0s
diff = 27s
同步器误判视频严重超前，导致大量丢帧

正确做法：
video_pts ≈ 27s
audio_clock ≈ 27s
diff ≈ 0
正常显示
```

---

## seek 设计

seek 不是简单调用一次 `avformat_seek_file`。

当前 seek 流程：

```text
用户按方向键
  ↓
主循环计算目标时间 seek_time
  ↓
seek_controller.Request(seek_time)
  ↓
Demuxer 线程检测到 seek 请求
  ↓
Demuxer::DoSeek(target_seconds)
  ↓
avformat_seek_file
  ↓
清空 video / audio packet queue
  ↓
通知 video / audio decoder flush
  ↓
清空 video / audio frame queue
  ↓
设置 drop_until，丢弃目标时间之前的旧帧
  ↓
清空 audio fifo
  ↓
audio_clock.Reset(target_seconds)
```

seek 后必须清理旧数据，否则可能出现：

* seek 后继续显示旧画面
* seek 后播放旧音频残留
* 解码器输出 seek 前缓存的旧帧
* 音视频主时钟不一致
* 同步器大量误丢帧

---

## 音视频同步策略

当前采用 **音频主时钟**。

原因是人耳对音频卡顿和断裂更敏感，而视频偶尔丢一两帧通常不容易察觉。因此播放器通常让音频按声卡时钟连续播放，让视频追随音频。

同步逻辑：

```text
diff = video_pts - audio_clock

diff > 阈值：
    视频太快，等待后显示

diff < -阈值：
    视频太慢，丢帧追赶

否则：
    正常显示
```

---

## 构建

### 安装依赖

Ubuntu 环境下：

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config gdb valgrind
sudo apt install libsdl2-dev
sudo apt install libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev
```

如果使用自编译 FFmpeg，需要确保 CMake 能找到对应 include 和 library 路径。

---

### 编译

在项目根目录执行：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

运行：

```bash
./build/mini-player assets/new.mp4
```

或者进入 build 目录运行：

```bash
cd build
./mini-player ../assets/new.mp4
```

---

## Sanitizer

项目支持通过 CMake 选项打开 sanitizer。

开启 AddressSanitizer：

```bash
cmake -S . -B build -DSANITIZE_ADDRESS=ON -DSANITIZE_THREAD=OFF
cmake --build build
```

开启 ThreadSanitizer：

```bash
cmake -S . -B build -DSANITIZE_ADDRESS=OFF -DSANITIZE_THREAD=ON
cmake --build build
```

关闭 sanitizer：

```bash
cmake -S . -B build -DSANITIZE_ADDRESS=OFF -DSANITIZE_THREAD=OFF
cmake --build build
```

注意：AddressSanitizer 和 ThreadSanitizer 不应同时开启。

---

## 调试与验证

### ThreadSanitizer

当前阶段已使用 ThreadSanitizer 检查多线程数据竞争。

结果：

```text
未发现数据竞争问题
```

---

### Valgrind Memcheck

当前阶段已使用 Valgrind Memcheck 检查内存泄漏与非法内存访问。

运行命令：

```bash
valgrind --tool=memcheck \
  --leak-check=full \
  --show-leak-kinds=all \
  --track-origins=yes \
  ./mini-player ../assets/new.mp4
```

验证结果：

```text
definitely lost: 0 bytes in 0 blocks
indirectly lost: 0 bytes in 0 blocks
possibly lost: 0 bytes in 0 blocks
ERROR SUMMARY: 0 errors
```

说明当前阶段未发现明确内存泄漏、间接泄漏、可疑泄漏或非法内存访问。

`still reachable` 不等于明确泄漏，通常可能来自 FFmpeg / SDL2 / 系统库的内部缓存或全局资源。

---

### GDB

启动 GDB：

```bash
gdb ./mini-player
```

在 GDB 中传入视频参数：

```gdb
run ../assets/new.mp4
```

程序卡住时按 `Ctrl+C`，查看线程：

```gdb
info threads
```

切换线程并查看调用栈：

```gdb
thread 1
bt
```

可用于判断卡顿发生在：

* 主线程等待 frame queue
* decoder 线程等待 packet queue
* Demuxer 线程正在 seek / read
* 线程之间是否出现死锁

---

## 常见问题

### 为什么 seek 后要 flush decoder？

因为解码器内部可能缓存了 seek 前位置的参考帧、B 帧重排序数据或延迟帧。

seek 后如果不 flush，解码器可能继续输出旧位置的帧，导致花屏或画面跳动。

---

### 为什么 seek 后要清 packet queue？

Demuxer 可能已经读取了 seek 前位置的 packet。如果不清空，Decoder 会继续解码旧 packet。

---

### 为什么 seek 后要清 frame queue？

Decoder 可能已经解出了 seek 前位置的 frame。如果不清空，Renderer 或 AudioPlayer 可能继续消费旧 frame。

---

### 为什么 seek 后要清 audio fifo？

SDL 音频播放端可能已经缓存了 seek 前位置的 PCM。如果不清空，seek 后可能先听到旧声音。

---

### 为什么 audio clock 不能 reset 到 0？

因为 seek 后视频帧的 PTS 是目标时间附近。

例如 seek 到 27 秒：

```text
video_pts ≈ 27s
audio_clock = 0s
diff = 27s
```

同步器会误以为视频比音频快了 27 秒，从而大量等待或丢帧。

正确做法是：

```text
audio_clock.Reset(27s)
```

让音频主时钟与 seek 目标时间对齐。

---

### 为什么 Valgrind 很慢？

Valgrind 会监控程序的内存读写、分配和释放，因此运行速度会比正常执行慢很多。播放器项目还包含 SDL、FFmpeg、多线程和音频回调，卡顿属于正常现象。

---

## 项目结构

```text
mini-player/
├── assets/              # 测试视频资源
├── include/             # 头文件
├── src/                 # 源文件
├── tests/               # 测试代码
├── CMakeLists.txt       # CMake 构建脚本
├── README.md            # 项目说明
└── CLAUDE.md            # 开发过程记录 / AI 协作说明
```

---

## 已知限制

* 当前只支持本地文件播放
* 暂未实现网络流播放
* 暂未实现字幕渲染
* 暂未实现进度条 UI
* 暂未实现鼠标点击 seek
* 当前 seek 依赖关键帧，极端长 GOP 视频可能出现 seek 延迟
* 当前主要使用 FFmpeg 软件解码，暂未接入硬件解码
* 当前主要在 Ubuntu 环境下开发和验证，暂未提供 Windows 可执行文件

---

## 后续计划

### Phase 1：播放器稳定性继续完善

* 更完整的状态机
* 暂停状态下 seek 行为优化
* 无音频视频支持
* 更准确的 duration / progress 处理
* 更完善的错误码与日志系统

### Phase 2：网络流支持

* RTSP 拉流播放
* HLS 拉流播放
* 连接超时处理
* 断流重连
* buffer 控制
* Wireshark 抓包分析 RTSP / HLS 流程

### Phase 3：跨平台与 SDK 化探索

* Android NDK / JNI 接入
* Surface 渲染
* AudioTrack 播放
* MediaCodec 硬解
* 对外封装播放器 API

### Phase 4：Linux / 嵌入式应用层探索

* ARM64 交叉编译
* QEMU 验证
* V4L2 摄像头采集 demo
* ALSA 音频采集 demo
* systemd 服务化运行

---

## 项目定位

mini-player 是一个学习型音视频播放器项目。

它的重点不是 UI，而是播放器内核：

* FFmpeg C API
* C++ RAII
* 多线程队列
* 音视频同步
* seek 状态清理
* SDL2 音频 / 视频输出
* GDB / TSan / Valgrind 调试验证

该项目后续可以继续向两个方向扩展：

1. C++ 音视频客户端 / 播放器 SDK
2. Linux / Android 多媒体系统层与嵌入式音视频应用层

---

## License

本项目使用 MIT License 开源，详见 [LICENSE](./LICENSE)。
