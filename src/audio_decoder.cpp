#include "audio_decoder.h"
#include "ffmpeg_error.h"
#include <iostream>
extern "C"{
    #include <libavcodec/avcodec.h>
}
namespace mp{
    AudioDecoder::AudioDecoder() = default;
    AudioDecoder::~AudioDecoder(){
        Stop();
        frame_queue_.Clear([](AVFrame* frame){av_frame_free(&frame);});
    }

    bool AudioDecoder::Open(AVCodecParameters* codecpar){
        if(!codecpar) return false;
        const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
        if(!codec){
            std::cerr << "未找到音频解码器, codec_id=" << codecpar->codec_id << std::endl;
            return false;
        }
        audio_ctx_.reset(avcodec_alloc_context3(codec));
        if(!audio_ctx_) return false;
        if(!CheckFFmpeg(
            avcodec_parameters_to_context(audio_ctx_.get(), codecpar),
            "avcodec_parameters_to_context(audio)"
        )){
            return false;
        }
        if(!CheckFFmpeg(
            avcodec_open2(audio_ctx_.get(), codec, nullptr),
            "avcodec_open2(audio)"
        )){
            return false;
        }
        return true;
    }

    void AudioDecoder::Start(PacketQueue* packet_queue){
        if(thread_.joinable()) return;
        packet_queue_ = packet_queue;
        thread_ = std::jthread([this](std::stop_token st){Run(st);});
    }

    void AudioDecoder::Stop(){
        // 调用方必须先 demuxer.Stop()，让 packet_queue 已 Close，否则 join 死锁
        thread_.request_stop();
        frame_queue_.Close();
        if(thread_.joinable()) thread_.join();
    }

    void AudioDecoder::Run(std::stop_token stoken){
        while(!stoken.stop_requested()){
            auto opt_pkt = packet_queue_->pop();
            if(!opt_pkt){
                // 上游 close 了，送 NULL 进解码器进入 flush 模式
                avcodec_send_packet(audio_ctx_.get(), nullptr);
                DrainFrames(stoken);
                break;
            }
            AVPacket* pkt = *opt_pkt;
            int ret = avcodec_send_packet(audio_ctx_.get(), pkt);
            av_packet_free(&pkt);
            if(ret < 0 && ret != AVERROR(EAGAIN)){
                CheckFFmpeg(ret, "avcodec_send_packet(audio)");
                continue;
            }
            if(!DrainFrames(stoken)) break;
        }
        frame_queue_.Close();
        std::cout << "[AudioDecoder] thread exiting" << std::endl;
    }

    bool AudioDecoder::DrainFrames(std::stop_token stoken){
        while(true){
            if(stoken.stop_requested()) break;
            AVFrame* frame = av_frame_alloc();
            if(!frame){
                std::cerr << "[AudioDecoder] av_frame_alloc failed" << std::endl;
                return false;
            }
            int ret = avcodec_receive_frame(audio_ctx_.get(), frame);
            if(ret == AVERROR(EAGAIN)){
                av_frame_free(&frame);
                return true;
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
}
