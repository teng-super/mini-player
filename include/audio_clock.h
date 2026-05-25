#pragma once
#include <atomic>

namespace mp{
    class AudioClock
    {
    private:
        std::atomic<double> last_frame_pts_seconds_ {0.0};
        int sample_rate = 0;//采样率
        int bytes_per_frame = 0;//一帧的字节数
    public:
        AudioClock() = default;
        //FillThread 每解一帧就调，把这帧的时间戳存进去
        void Update(double this_current_pts){
            last_frame_pts_seconds_.store(this_current_pts);
        }
        //主循环调，读出当前音频时间
        double Getseconds(){
            return last_frame_pts_seconds_.load();
        }
        void Reset(){
            last_frame_pts_seconds_.store({0.0});
        }
    };
    
}