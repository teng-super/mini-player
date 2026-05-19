#include "video_decoder.h"
#include "ffmpeg_error.h"
#include <iostream>
extern "C"{
    #include<libavcodec/avcodec.h>
}
namespace mp{
    VideoDecoder::VideoDecoder() = default;
    VideoDecoder::~VideoDecoder(){
        Stop();//停止
        frame_queue_.Clear([](AVFrame* frame){av_frame_free(&frame);});
    };
    bool VideoDecoder::open(AVCodecParameters* codecpar){
        if(!codecpar) return false;
        const AVCodec* codec=avcodec_find_decoder(codecpar->codec_id);
        if(!codec) {
            std::cerr << "未找到解码器, codec_id=" << codecpar->codec_id << std::endl;
            return false;
        }
        video_ctx_.reset(avcodec_alloc_context3(codec));
        if(!video_ctx_) return false;
        //检查，凡是这种返回一个数字（0，1，-1的都可以用checkffmpeg函数检查）
        if (!CheckFFmpeg(
            avcodec_parameters_to_context(video_ctx_.get(), codecpar),
            "avcodec_parameters_to_context"
        )) {
        return false;
        }

        if(!CheckFFmpeg(
            avcodec_open2(video_ctx_.get(),codec,nullptr),
            "avcodec_open2"
        )){
            return false;
        }
        return true;
    }

    void VideoDecoder::Start(PacketQueue* packet_queue){
        if(thread_.joinable()) return;
        packet_queue_ = packet_queue;
        //stop_requested_ = false;//jthread不需要这个
        thread_ = std::jthread([this](std::stop_token st){Run(st);});
    }
    void VideoDecoder::Stop(){
        // 调用方必须保证在此之前已调用 demuxer.Stop()，
        // 以确保 packet_queue 已 close，否则 join 可能死锁
        thread_.request_stop();
        //stop_requested_ = true;
        frame_queue_.Close();
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    void VideoDecoder::Run(std::stop_token stoken){
        while(!stoken.stop_requested()){
            //注意！stoken用的判断是stop_requested，而thread用的是request_stop，两个是反着的！！！
            auto opt_pkt = packet_queue_->pop();//从上游取出一个packet

            if (!opt_pkt) {
            // 上游 close 了开始送 NULL 进解码器进入 flush 模式
                avcodec_send_packet(video_ctx_.get(), nullptr);
                DrainFrames(stoken);//把剩余帧全部取出 
                break;
            }
            AVPacket* pkt = *opt_pkt;//这里用*是为了获得真正的AVPacket指针，而不是optional这个奇特类型
            int ret = avcodec_send_packet(video_ctx_.get(),pkt);
            av_packet_free(&pkt);//输送给解码器了，可以释放了
            //严格来讲，这里涉及到引用计数和关于packet底层原理相关的东西
            //这里send时，引用计数会加一，在释放这个pkt壳子的同时减一，对象并不会被销毁
            if (ret < 0 && ret != AVERROR(EAGAIN)) {
                CheckFFmpeg(ret, "avcodec_send_packet");
                continue;
            }

            if (!DrainFrames(stoken)) break;
        }
        frame_queue_.Close();//
        std::cout << "[VideoDecoder] thread exiting" << std::endl;
    }

    bool VideoDecoder::DrainFrames(std::stop_token stoken){
        while(true){
            if(stoken.stop_requested()) break;
            AVFrame* frame = av_frame_alloc();
            if (!frame) {                          // ← 加这个检查
            std::cerr << "[VideoDecoder] av_frame_alloc failed" << std::endl;
            return false;
        }
            int ret = avcodec_receive_frame(video_ctx_.get(),frame);

            //if(!frame) return false;这里错了，应该先检查frame申请成功没有再传入receiveframe
            if(ret == AVERROR(EAGAIN)){
                av_frame_free(&frame);
                return true;//返回true再来一轮
            }
            if(ret == AVERROR_EOF){
                av_frame_free(&frame);
                return false;
            }
            if(ret < 0){
                av_frame_free(&frame);
                return false;
            }

            if(!frame_queue_.push(frame)){
                av_frame_free(&frame);
                return false;
            }
        }
        return false;
    }










    /*bool VideoDecoder::SendPacket(AVPacket* pkt){
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
        return -1;}
    *///时代的眼泪

        
}