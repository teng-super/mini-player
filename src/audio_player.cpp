#include "audio_player.h"
#include <cstring>
#include <iostream>
#include <algorithm>

namespace mp{
    AudioPlayer::~AudioPlayer(){
        fill_thread_.request_stop();
        decoder_->frame_queue().Close();//防止卡在pop的死锁！
        if (device_id_ != 0) {
            SDL_CloseAudioDevice(device_id_);//关闭声卡设备
            device_id_ = 0;
        }
        if (fill_thread_.joinable()) fill_thread_.join();
    }
    bool AudioPlayer::Open(AudioDecoder* decoder){
        if (!decoder) return false;
        decoder_=decoder;

        if(SDL_InitSubSystem(SDL_INIT_AUDIO) < 0){//初始化音频子系统
            std::cerr << "SDL_InitSubSystem AUDIO: " << SDL_GetError() << std::endl;
            return false;
        }

        //配置目标参数SDL_AudioSpec
        SDL_AudioSpec want_spec = {};
        want_spec.freq = 44100;//目标采样率
        want_spec.format = AUDIO_S16SYS;//采样格式
        want_spec.channels = 2;//声道数，立体声stereo
        want_spec.samples = 1024;//采样点数
        want_spec.callback = &AudioPlayer::SdlAudioCallback;
        want_spec.userdata = this;//传给回调的用户数据，下面回调要用

        SDL_AudioSpec ob_spec = {};
        device_id_ = SDL_OpenAudioDevice(//这个api是打开声卡，建立一条音频输出通道的
            nullptr,//选哪个音频设备，nullpter表示默认
            0,//表示输出（播放），1是输入
            &want_spec,// 想要的参数，SDL 拿去尝试匹配。
            &ob_spec,
            0//允许修改的参数类型。0表示都不能改
        );
        if (device_id_ == 0) {
            std::cerr << "SDL_OpenAudioDevice: " << SDL_GetError() << std::endl;
            return false;
        }
        spec_ = ob_spec;
        std::cout << "[AudioPlayer] SDL opened: freq=" << spec_.freq
        << " format=" << spec_.format
        << " channels=" << static_cast<int>(spec_.channels)//这个原本是uint8
        << " samples=" << spec_.samples << std::endl;
        //初始化重采样Resampler
        if (!resampler_.Open(
            decoder_->sample_rate(),
            decoder_->sample_format(),
            decoder_->ch_layout(),
            44100)) {
                return false;
        }

        if (!fifo_.Open(44100)) return false;
        return true;
    }

        void AudioPlayer::Start() {
            // 启动 fill 线程
            fill_thread_ = std::jthread([this](std::stop_token st) {
                FillThread(st); // 启动 SDL 音频(开始调回调)
            });
            SDL_PauseAudioDevice(device_id_, 0); // 0代表开始，意思是让初始化好的音频设备开始播放
            std::cout << "[AudioPlayer] started" << std::endl;
        }

        void AudioPlayer::SetPaused(bool paused) {
            paused_ = paused;
            if (device_id_ != 0) {
                SDL_PauseAudioDevice(device_id_, paused ? 1 : 0);
            }
        }

        void AudioPlayer::SdlAudioCallback(void* userdata, uint8_t* stream, int len){
            auto* self = static_cast<AudioPlayer*>(userdata);//把传进来的指针换成可以访问的类型
            //看上面的spec！把一个 AudioPlayer* 伪装成 void* 穿越了 SDL 的边界，
            //然后在另一边脱下伪装变回 AudioPlayer*
            if (self->paused_.load()) {//为1说明现在是不播放的时间，传0可以金鹰
                std::memset(stream, 0, len);
                return;
            }
            int read = 0;//记录拿了多少字节
            {
                std::lock_guard<std::mutex> lock(self->fifo_mu_);//上锁
                read = self->fifo_.ReadBytes(stream, len);
            }
            if (read < len) {
                std::memset(stream + read, 0, len - read);
                //避免噪音，不够的地方传0，也就是静音 
            }
        }

        void AudioPlayer::FillThread(std::stop_token stoken){
            std::vector<uint8_t> tmp_buffer;//这个用来重采样
            while(!stoken.stop_requested()){
                if(paused_.load()){
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    continue;
                }
                int anv;
                {
                    std::lock_guard<std::mutex> lock(fifo_mu_);
                    //看看fifo快满了没，快了就做一个背压
                    anv = fifo_.AvailableSamples();
                }
                if(anv>22050){//采样率是44100，到这个值说明过半了，歇一下
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
                auto opt_frame = decoder_->frame_queue().pop();
                if(!opt_frame){
                    // 一般是EOF了
                    std::cout << "[AudioPlayer::FillThread] decoder EOF" << std::endl;
                    break;
                }
                AVFrame* frame = *opt_frame;
                //算出这一帧的时间戳
                if(clock_&&frame->pts != AV_NOPTS_VALUE){
                    double this_second = frame->pts * av_q2d(time_base_);
                    last_frame_pts_seconds_ = this_second;//更新audioplayer的时间戳
                    // 减去 FIFO 里积压的时长，得到声卡真正在播的位置
                    int backlog;
                    {
                        std::lock_guard<std::mutex> lock(fifo_mu_);
                        backlog = fifo_.AvailableSamples();
                    }
                    double fifo_lag = backlog / 44100.0;
                    clock_->Update(this_second - fifo_lag);//一定要减去在fifo里积压的量，不然会有很大（0.5）秒的延迟
                    //是由于背压导致的,导致你的时钟记录的当前时间和实际播放的时间不一样！！！
                }
                //开始重采样
                // 根据这帧的采样点数，估算输出需要多少字节
                int need_bytes = resampler_.EstimateOutputSize(frame->nb_samples);

                if (static_cast<int>(tmp_buffer.size()) < need_bytes) {
                    tmp_buffer.resize(need_bytes);
                }

                int written = resampler_.Convert(frame,tmp_buffer.data(),
                tmp_buffer.size());

                av_frame_free(&frame);

                if(written > 0){
                    std::lock_guard<std::mutex> lock(fifo_mu_);
                    fifo_.WriteBytes(tmp_buffer.data(), written);
                }
            }
            std::cout << "[AudioPlayer::FillThread] exiting" << std::endl;
        }
        void AudioPlayer::SetClock(AudioClock* clock, AVRational time_base) {
            clock_ = clock;
            time_base_ = time_base;
        }

}