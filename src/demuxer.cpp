#include<iostream>
#include"demuxer.h"
#include"ffmpeg_error.h"

namespace mp{
    Demuxer::Demuxer() = default;
    Demuxer::~Demuxer() {//析构函数不能写等于
        Stop();//严格按照先后顺序析构，每一个阻塞在退出时都必须有可以唤醒他的能力
        video_packet_queue_.Clear([](AVPacket*pkt){av_packet_free(&pkt);});
        audio_packet_queue_.Clear([](AVPacket*pkt){av_packet_free(&pkt);});
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
            }
            if(fmt_ctx_->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_AUDIO){
                audio_stream_idx_ = i;
            }
        }
        if (video_stream_idx_ < 0 && audio_stream_idx_ < 0) {
            std::cerr << "No video or audio stream in " << path << std::endl;
            return false;
        }
            std::cout << "[Demuxer] video stream: " << video_stream_idx_
            << ", audio stream: " << audio_stream_idx_ << std::endl;
            return true;
        }      
        
    //之前这里有个单线程用的readpacket一一读包函数，放到run里面了
    void Demuxer::Start(){
        if(thread_.joinable()){//这个函数表示是否需要被join或者detach，需要的话返回true
            //这个地方如果返回true说明需要被join，说明这个线程已经跑起来了
           std::cerr << "Demuxer already started" << std::endl;
            return; 
        }
        //stop_requested_=false;bug1:这里写的true，导致run不起来
        thread_ = std::jthread([this](std::stop_token st){Run(st);});//jthread创建时会把stop_token作为第一个参数传入jthread
        //创建一个线程让他去执行Run,用了lambda捕获this，因为这里第一个参数要传stop_token但是又要传this
        //正常写法GCC13其实可以但是不严谨：jthread(&Demuxer::Run,this)
    }

    void Demuxer::Stop(){
        thread_.request_stop();//终止线程
        video_packet_queue_.Close();//这里加这个是为了防止睡着的push线程导致无法join（哪怕是jthread也不行）导致的死锁
        audio_packet_queue_.Close();//同上
        if(thread_.joinable()) thread_.join();// 必须 join：保证 Run() 退出后才能析构 fmt_ctx_（否则 UAF）
    }

    void Demuxer::Run(std::stop_token stoken){
        while(!stoken.stop_requested()){
            AVPacket* pkt = av_packet_alloc();
            if (!pkt) {
                std::cerr << "av_packet_alloc failed" << std::endl;
                break;
            }
            int ret = av_read_frame(fmt_ctx_.get(),pkt);
            if(ret<0){
                av_packet_free(&pkt);
                if (ret != AVERROR_EOF) {
                    CheckFFmpeg(ret, "av_read_frame");
                }
                video_packet_queue_.Close();
                audio_packet_queue_.Close();
                break;
            }
            // 按 stream_index 分发到对应队列；push 失败 = 队列被 Close = 下游退出 = 整个生产者结束
            PacketQueue* target = nullptr;
            if (pkt->stream_index == video_stream_idx_)      target = &video_packet_queue_;
            else if (pkt->stream_index == audio_stream_idx_) target = &audio_packet_queue_;

            if (!target) {
                av_packet_free(&pkt); // 字幕/附件等无关流，直接丢弃
                continue;
            }
            if (!target->push(pkt)) {
                av_packet_free(&pkt);
                break;
            }
        }
        std::cout << "[Demuxer] thread exiting" << std::endl;
    }

    AVCodecParameters* Demuxer::video_cpar() const {
        if(!fmt_ctx_ || video_stream_idx_ < 0) return nullptr;
        return fmt_ctx_->streams[video_stream_idx_]->codecpar;
    }

    AVRational Demuxer::video_time_base() const {
        if(!fmt_ctx_ || video_stream_idx_ < 0) return AVRational{0, 1};
        return fmt_ctx_->streams[video_stream_idx_]->time_base;
    }

    AVRational Demuxer::video_frame_rate() const {
        if(!fmt_ctx_ || video_stream_idx_ < 0) return AVRational{0, 1};
        return av_guess_frame_rate(
            fmt_ctx_.get(),
            fmt_ctx_->streams[video_stream_idx_],
            nullptr);
    }
    //基于 container 和 codec 信息猜测帧率；第三个参数 frame 可以为 NULL；如果不知道帧率，会返回 0/1
    AVCodecParameters* Demuxer::audio_codecpar() const {
        if(!fmt_ctx_ || audio_stream_idx_ < 0) return nullptr;
        return fmt_ctx_->streams[audio_stream_idx_]->codecpar;
    }
    AVRational Demuxer::audio_time_base() const {
        if (audio_stream_idx_ < 0) return AVRational{0, 1};
        return fmt_ctx_->streams[audio_stream_idx_]->time_base;
    }
    double Demuxer::total_duration_seconds() const {//这个是媒体的总播放时长,把微妙转换成秒
        if(!fmt_ctx_) return 0.0;
        if(fmt_ctx_->duration == AV_NOPTS_VALUE) return 0.0;
        //直播流和裸文件流没有pts
        return static_cast<double>(fmt_ctx_->duration) / AV_TIME_BASE;//100万，强制转换防止整数除法
    }
}

