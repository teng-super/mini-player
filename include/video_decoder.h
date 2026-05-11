#pragma once
#include"ffmpeg_raii.h"

namespace mp{
    class VideoDecoder{
        public:
            VideoDecoder() = default;
            ~VideoDecoder() = default;
            VideoDecoder(const VideoDecoder& other) = delete;
            VideoDecoder& operator = (const VideoDecoder& other) = delete;
            bool open(AVCodecParameters* codec);
            //用codecpar初始化解码器
            bool SendPacket(AVPacket* pkt);
            //输入一个packet
            int ReceiveFrame(AVFrame* fam);
            //取一帧frame，返回1：取出一帧，0：EAGAIN，需要更多packet，-1：EOF或真错；
            AVPixelFormat pixelformat()const {return ctx_?ctx_->pix_fmt:AV_PIX_FMT_NONE;};
            //AVPixelFormat表示像素数据到底怎么摆放，即源图像是什么像素格式
            int width()const {
                return ctx_?ctx_->width : 0;
            }
            int height()const {
                return ctx_?ctx_->height : 0;
            }
            //求分辨率
        private:
            AVCodecContextPtr ctx_;
    };
}
