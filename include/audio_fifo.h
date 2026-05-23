#pragma once
#include <cstdint>
#include <memory> 

extern "C"{
    #include <libavutil/audio_fifo.h>
}

namespace mp{
    struct AVAudioFifoDeleter{
        void operator()(AVAudioFifo* fifo) const{
            if(fifo) av_audio_fifo_free(fifo);
        }
    };

    using AVAudioFifoPtr = std::unique_ptr<AVAudioFifo,AVAudioFifoDeleter>;

    class AudioFifo{
        private:
            AVAudioFifoPtr fifo_;
            static constexpr int kChannels = 2;
            static constexpr int kBytesPerSample = 2;
        public:
            AudioFifo() = default;
            ~AudioFifo() = default;
            AudioFifo(const AudioFifo&) = delete;
            AudioFifo& operator = (const AudioFifo&) = delete;

            //创建 FIFO，分配内存
            bool Open(int initial_capacity_samples);

            //Resampler 吐出来的字节往里写
            int WriteBytes(const uint8_t* data, int bytes);

            //SDL 回调来取数据
            int ReadBytes(uint8_t* out, int requested_bytes);

            //查桶里现在有多少 sample
            int AvailableSamples() const;

            //清空桶（seek 的时候用）
            void Clear();

    };
}