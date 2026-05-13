#pragma once
#include"ffmpeg_raii.h"
#include<SDL2/SDL.h>

namespace mp{
    class Renderer{
        public:
            Renderer() = default;
            ~Renderer();
            Renderer(const Renderer&) = delete;
            Renderer& operator=(const Renderer&) = delete;
            //初始化SDL，创建窗口和渲染器
            bool Open(int width,int height,AVPixelFormat src_pix_fmt);
            //把frame（YUV）转换成成RGB格式渲染
            bool RenderFrame(AVFrame* frame);
            // 处理 SDL 事件,返回 false 表⽰⽤⼾要退出
            bool HandleEvents();

            private:
            SDL_Window* window_ = nullptr;//窗口
            SDL_Renderer* renderer_ = nullptr;//渲染器
            SDL_Texture* texture_ = nullptr;//纹理

            SwsContextPtr sws_ctx_;//一个智能指针封装的格式转化上下文
            AVFramePtr rgb_frame_;//转换后的RGB帧
            //分辨率
            int width_ = 0;
            int height_ = 0;
    };
}
