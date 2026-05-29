#pragma once
#include <atomic>
#include <thread>
#include <stop_token>
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

            bool Open(AVCodecParameters* codec);
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
            //用于seek时清除残余物
            void Flush(){
                if(video_ctx_) avcodec_flush_buffers(video_ctx_.get());
            }
            void ClearFrameQueue() {
                frame_queue_.Clear([](AVFrame* frame) { av_frame_free(&frame); });
            }
            
            FrameQueue& frame_queue(){
                return frame_queue_;
            }//给渲染器使用这个队列的接口

            AVPixelFormat pixelformat()const {return video_ctx_ ? video_ctx_->pix_fmt : AV_PIX_FMT_NONE;};
            //AVPixelFormat表示像素数据到底怎么摆放，即源图像是什么像素格式
            int width()const {
                return video_ctx_ ? video_ctx_->width : 0;
            }
            int height()const {
                return video_ctx_ ? video_ctx_->height : 0;
            }
            //求分辨率
        private:
            void Run(std::stop_token stoken);
            bool DrainFrames(std::stop_token stoken);

            AVCodecContextPtr video_ctx_;
            FrameQueue frame_queue_{4};
            PacketQueue* packet_queue_ = nullptr; //不拥有上游队列，纯靠外部传入

            std::jthread thread_;
            //std::atomic<bool> stop_requested_{false};
    };
}
