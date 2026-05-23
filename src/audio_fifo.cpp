#include <iostream>
#include <algorithm> 
#include "audio_fifo.h"

namespace mp{
    bool AudioFifo::Open(int initial_capacity_samples){//初始容量
        AVAudioFifo* raw = av_audio_fifo_alloc(
            AV_SAMPLE_FMT_S16,//装什么格式的数据？
            kChannels,//几个声道？
            initial_capacity_samples//具体多大？
        );
        if(!raw) {
            std::cerr << "av_audio_fifo_alloc failed" << std::endl;
            return false;
        }
        fifo_.reset(raw);
        return true;
    }

    int AudioFifo::WriteBytes(const uint8_t* data,int byte){//往fifo里面写数据
        if(!fifo_) return -1;
        int samples = byte/(kChannels * kBytesPerSample);
        if(samples<=0) return 0;
        void* p[1] = {const_cast<uint8_t*>(data)};//把const去了，
        //AVAudioFifo需要支持两种格式，所以需要一个双指针格式，这里可以表示为指针数组void*p[]
        //但是这样就需要去掉传入时候的const了
        int written = av_audio_fifo_write(fifo_.get(),p,samples);
        return written;
    }

    int AudioFifo::ReadBytes(uint8_t* out, int requested_bytes){//给回调函数喂数据
        if(!fifo_) return 0;
        int samples = requested_bytes/(kChannels * kBytesPerSample);
        int available = av_audio_fifo_size(fifo_.get());//fifo剩余的samples数量
        int to_read = std::min(available,samples);
        if(to_read<=0) return 0;
        void* p[1] = { out };
        int read = av_audio_fifo_read(fifo_.get(),p,to_read);//从哪里读，读那些数据（数组指向），读多少
        return read * kChannels * kBytesPerSample;
    }

    int AudioFifo::AvailableSamples() const {
        if (!fifo_) return 0;
            return av_audio_fifo_size(fifo_.get());//剩余sample数量
    }

    void AudioFifo::Clear() {
        if (fifo_) {
            av_audio_fifo_drain(fifo_.get(), av_audio_fifo_size(fifo_.get()));
        }
    }
}