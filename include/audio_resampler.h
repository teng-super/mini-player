#pragma once 
#include "ffmpeg_raii.h"

extern "C"{
    #include <libavutil/channel_layout.h>
    #include <libavutil/samplefmt.h>
}

namespace mp {

class AudioResampler {
    public:
    AudioResampler() = default;
    ~AudioResampler() = default;
    AudioResampler(const AudioResampler&) = delete;
    AudioResampler& operator = (const AudioResampler&) = delete;

    bool Open(int src_sample_rate, AVSampleFormat src_fmt,
        const AVChannelLayout* src_ch_layout,
        int dst_sample_rate);

    // 把一个 AVFrame 重采样,写入调用方的 buffer
    // 返回写入的字节数,失败返回 -1
    int Convert(AVFrame* in_frame, uint8_t* out_buffer, int out_buffer_size);
    // 估算给定输入 sample 数对应的输出字节数(留余量)，这里涉及到的三个变量
    //第一个是解码出来的原始数据，写到Swrcontext里面，然后提前分配好fefo和最大容量往里面塞
    //返回一共写的字节数
    //内存分配（malloc/new）开销比较大，可以让调用方在外部准备好一个
    //可以反复利用的集装箱（复用一块 std::vector<uint8_t>），每次只传指针进去装
    //也就是fifo中央环形缓冲区
    int EstimateOutputSize(int in_nb_samples) const;
    
    int dst_sample_rate() const { return dst_sample_rate_; }
    int dst_channels() const { return 2; } // 固定立体声
    int dst_bytes_per_sample() const { return 2; } // S16

    private:
        SwrContextPtr swr_;
        int src_sample_rate_ = 0;//原采样率
        int dst_sample_rate_ = 0;//更改后的采样率
        AVChannelLayout dst_ch_layout_{};//更改后的声道布局
};

}