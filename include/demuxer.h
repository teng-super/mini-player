#pragma once
#include <atomic>
#include <string>
#include <optional>
#include <thread>
#include <memory>
#include <stop_token>
#include <functional>//std::function<void()>的头文件
#include <utility>//移动语义的头文件

#include "ffmpeg_raii.h"
#include "bounded_blocking_queue.h"
#include "seek_controller.h"

namespace mp{
    class VideoDecoder;//前向声明，用于解决demuxer和两个decoder类的循环include问题
    class AudioDecoder;

    using PacketQueue = BoundedBlockingQueue<AVPacket*>;//packet队列
class Demuxer {
    public:
        Demuxer();
        ~Demuxer();
        // 禁⽌拷⻉(资源类的标准做法)
        Demuxer(const Demuxer&) = delete;
        Demuxer& operator=(const Demuxer&) = delete;

        bool Open(const std::string& path);//自定义函数，打开对应文件,成功返回true
        void Start();//启动一个读包线程

        void Stop();//停止此线程，引入jthread自动析构
        //将原先的一次一次调用换成start,stop这样Run() + packet_queue()给代替

        //================视频类==================
        bool has_video() const {return video_stream_idx_ >= 0;};
        PacketQueue& video_packet_queue(){return video_packet_queue_;};
        int video_stream_index() const {return video_stream_idx_;} ;//返回对应流的下标
        AVCodecParameters* video_codecpar() const;
        /*  codec_id       编码格式，比如 H.264 / H.265 / VP9
            width          视频宽度
            height         视频高度
            format         像素格式信息
            bit_rate       码率
            extradata      SPS/PPS 等额外信息*/
        AVRational video_time_base() const;//时间基查找
        AVRational video_frame_rate() const;

        //================音频类====================
        bool has_audio() const {return audio_stream_idx_ >= 0;};
        PacketQueue& audio_packet_queue(){return audio_packet_queue_;};
        int audio_stream_index() const {return audio_stream_idx_;};
        AVCodecParameters* audio_codecpar() const;
        AVRational audio_time_base() const;
        double total_duration_seconds() const;


        //基于 container 和 codec 信息猜测帧率；第三个参数 frame 可以为 NULL；如果不知道帧率，会返回 0/1
        AVFormatContext* format_context() const {return fmt_ctx_.get();};//

        //===============seek类==================
        void RegisterVideoDecoder(VideoDecoder* d) { video_decoder_ = d; }
        void RegisterAudioDecoder(AudioDecoder* d) { audio_decoder_ = d; }
        void SetSeekController(SeekController* seek_controller){
            seek_controller_ = seek_controller;
        }
        using FifoClearer = std::function<void(double)>;//改成接受一个double类型的参数
        //function<返回值类型(参数类型)>指的是一个类模板对象，它内部保存一个“可调用对象”。
        void SetAudioFifoClearer(FifoClearer clearer){//主函数传audioplayer里面新定义的哪个fifoclear接口给funtion封装住
            audio_fifo_clearer_ = std::move(clearer);
        }


    private:
    void Run(std::stop_token stoken);//后台的跑函数
    void DoSeek(double target_seconds);//具体seek到的时间点，target_seconds为音频时钟上的时间

    int video_stream_idx_ = -1;
    int audio_stream_idx_ = -1;
    AVFormatContextPtr fmt_ctx_;


    PacketQueue video_packet_queue_{60};  // capacity 60 的视频读 packet 队列
    PacketQueue audio_packet_queue_{100}; // 音频 packet 数量比视频多,容量大一些
    std::jthread thread_;//读线程
    //不需要了std::atomic<bool> stop_requested_{false};//让线程开始和停止

    VideoDecoder* video_decoder_ = nullptr;
    AudioDecoder* audio_decoder_ = nullptr; 
    SeekController* seek_controller_ = nullptr;
    FifoClearer audio_fifo_clearer_;
};

}
