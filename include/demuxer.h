#pragma once
#include <string>
#include <optional>
#include "ffmpeg_raii.h"
namespace mp{
    
class Demuxer {
    public:
        Demuxer() = default;
        ~Demuxer() = default;
        // 禁⽌拷⻉(资源类的标准做法)
        Demuxer(const Demuxer&) = delete;
        Demuxer& operator=(const Demuxer&) = delete;

        bool Open(const std::string& path);//自定义函数，打开对应文件
        bool ReadPacket(AVPacket* pkt);
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
    int video_stream_idx_ = -1;
    AVFormatContextPtr fmt_ctx_;
};

}
