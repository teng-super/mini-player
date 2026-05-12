#include"renderer.h"
#include"ffmpeg_error.h"
#include<iostream>

namespace mp{
    Renderer::~Renderer(){
        // 按顺序释放所有资源
        if (renderer_) SDL_DestroyRenderer(renderer_);  // 销毁渲染器
        if(texture_) SDL_DestroyTexture(texture_);      // 销毁纹理
        if (window_) SDL_DestroyWindow(window_);        // 销毁窗口
        SDL_Quit();                                      // 退出SDL
    }
    bool Renderer::Open(int width,int height,AVPixelFormat src_pix_fmt){
        width_ = width;
        height_ = height;
        if(SDL_Init(SDL_INIT_VIDEO) < 0){
            std::cerr << "SDL_Init: " << SDL_GetError() << std::endl;
            return false;
        }

        window_ = SDL_CreateWindow(
            "mini-player",                    // 窗口标题
            SDL_WINDOWPOS_CENTERED,           // 窗口水平位置（屏幕中央）
            SDL_WINDOWPOS_CENTERED,           // 窗口竖直位置（屏幕中央）
            width,                            // 窗口宽度
            height,                           // 窗口高度
            SDL_WINDOW_SHOWN);                // 窗口显示方式（显示出来）
        if(!window_){
            return false;
        }

        renderer_ = SDL_CreateRenderer(
            window_,                          // 往哪个窗口上画
            -1,                               // 用哪个驱动（-1就是自动选）
            SDL_RENDERER_ACCELERATED);        // 用显卡加速渲染
        if (!renderer_) return false;

        texture_ = SDL_CreateTexture(
            renderer_,                        // 往哪个渲染器上画
            SDL_PIXELFORMAT_RGB24,            // 图片的颜色格式（RGB）
            SDL_TEXTUREACCESS_STREAMING,      // 允许边读边更新图片
            width,                            // 图片宽度
            height);                          // 图片高度
        if (!texture_) return false;

        sws_ctx_.reset(sws_getContext(
            width, height, src_pix_fmt,      // 源图像：宽、高、格式（YUV等）
            width, height, AV_PIX_FMT_RGB24, // 目标图像：宽、高、格式（RGB）
            SWS_BILINEAR,                    // 缩放算法（双线性插值）
            nullptr, nullptr, nullptr));     // 颜色空间和参数（用默认值）
        if(!sws_ctx_) return false;

        // 创建RGB帧用来存储转换后的数据
        rgb_frame_ = MakeFrame();              // 创建一个空的帧对象
        rgb_frame_->format = AV_PIX_FMT_RGB24; // 设置颜色格式为RGB
        rgb_frame_->width = width;             // 设置帧宽度
        rgb_frame_->height = height;           // 设置帧高度

        // 给RGB帧分配实际的内存（32字节对齐）
        if (!CheckFFmpeg(
            av_frame_get_buffer(rgb_frame_.get(), 32),
            "av_frame_get_buffer")) {
            return false;
        }
        return true;
    }
    bool Renderer::RenderFrame(AVFrame* frame){
        // 格式转换：YUV → RGB
        sws_scale(sws_ctx_.get(),
            frame->data, frame->linesize, 0, height_,  // 源数据、行大小、起始行、行数
            rgb_frame_->data, rgb_frame_->linesize);   // 目标数据、行大小
        
        // 把RGB数据上传到纹理
        SDL_UpdateTexture(texture_, nullptr, rgb_frame_->data[0], rgb_frame_->linesize[0]);
        
        // 清理上一帧（黑屏）
        SDL_RenderClear(renderer_);
        
        // 把纹理复制到渲染器
        SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
        
        // 显示到屏幕
        SDL_RenderPresent(renderer_);
        
        return true;
    }
    
}