#pragma once
#include <iostream>
#include <string>

extern "C"{
    #include <libavutil/error.h>
}

namespace mp{
    //把ffmpeg的报错记录可视化
    inline std::string FFmpegErrorString(int errnum){
        char buf[AV_ERROR_MAX_STRING_SIZE]={0};//AV_ERROR_MAX_STRING_SIZE是 FFmpeg 库中定义的一个宏
        //用于指定存储错误字符串的缓冲区的最大大小。
        av_strerror(errnum,buf,sizeof(buf));
        return std::string(buf);
    }
    // 检查 FFmpeg 调⽤结果,出错时打印信息
// ⽤法:CheckFFmpeg(avformat_open_input(...), "Failed to open input")
// 返回值:成功为 true,失败为 false
    inline bool CheckFFmpeg(int ret, const std::string& context) {
        if (ret < 0) {
            std::cerr << "[FFmpeg error] " << context
            << ": " << FFmpegErrorString(ret)
            << " (code=" << ret << ")" << std::endl;
        return false;
        }
        return true;
    }
    
}