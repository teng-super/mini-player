#pragma once
#include "audio_resampler.h"
#include <iostream>
#include "ffmpeg_error.h"

extern "C" {
#include <libavutil/channel_layout.h>
#include <libavutil/mathematics.h>  //av_rescale_rnd()安全计算
#include <libavutil/opt.h>
}

namespace mp {
    bool AudioResampler::Open(int src_sample_rate,AVSampleFormat src_fmt,
        const AVChannelLayout* src_ch_layout,
        int dst_sample_rate){
            src_sample_rate_ = src_sample_rate;
            dst_sample_rate_ = dst_sample_rate;
            //记录原采样率和更改后采样率
            av_channel_layout_default(&dst_ch_layout_,2);//改成双声道布局
            //AV_CHANNEL_LAYOUT_STEREO

            SwrContext* raw = nullptr;
            int ret = swr_alloc_set_opts2(//特有的双指针接码
                &raw,//往那里初始化,这里注意这个set_opts2要求双指针
                //所以要传进来这个而不是直接get
                &dst_ch_layout_,// 输出声道布局
                AV_SAMPLE_FMT_S16,// 输出采样格式
                dst_sample_rate,// 输出采样率
                src_ch_layout,// 输入声道布局
                src_fmt,// 输入采样格式
                src_sample_rate,// 输入采样率
                0,nullptr);

            if(ret<0 || !raw){
                CheckFFmpeg(ret, "swr_alloc_set_opts2");
                return false;
            }
            swr_.reset(raw);

            if (!CheckFFmpeg(swr_init(swr_.get()), "swr_init")) {
                return false;
            }

            std::cout << "[AudioResampler] " << src_sample_rate << "Hz "
              << av_get_sample_fmt_name(src_fmt) << " "
              << src_ch_layout->nb_channels << "ch -> "
              << dst_sample_rate << "Hz S16 2ch" << std::endl;
        return true;
    }
    int AudioResampler::Convert(AVFrame* in_frame,uint8_t* out_buffer,int out_buffer_size){
        if(!swr_ || !in_frame) return -1;
        int max_out_samples = static_cast<int>(
            av_rescale_rnd(//av_rescale_rnd(a, b, c, AV_ROUND_UP)
            //计算a×b/c,向上取整,内部用 int64 防溢出。
            //逻辑是有n个采样时候采样率是A，那采样率是B的时候，有总采样xA=n，xB=target
            //所以target=n*B/A,
                swr_get_delay(swr_.get(), src_sample_rate_) + in_frame->nb_samples,
                dst_sample_rate_,
                src_sample_rate_,
                AV_ROUND_UP
            ));
            int max_out_bytes = max_out_samples * 2 /* S16 */ * 2 /* stereo */;
                if (max_out_bytes > out_buffer_size) {
                std::cerr << "[AudioResampler] out_buffer too small: need "
                << max_out_bytes << " got " << out_buffer_size << std::endl;
                return -1;
            }

            uint8_t* out[1] = { out_buffer };
            int converted = swr_convert(
                swr_.get(),//格式转换上下文
                out,max_out_samples,//输出容器和容器容量
                const_cast<const uint8_t**>(in_frame->extended_data),//in_frame里实际的PCM数据
                in_frame->nb_samples
            );

            if(convered < 0){
                CheckFFmpeg(convered,"swr_convert");
            }

            return converted * 2 /* S16 */ * 2 /* stereo */;
    }
    int AudioResampler::EstimateOutputSize(int in_nb_sample){
        int est_samples = static_cast<int>(av_rescale_rnd(in_nb_samples, 
            dst_sample_rate_, src_sample_rate_,
            AV_ROUND_UP))+256;
            return 2*2*3/2;
    }
}