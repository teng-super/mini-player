#pragma once
#include <atomic>
#include <string>
#include <optional>
#include <thread>
#include <memory>
#include "ffmpeg_raii.h"
#include "bounded_blocking_queue.h"
namespace mp{
    
class Demuxer {
    using PacketQueue = BoundedBlockingQueue<AVPacket*>;
    public:
        Demuxer() = default;
        ~Demuxer() = default;
        // 禁⽌拷⻉(资源类的标准做法)
        Demuxer(const Demuxer&) = delete;
        Demuxer& operator=(const Demuxer&) = delete;

        bool Open(const std::string& path);//自定义函数，打开对应文件,成功返回true
        void Start();//启动一个读包线程

        void Stop();//停止此线程
        //将原先的一次一次调用换成start,stop这样Run() + packet_queue()给代替

        PacketQueue& packet_queue(){return packet_queue_;};
        int video_stream_index() const {return video_stream_idx_;} ;//返回对应流的下标
        AVCodecParameters* video_cpar() const;
        /*  codec_id       编码格式，比如 H.264 / H.265 / VP9
            width          视频宽度
            height         视频高度
            format         像素格式信息
            bit_rate       码率
            extradata      SPS/PPS 等额外信息*/
        AVRational video_time_base() const;//时间基查找
        AVRational video_frame_rate() const;
        //基于 container 和 codec 信息猜测帧率；第三个参数 frame 可以为 NULL；如果不知道帧率，会返回 0/1
        AVFormatContext* format_context() const {return fmt_ctx_.get();};//
    private:
    void Run();//后台的跑函数
    int video_stream_idx_ = -1;
    AVFormatContextPtr fmt_ctx_;

    PacketQueue packet_queue_{60}; // capacity 60的读packet队列
    std::thread thread_;//读线程
    std::atomic<bool> stop_requested_{false};//让线程开始和停止
};

}
