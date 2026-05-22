#include <iostream>
#include <fstream>
#include <vector>

#include "audio_decoder.h"
#include "audio_resampler.h"
#include "demuxer.h"

using namespace mp;

int main(int argc,char* argv[]){
    if (argc < 2) {
        std::cerr <<"请输入视频" << std::endl;
        return 1;
    }

    Demuxer demuxer;
    if(!demuxer.Open(argv[1]))  return -1;
    if(!demuxer.has_audio()){
        std::cerr<<"No audio get";
        return -1;
    }

    AudioDecoder audio_decoder;
    if(!audio_decoder.Open((demuxer.audio_codecpar())))    return 1;

    AudioResampler sampler;
    if(!sampler.Open(
        audio_decoder.sample_rate(),
        audio_decoder.sample_format(),
        audio_decoder.ch_layout(),
        44100
    ))  return -1;

    std::ofstream pcm_file("output.pcm",std::ios::binary);//不加这个binary会把\n转换成\r\n这种
    if (!pcm_file) {
        std::cerr << "Cannot open output.pcm" << std::endl;
        return 1;
    }
    demuxer.Start();
    audio_decoder.Start(&demuxer.audio_packet_queue());

    std::vector<uint8_t> out_buffer(1024*32);//暂时设定32字节，这个是用来接convert的数据然后写入文件
    int total_bytes = 0;//记录多杀字节

    std::jthread thread_cons([&demuxer](){
        while(auto opt_pkt = demuxer.video_packet_queue().pop()){//用不到stop_token
            AVPacket* packet = *opt_pkt;
            av_packet_free(&packet);
        }
    });

    while(auto opt_frame = audio_decoder.frame_queue().pop()){
        AVFrame* frame = *opt_frame;
        int need = sampler.EstimateOutputSize(frame->nb_samples);//估算好需要的空间
        if(static_cast<int>(out_buffer.size()) < need){//out_buffer.size()单位是size_t所以需要强转
            out_buffer.resize(need);//正式赋值
        }
        int written = sampler.Convert(frame,out_buffer.data(),out_buffer.size());//c接口不认识vector，得传.data()
        if(written > 0){
            pcm_file.write(reinterpret_cast<const char*>(out_buffer.data()), written);
            //write只接受char*类型，reinterpret_cast代表换种方式解释内存，uint8_t和char都是1字节所以没事
            total_bytes+=written;
        }
        av_frame_free(&frame);
    }
    demuxer.Stop();                        
    audio_decoder.Stop();                  

    pcm_file.close();
    std::cout << "Wrote " << total_bytes << " bytes to output.pcm" << std::endl;
    std::cout << "用这个命令行测试: ffplay -ar 44100 -ch_layout stereo -f s16le output.pcm" <<
    std::endl;
    return 0;
}