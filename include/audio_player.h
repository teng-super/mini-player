#pragma once
#include <SDL2/SDL.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>

#include "audio_decoder.h"
#include "audio_fifo.h"
#include "audio_resampler.h"
#include "audio_clock.h"//音频时钟接入

namespace mp{
    class AudioPlayer{
        public:
            AudioPlayer() = default;
            ~AudioPlayer();
            AudioPlayer(const AudioPlayer&) = delete;
            AudioPlayer& operator=(const AudioPlayer&) = delete;

            // 用 AudioDecoder 的输出参数初始化
            bool Open(AudioDecoder* decoder);

            // 启动 SDL 音频 + 后台 fill 线程(从 decoder 取 frame 重采样写入 fifo)
            void Start();
            
            //暂停时调(让 SDL 停止调回调,后台线程也暂停 fill)
            void SetPaused(bool paused);

            //传入时钟和时间戳
            void SetClock(AudioClock* clock, AVRational time_base);

        private:
            void FillThread(std::stop_token stoken);
            static void SdlAudioCallback(void* userdata, uint8_t* stream, int len);
            //加static是为了去掉成员函数默认自带的this指针好让SDL的api接收

            AudioDecoder* decoder_ = nullptr;//用裸指针表示"我只是借用，不负责释放"
            AudioResampler resampler_;
            AudioFifo fifo_;
            //按值持有，不会被其他地方用到，这样写最方便
            std::atomic<bool> paused_{false};
            std::mutex fifo_mu_;

            SDL_AudioDeviceID device_id_ = 0;
            //SDL 打开音频设备之后会返回一个 ID
            //后续所有操作——暂停、继续、关闭,都靠这个 ID辨识
            SDL_AudioSpec spec_{};//音频参数
            std::jthread fill_thread_;//取帧-从采样-写入fifo

            //----------时钟---------
            AudioClock* clock_ = nullptr; //不持有该时钟，外部创建
            double last_frame_pts_seconds_ = 0.0;//记录上一帧的时间戳
            AVRational time_base_{1, 1};
    };
}