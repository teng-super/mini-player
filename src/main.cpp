#define _CRT_SECURE_NO_WARNINGS
#include<iostream>
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
    if (!decoder.open(demuxer.video_cpar())) return 1;
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
    }
    return 0;
}
