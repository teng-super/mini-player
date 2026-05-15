#include<iostream>
#include"demuxer.h"
#include"ffmpeg_error.h"

namespace mp{
    Demuxer::Demuxer() = default;
    Demuxer::~Demuxer() {//析构函数不能写等于
        Stop();
        packet_queue_.Clear([](AVPacket*pkt){av_packet_free(&pkt);});
    }
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
         av_find_best_stream(fmt_ctx_.get,AVMEDIA_TYPE_VIDEO,-1,-1,&decoder,0);*/
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

    //之前这里有个单线程用的readpacket一一读包函数，放到run里面了
    void Demuxer::Start(){
        if(thread_.joinable()){//这个函数表示是否需要被join或者detach，需要的话返回true
            //这个地方如果返回true说明需要被join，说明这个线程已经跑起来了
           std::cerr << "Demuxer already started" << std::endl;
            return; 
        }
        stop_requested_=true;
        thread_ = std::thread(&Demuxer::Run, this);
        //创建一个线程让他去执行Run
    }

    void Demuxer::Stop(){
        stop_requested_ = true;
        packet_queue_.Close();
        if (thread_.joinable()) {
            thread_.join();//还能停的话就停了吧
        }
    }

    void Demuxer::Run(){
        while(!stop_requested_){
            AVPacket* pkt = av_packet_alloc();
            if (!pkt) {
                std::cerr << "av_packet_alloc failed" << std::endl;
                break;
            }
            int ret = av_read_frame(fmt_ctx_.get(),pkt);
            if(ret<0){
                // 读完文件或出错
                av_packet_free(&pkt);
                if (ret != AVERROR_EOF) {//在ret为AVERROR_EOF时，此处返回-1
                    //不同于自己封装的receiveframe，这个是ffmpeg的原生返回值
                    CheckFFmpeg(ret, "av_read_frame");
                    }
                    // 通知下游：没有更多 packet 了
                    packet_queue_.Close();
                    break;
                }
                if (pkt->stream_index != video_stream_idx_) {
                    // 不是我们要的流，丢弃
                    av_packet_free(&pkt);
                    continue;
                }
                if (!packet_queue_.push(pkt)) {
                    av_packet_unref(pkt);
                    break;
                }
            }
            //全部push之后就不需要Demuxer线程去释放了，防止幽灵指针
            std::cout << "[Demuxer] thread exiting" << std::endl;
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

