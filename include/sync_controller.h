#pragma once
#include <algorithm>
#include "audio_clock.h"

namespace mp{
    enum class SyncAction{
        kDisplay,//显示
        kWait,//等一段时间在显示（一个返回值）
        kDrop,//丢帧
    };

    struct SyncDecision{
        SyncAction action;
        double wait_seconds = 0.0; 
        //kWait时使用
    };

    class SyncController{
        public:
            SyncController() = default;
            void SetAudioClock(AudioClock* audio_clock){
                audio_clock_ = audio_clock;
            }

            SyncDecision Decide(double viddo_pet_seconds){
                if(!audio_clock_) {
                    return {SyncAction::kDisplay,0.0};//无音频直接显示
                }
                double audio_time = audio_clock_->Getseconds();
                double diff = viddo_pet_seconds - audio_time;
                diff_ = diff;
                if(diff > sync_threshold_seconds_){
                    return {SyncAction::kWait,std::min(diff,1.0)};
                }
                else if(diff > drop_threshold_seconds_){
                    return {SyncAction::kDrop,{0.0}};
                }
                else return {SyncAction::kDisplay,{0.0}};
            }

            double last_diff() const { return diff_;}

        private:
            AudioClock* audio_clock_ = nullptr;//不占有只借用
            double sync_threshold_seconds_ = 0.04;//等待阈值 40ms，视频超前超过这个值就等。
            double drop_threshold_seconds_ = 0.10;//丢帧阈值 100ms，视频落后超过这个值就丢。
            double diff_;
    };
}