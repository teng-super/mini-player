#pragma once
#include <atomic>

namespace mp{
    class SeekController{
        private:
            std::atomic<bool> pending = {false};
            std::atomic<double> target_seconds_ = {0.0};

        public:
            SeekController() = default;
            //主线程要求seek：
            void Request(double target_seconds){
                target_seconds_.store(target_seconds);
                pending.store(true);
            }
            //demuxer线程收到要seek（如果有）
            //返回值:如果有 pending seek,target_seconds 填入并 pending 置 false,返回 true
            bool ConsumeIfPending(double& target_seconds){
                if(!pending.load()) return false;
                target_seconds = target_seconds_.load();
                pending.store(false);//已经完成了操作，就可以把标志换回来了
                return true;
            }
            // 检查是否有 pending
            bool HasPending() const {
                return pending;
            }
    };
}