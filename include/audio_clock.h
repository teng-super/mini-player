#pragma once
#include <chrono>
#include <mutex>
#include <algorithm>

namespace mp{
    class AudioClock
    {
    private:
        std::mutex mu_;
        double last_frame_pts_seconds_ {0.0};
        std::chrono::steady_clock::time_point last_update_time_;
        // 外推的"天花板"：已经塞进 FIFO 的最新一帧音频的结束时间戳。
        // 时钟再怎么外推也不能超过它——超过就说明在给"还没解出来的音频"记账（underrun/暂停空转）。
        double clock_ceiling_ {0.0};

    public:
        AudioClock() = default;
        //FillThread 每解一帧就调，把这帧的时间戳存进去
        void Update(double this_current_pts,double buffer_until){
            std::lock_guard<std::mutex> lock(mu_);
            last_frame_pts_seconds_ = this_current_pts;
            last_update_time_ = std::chrono::steady_clock::now();
            clock_ceiling_ = buffer_until;
        }
        //主循环调，读出当前音频时间
        double Getseconds(){
            std::lock_guard<std::mutex> lock(mu_);
            if (last_update_time_ == std::chrono::steady_clock::time_point{})
                return last_frame_pts_seconds_;   // 还没 Update 过，直接返回原始值
            auto elapsed = std::chrono::steady_clock::now() - last_update_time_;
            double elapsed_sec = std::chrono::duration<double>(elapsed).count();
            return std::min(last_frame_pts_seconds_ + elapsed_sec , clock_ceiling_);
        }
        void Reset(double target = 0.0){//如果一开始没有赋值，就赋值成0；
            std::lock_guard<std::mutex> lock(mu_);
            last_frame_pts_seconds_ = target;
            last_update_time_ = std::chrono::steady_clock::now();//现在的时间
            clock_ceiling_ = target;
        }
    };
    
}