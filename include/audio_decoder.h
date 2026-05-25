#pragma once
#include<stop_token>
#include<thread>

#include "bounded_blocking_queue.h"
#include "demuxer.h"
#include "ffmpeg_raii.h"

namespace mp{
    using FrameQueue = BoundedBlockingQueue<AVFrame*>;//别忘了这个

    class AudioDecoder{
        public:
        AudioDecoder();
        ~AudioDecoder();

        AudioDecoder(const AudioDecoder&) = delete;
        AudioDecoder& operator=(const AudioDecoder&) = delete;
        /*AudioDecoder 内部持有 std::jthread、AVCodecContextPtr、BoundedBlockingQueue。这三个东西都不能随便拷贝:
        jthread 本身就是 move-only,拷贝构造会编译错
        AVCodecContextPtr 是 std::unique_ptr(讲义里的 RAII 包装),也是 move-only
        队列里塞着 AVFrame* 裸指针,如果拷贝就两个对象同时持有同一批指针,析构时 double free*/

        bool Open(AVCodecParameters* codecpar);//需要拿这个申请解码器信息去finddecoder
        void Start(PacketQueue* packet_queue);
        void Stop();

        int sample_rate() const {return audio_ctx_ ? audio_ctx_->sample_rate : 0;};//没有重采样时的采样率
        int channels() const {return audio_ctx_ ? audio_ctx_-> ch_layout.nb_channels : 0;};//原始声道数
        const AVChannelLayoutgi* ch_layout() const { return audio_ctx_ ? &audio_ctx_->ch_layout : nullptr;}//第一个const: 不能修改底层数据; 第二个const: 此函数不修改类成员
        FrameQueue& frame_queue() { return frame_queue_; }
        AVSampleFormat sample_format() const {
            return audio_ctx_ ? audio_ctx_->sample_fmt : AV_SAMPLE_FMT_NONE;
        }

        private:
        void Run(std::stop_token stoken);
        bool DrainFrames(std::stop_token stoken);
        AVCodecContextPtr audio_ctx_; // 命名 audio_ctx_,避免和VideoDecoder::ctx_ 混淆
        PacketQueue* packet_queue_ = nullptr;
        //裸指针，代表不管理其生命周期
        FrameQueue frame_queue_{30}; // 容量 30
        std::jthread thread_;
    };
}