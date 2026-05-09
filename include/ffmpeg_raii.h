#pragma once //防止同一个头文件被重复包含
#include <memory>

extern "C"{
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/frame.h>
    #include <libswscale/swscale.h>
}

namespace mp { // mini-player命名空间

    // ===== AVFormatContext 包装 =====
    struct AVFormatContextDeleter {
        void operator()(AVFormatContext* ctx) const {
            if (ctx) {
                avformat_close_input(&ctx);
            }
        }
    };
    using AVFormatContextPtr = std::unique_ptr<AVFormatContext, AVFormatContextDeleter>;
    //格式: unique_ptr<删除元素类型，删除器类型>
    // ===== AVCodecContext 包装 =====
    struct AVCodecContextDeleter {
        void operator()(AVCodecContext* ctx) const {
            if (ctx) {
                avcodec_free_context(&ctx);
            }
        }
    };
    using AVCodecContextPtr = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;

    // ===== AVPacket 包装 =====
    struct AVPacketDeleter {
        void operator()(AVPacket* pkt) const {
            if (pkt) {
                av_packet_free(&pkt);
            }
        }
    };
    using AVPacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;

    // ===== AVFrame 包装 =====
    struct AVFrameDeleter {
        void operator()(AVFrame* frame) const {
            if (frame) {
                av_frame_free(&frame);
            }
        }
    };
    using AVFramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;

    // ==== SwsContext包装 ====
    // 注意: sws_freeContext 接受单指针, 和上面几个不一样
    struct SwsContextDeleter {
        void operator()(SwsContext* ctx) const {
            if (ctx) {
                sws_freeContext(ctx);
            }
        }
    };

    using SwsContextPtr = std::unique_ptr<SwsContext, SwsContextDeleter>;

    // ==== 关于packet和frame的内联函数 ====
    //inline = 这个函数可以安全地写在头文件里

    inline AVPacketPtr MakePacket(){
        return AVPacketPtr(av_packet_alloc());
    }
    
    inline AVFramePtr MakeFrame(){
        return AVFramePtr(av_frame_alloc());
    }
}