#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <algorithm>
#include "demuxer.h"
#include "renderer.h"
#include "video_decoder.h"
#include "audio_decoder.h"
#include "audio_resampler.h"
#include "audio_player.h"
#include "audio_clock.h"
#include "sync_controller.h"

using namespace mp;
int main(int argc,char* argv[]){//命令行参数和参数具体内容
    if(argc<2){
        std::cerr<<"请传入视频\n";
        return 1;
    }
    Demuxer demuxer;
    if (!demuxer.Open(argv[1])) return 1;//【0】是miniplayer，【1】是assert里面那个MP4的路径
    VideoDecoder video_decoder;
    AudioDecoder audio_decoder;
    AudioPlayer audio_player;
    if (demuxer.has_video()) {
        if (!video_decoder.Open(demuxer.video_codecpar())) return 1;
    }
    if (demuxer.has_audio()) {
        if (!audio_decoder.Open(demuxer.audio_codecpar())) return 1;
        if (!audio_player.Open(&audio_decoder)) return 1;
    }
    AudioClock audio_clock;//音频时钟
    audio_player.SetClock(&audio_clock,demuxer.audio_time_base());

    SyncController sync_controller;//初始化同步控制器
    sync_controller.SetAudioClock(&audio_clock);
    AVRational video_tb = demuxer.video_time_base(); // 获得视频的时间基用来把 pts 换算成秒

    std::ofstream sync_log("sync_log.csv");
    sync_log << "frame_idx,video_pts,audio_clock,diff_ms,action\n";
    //第几帧，视频帧时间戳，音频时钟，相差，采取什么欣慰
    
    Renderer renderer;
    if(!renderer.Open(video_decoder.width(),video_decoder.height(),video_decoder.pixelformat())) return 1;

    
    //启动两个后台线程
    demuxer.Start();//start->run读取packet包并放入packet队列
    video_decoder.Start(&demuxer.video_packet_queue());//demuxer类里的接口，提供packet队列
    audio_decoder.Start(&demuxer.audio_packet_queue());
    audio_player.Start();
    
    //steady_clock 的定义是：now() 返回值单调非递减，不受系统时间调整影响。
    bool running = true;//控制住循环是否关闭
    int rendered = 0;//统计已经渲染到屏幕上的帧
    int drop = 0;//统计丢弃的帧数
    int frame_idx = 0;//记录当前是第几帧

    // 防止音频以及编译了视频还没有开始编译
    while (audio_clock.Getseconds() == 0.0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    //编译器计算
    while(running){
        if (!renderer.HandleEvents()) {//按下q或者esc的时候会返回false
            running = false;
            break;
        }

        auto opt_frame = video_decoder.frame_queue().pop();
        if(!opt_frame){
            std::cout << "End of stream" << std::endl;
            break;
        }

        AVFrame* frame = *opt_frame;

        int64_t raw_pts = (frame->pts != AV_NOPTS_VALUE)? frame->pts : frame->best_effort_timestamp;
        double video_pts_sec =(raw_pts != AV_NOPTS_VALUE)? raw_pts * av_q2d(video_tb):0.0;

        double audio_time = audio_clock.Getseconds();
        double diff_ms = (video_pts_sec - audio_time) * 1000.0;

        SyncDecision daction = sync_controller.Decide(video_pts_sec);
        if(daction.action == SyncAction::kWait){
            auto wait = std::chrono::microseconds(
                static_cast<long>(daction.wait_seconds*1000'000)//秒换成微妙
            );
            std::this_thread::sleep_for(wait);
            renderer.RenderFrame(frame);
            rendered++;
        }
        else if(daction.action == SyncAction::kDrop){
            drop++;
        }
        else if(daction.action == SyncAction::kDisplay){
            renderer.RenderFrame(frame);
            rendered++;
        }
        
        const char* action_str = 
            (daction.action == SyncAction::kWait)  ? "wait" :
            (daction.action == SyncAction::kDrop)  ? "drop" : "display";

        sync_log << frame_idx << "," 
                << video_pts_sec << ","
                << audio_time << ","
                << diff_ms << ","
                << action_str << "\n";

        frame_idx++;

        av_frame_free(&frame);
    }

    std::cout << "Rendered " << rendered << " frames, dropped " << drop << std::endl;
    
    sync_log.close();
    demuxer.Stop();  // 1. 先停上游，它会 Close packet_queue
    audio_decoder.Stop();
    video_decoder.Stop();  // 2. 再停下游，此时 Pop 会返回 nullopt，线程自然退出
    // 之后 demuxer 和 decoder 离开作用域时，析构里的 Stop() 又调一遍，无害
    //如果顺序反了，就没法往下走了，packetqueue里的进程永远无法被唤醒

    
    return 0;
}
