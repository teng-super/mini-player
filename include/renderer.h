#pragma once
#include"ffmpeg_raii.h"
#include<SDL2/SDL.h>

namespace mp{
    class Randerer{
        public:
            Randerer() = default;
            ~Randerer() = default;
            Randerer(const Randerer&) = delete;
            Randerer operator = (const Randerer&) = delete;
            //初始化SDL，创建窗口和渲染器
            bool Open(int width,int hight,AVPixelFormat src_pix_fmt);
            //把frame（YUV）渲染成RGB格式
            bool RanderFrame(AVFrame* frame);
            // 处理 SDL 事件,返回 false 表⽰⽤⼾要退出
            bool HandleEvents();
    };
}
