#include<iostream>
#include"demuxer.h"
#include"ffmpeg_error.h"

namespace mp{
    bool Demuxer::Open(const std::string& path){
        AVFormatContext* raw = nullptr;
        if(!CheckFFmpeg(avformat_open_input(&raw,path.c_str(),nullptr,nullptr),
        "avformat_open_input"    
    )){
        return false;    
        }
        fmt_ctx_.reset(raw);//将Demuxxer类里面私有变量AVFormatContextPtr类型的变量fmt_ctx重写为raw
        //存入被改写的智能指针
        if(!CheckFFmpeg(avformat_find_stream_info(raw,nullptr),
        "avformat_find_stream_info")){
            return false;
        }
        av_dump_format(fmt_ctx_.get(), 0, path.c_str(), 0);//打开文件并读取流信息之后打印诊断信息

        /*const AVCodec* decoder = nullptr;
         av_find_best_stream(fmt_ctx.get,AVMEDIA_TYPE_VIDEO,-1,-1,&decoder,0)；*/
        for(unsigned i=0;i<fmt_ctx_->nb_streams;i++){
            if(fmt_ctx_->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO){
                video_stream_idx_ = i;
                break;
            }
        }
        if (video_stream_idx_ < 0) {
            std::cerr << "No video stream in " << path << std::endl;
            return false;
        }            
        return true;
    }

    bool Demuxer::ReadPacket(AVPacket* pkt){
        while(true){
            int ret = av_read_frame(fmt_ctx_.get(),pkt);//.get()从智能指针里取出原始指针
            if (ret == AVERROR_EOF) return false;
            if (ret < 0) CheckFFmpeg(ret,"av_read_frame");
            if (pkt->stream_index == video_stream_idx_) return true;
            av_packet_unref(pkt);
        }
    }

    AVCodecParameters* Demuxer::video_cpar() const {
        if(video_stream_idx_ < 0 ) return nullptr;
        return fmt_ctx_->streams[video_stream_idx_]->codecpar;
    }

    AVRational Demuxer::video_time_base() const {
        return fmt_ctx_->streams[video_stream_idx_]->time_base;
    }

    AVRational Demuxer::video_frame_rate() const {
        return av_guess_frame_rate(
            fmt_ctx_.get(),
            fmt_ctx_->streams[video_stream_idx_],
            nullptr);
    }
    //基于 container 和 codec 信息猜测帧率；第三个参数 frame 可以为 NULL；如果不知道帧率，会返回 0/1
}
