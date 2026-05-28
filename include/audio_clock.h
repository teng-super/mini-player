#pragma once
#include <chrono>
#include <mutex>

namespace mp{
    class AudioClock
    {
    private:
        std::mutex mu_;
        double last_frame_pts_seconds_ {0.0};
        std::chrono::steady_clock::time_point last_update_time_;
        int sample_rate = 0;//采样率
        int bytes_per_frame = 0;//一帧的字节数
    public:
        AudioClock() = default;
        //FillThread 每解一帧就调，把这帧的时间戳存进去
        void Update(double this_current_pts){
            std::lock_guard<std::mutex> lock(mu_);
            last_frame_pts_seconds_ = this_current_pts;
            last_update_time_ = std::chrono::steady_clock::now();
        }
        //主循环调，读出当前音频时间
        double Getseconds(){
            std::lock_guard<std::mutex> lock(mu_);
            if (last_update_time_ == std::chrono::steady_clock::time_point{})
                return last_frame_pts_seconds_;   // 还没 Update 过，直接返回原始值
            auto elapsed = std::chrono::steady_clock::now() - last_update_time_;
            double elapsed_sec = std::chrono::duration<double>(elapsed).count();
            return last_frame_pts_seconds_ + elapsed_sec;
        }
        void Reset(){
            std::lock_guard<std::mutex> lock(mu_);
            last_frame_pts_seconds_ = {0.0};
            last_update_time_ = {};
        }
    };
    
}