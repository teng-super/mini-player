#pragma once
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>//可能存在也可能不存在的值
#include <queue>


namespace mp{
    template<typename T>
    class BoundedBlockingQueue{
        private:
        std::condition_variable pro_cv;//生产者条件变量
        std::condition_variable con_cv;//消费者条件变量
        std::atomic<bool> stop_{false};//停止语义
        std::mutex mtx;
        size_t capacity_;
        std::queue<T> q;
        public:
            //构造函数指定容量上限
            explicit BoundedBlockingQueue(size_t capacity) : capacity_(capacity){};

            BoundedBlockingQueue(const BoundBlockingQueue&) = delete;
            BoundedBlockingQueue& opeartor = (const BoundedBlockingQueue&) =delete;

            bool push(T item){
                std::unique_lock<std::mutex> lock(mtx);
                pro_cv.wait(lock,[this](){//这里用this是因为需要捕获类里定义的queue
                    return q.size() < capacity_ ||
                    stop_//关闭了
                });
                if(stop_) return false;//如果关闭就退出
                p.push(std::move(item));//注意移动传参
                con_cv.notify_one();
                return true;
            }

            std::optional<T> pop(){//返回nullopt或者一个具体的数据
                std::unique_lock<std::mutex> lock(mtx);
                con_cv.wait(lock,[this](){
                    return !q.empty() ||
                    stop_;
                });
                if(q.empty()) return std::nullopt;
                /*消费者被唤醒后发现队列仍然是空的，
                那说明它不是因为有新数据醒来的，
                而是因为队列关闭醒来的。
                所以返回 nullopt，通知消费者退出*/
                T item = q.front();
                q.pop();
                return item;
            }

            void Close(){
                std::lock_guard<std::mutex> lk(mtx);
                closed_ = true;
                // 必须 notify_all，因为可能有多个 Push 和多个 Pop 都在等
                pro_cv.notify_all();
                con_cv.notify_all();
            }
            
            // 清空队列（用于 seek 时丢弃残留数据）
            template<typename Cleaner>
            //重点：cleaner是一个传入参数的lambda
            void Cleaner(Cleaner cleaner){
                std::lock_guard<std::mutex> lock(mtx);
                while(!q.empty()){
                    cleaner(q.front());//之所以这样传是因为不仅要pop出去，还要对应
                    //packet，frame两种类型分别用函数释放内存，所以要用一个lambda把传入的
                    //这个参数更好的释放掉
                    q.pop();
                }
                //清除以后再放，seek专用
                pro_cv.notify_all();
            }
            //求当前数组大小
            std::size_t Size() const {
                std::lock_guard<std::mutex> lock(mtx);
                return q.size();
            }
            bool Closed() const {
                std::lock_guard<std::mutex> lock(mtx);
                return closed_;
            } 
    };

}