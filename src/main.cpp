#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <chrono>
#include <thread>
#include <algorithm>
#include "demuxer.h"
#include "renderer.h"
#include "video_decoder.h"

using namespace mp;
int main(int argc,char* argv[]){//命令行参数和参数具体内容
    if(argc<2){
        std::cerr<<"请传入视频\n";
        return 1;
    }
    Demuxer demuxer;
    if (!demuxer.Open(argv[1])) return 1;//【0】是miniplayer，【1】是assert里面那个MP4的路径
    VideoDecoder decoder;
    if (!decoder.Open(demuxer.video_codecpar())) return 1;
    Renderer renderer;
    if(!renderer.Open(decoder.width(),decoder.height(),decoder.pixelformat())) return 1;

    //2. 计算粗糙的帧间隔（暂时不做精确同步）
    AVRational rf = demuxer.video_frame_rate();//视频帧率
    // 必须 num/den 都 > 0；否则 av_q2d 会得到 0 或负 fps，无法用作帧间隔
    double fps = (rf.num>0 && rf.den>0) ? av_q2d(rf) : 30.0;
    auto frame_delay = std::chrono::microseconds(static_cast<long>(1000000.0/fps));
    //这里不用SDL_Delay了，因为那个只支持毫秒级别，会带来误差
    std::cout << "FPS: " << fps << std::endl;
    //启动两个后台线程
    demuxer.Start();//start->run读取packet包并放入packet队列
    decoder.Start(&demuxer.video_packet_queue());//demuxer类里的接口，提供packet队列
    //steady_clock 的定义是：now() 返回值单调非递减，不受系统时间调整影响。
    auto next_present = std::chrono::steady_clock::now();//第一帧应该呈现的时间点
    bool running = true;//控制住循环是否关闭
    int rendered = 0;//统计已经渲染到屏幕上的帧

    // 主循环每隔 kEventPollInterval 至少回到顶端一次以处理窗口事件；
    // 10ms ≈ 100Hz 是 GUI 事件响应的舒适甜点（人对 ≤50ms 内的延迟基本无感知，
    // 但 >16ms 就开始让人觉得"卡"），同时 CPU 唤醒开销可忽略
    constexpr auto kEventPollInterval = std::chrono::milliseconds(10);

    while(running){
        if (!renderer.HandleEvents()) {//按下q或者esc的时候会返回false
            running = false;
            break;
        }
        //判断下一帧出现时间
        auto now = std::chrono::steady_clock::now();
        if(now < next_present) {
            auto sleep_time = next_present - now;
            if (sleep_time > kEventPollInterval) {
                std::this_thread::sleep_for(kEventPollInterval);
                continue; // 必须 continue 重新检测时间并处理窗口事件！否则会提前渲染
            }
            std::this_thread::sleep_for(sleep_time);
        }

        auto opt_frame = decoder.frame_queue().pop();
        if(!opt_frame){
            std::cout << "End of stream" << std::endl;
            break;
        }
        AVFrame* frame = *opt_frame;
        renderer.RenderFrame(frame);
        av_frame_free(&frame);
        rendered++;
        next_present += frame_delay;//关键！自适应更新时钟周期
        /*next_present 是一个绝对时间点序列，比如 [T₀, T₀+33ms, T₀+66ms, T₀+99ms, ...]。
        每帧渲染完后，目标时间往前推 33ms，不管这一帧实际花了多久。
        渲染如果只花 5ms：sleep 28ms，总耗时 33ms ✓
        渲染如果花了 20ms：sleep 13ms，总耗时 33ms ✓
        渲染如果花了 40ms（超时）：不 sleep，直接进下一帧，目标时间已经跟不上但下一帧会试图追
        这就叫自补偿调度。你的渲染快慢的抖动会被自动吸收，长期平均严格 30fps。
        讲义原话："这种'基于绝对目标时间的等待'是音视频时序的基础模式。
        "——这是 ffplay、mpv、ijkplayer 共有的设计模式。
        ffplay 源码会看到 video_refresh 函数里有几乎完全一样的结构。*/
    }
    std::cout << "Rendered " << rendered << " frames" << std::endl;
    demuxer.Stop();  // 1. 先停上游，它会 Close packet_queue
    decoder.Stop();  // 2. 再停下游，此时 Pop 会返回 nullopt，线程自然退出
    // 之后 demuxer 和 decoder 离开作用域时，析构里的 Stop() 又调一遍，无害
    //如果顺序反了，就没法往下走了，packetqueue里的进程永远无法被唤醒

    /*旧版本：
    Renderer renderer;
    if (!renderer.Open(decoder.width(), decoder.height(), decoder.pixelformat())) return 1;

    AVRational fr = demuxer.video_frame_rate();//获取视频帧率
    double fps = (fr.num && fr.den)? av_q2d(fr) : 30.0;//判断分子和分母不为0，den是分母
    Uint32 frame_delay = static_cast<Uint32> (1000/fps);//一帧停留多少毫秒
    auto packet = MakePacket();
    auto frame = MakeFrame();
    bool eof_reached = false;//读到末尾了
    bool running = true;
    while(running){
        if(!renderer.HandleEvents()) break;
        if(!eof_reached){
            if(demuxer.ReadPacket(packet.get())){
                decoder.SendPacket(packet.get());
                av_packet_unref(packet.get());
            }
            else{
                decoder.SendPacket(nullptr); // flush
                eof_reached = true;//已到达最深处，没文件了
            }
        }
        int ret = decoder.ReceiveFrame(frame.get());
        if(ret == 1){
            //根据我在vd里面的定义，返回1时真正的frame为0，意思是刚好吃下
            renderer.RenderFrame(frame.get());
            SDL_Delay(frame_delay);//随后音视频同步
        }
        else if (ret == -1) {
            running = false;//AVERROR_EOF
        }
        //为0时说明返回的是EAGAIN，继续循环
    }*///旧版流程
    return 0;
}
