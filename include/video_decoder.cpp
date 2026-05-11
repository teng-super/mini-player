#include "video_decoder.h"
#include "ffmpeg_error.h"
#include <iostream>
extern "C"{
    #include<libavcodec/avcodec.h>
}
namespace mp{
    bool VideoDecoder::open(AVCodecParameters* codecpar){
        if(!codecpar) return false;
        const AVCodec* codec=avcodec_find_decoder(codecpar->codec_id);
        if(!codec) {
            std::cerr << "未找到解码器, codec_id=" << codecpar->codec_id << std::endl;
            return false;
        }
        ctx_.reset(avcodec_alloc_context3(codec));
        if(!ctx_) return false;
        //检查，凡是这种返回一个数字（0，1，-1的都可以用checkffmpeg函数检查）
        if (!CheckFFmpeg(
            avcodec_parameters_to_context(ctx_.get(), codecpar),
            "avcodec_parameters_to_context"
        )) {
        return false;
        }

        if(!CheckFFmpeg(
            avcodec_open2(ctx_.get(),codec,nullptr),
            "avcodec_open2"
        )){
            return false;
        }
        return true;
    }
    bool VideoDecoder::SendPacket(AVPacket* pkt){
        int ret = avcodec_send_packet(ctx_.get(),pkt);
        if(ret<0 && ret!=AVERROR(EAGAIN) && ret!=AVERROR_EOF){
            CheckFFmpeg(ret,"avcodec_send_packet");
            return false;
            }
            return true;
        }
    int VideoDecoder::ReceiveFrame(AVFrame* frame){
        int ret = avcodec_receive_frame(ctx_.get(),frame);
        if(ret == 0) return 1;//取出一帧
        if(ret == AVERROR(EAGAIN)) return 0;
        if(ret == AVERROR_EOF) return -1;

        CheckFFmpeg(ret,"avcodec_receive_frame");
        return -1;
    }
}