#pragma once
#include <atomic>
#include <thread>
#include "bounded_blocking_queue.h"
#include "demuxer.h"
#include "ffmpeg_raii.h"

namespace mp{
    using FrameQueue = BoundedBlockingQueue<AVFrame*>;//接入frame数据的队列
    class VideoDecoder{
        public:
            VideoDecoder();
            ~VideoDecoder();

            VideoDecoder(const VideoDecoder& other) = delete;
            VideoDecoder& operator = (const VideoDecoder& other) = delete;

            bool open(AVCodecParameters* codec);
            /*
            //用codecpar初始化解码器
            bool SendPacket(AVPacket* pkt);
            //输入一个packet
            int ReceiveFrame(AVFrame* fam);
            //取一帧frame，返回1：取出一帧，0：EAGAIN，需要更多packet，-1：EOF或真错；
            */

            //上面那个是老版本，多线程引入
            void Start(PacketQueue* packet_queue);//接入被塞满packet的管子
            void Stop();
            FrameQueue& frame_queue(){
                return frame_queue_;
            }//给渲染器使用这个队列的接口

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
            void Run();
            bool DrainFrames();

            AVCodecContextPtr ctx_;
            FrameQueue frame_queue_{4};
            PacketQueue* packet_queue_ = nullptr; //不拥有上游队列，纯靠外部传入

            std::thread thread_;
            std::atomic<bool> stop_requested_{false};
    };
}
