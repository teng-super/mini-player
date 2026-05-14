# Mini Player

一个基于现代 C++、FFmpeg 和 SDL2 构建的极简本地视频播放器。旨在探索音视频工程、解码管线以及多媒体系统的底层开发。

## 目前状态 
还未完成

## 🛠️ 技术栈
* **核心语言**: C++20 (利用现代 C++ 特性与 RAII 机制管理底层资源)
* **解封装与解码**: FFmpeg (libavformat, libavcodec, libavutil, libswscale)
* **音视频渲染**: SDL2
* **构建系统**: CMake

## 📦 依赖安装
在 Ubuntu 系统下，可以通过系统包管理器直接安装所需的开发库：

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config
sudo apt install libavcodec-dev libavformat-dev libswscale-dev libavutil-dev
sudo apt install libsdl2-dev