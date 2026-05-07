#define __STDC_CONSTANT_MACROS
#include <iostream>
extern "C" {
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}
#include <SDL2/SDL.h>
int main() {
// === FFmpeg 版本检查 ===
    std::cout << "===== FFmpeg version check =====" << std::endl;
    std::cout << "libavutil: " << av_version_info() << std::endl;
    std::cout << "libavcodec: "
    << AV_VERSION_MAJOR(avcodec_version()) << "."
    << AV_VERSION_MINOR(avcodec_version()) << "."
    << AV_VERSION_MICRO(avcodec_version()) << std::endl;
    std::cout << "libavformat: "
    << AV_VERSION_MAJOR(avformat_version()) << "."
    << AV_VERSION_MINOR(avformat_version()) << "."
    << AV_VERSION_MICRO(avformat_version()) << std::endl;
    std::cout << "libswscale: "
    << AV_VERSION_MAJOR(swscale_version()) << "."
    << AV_VERSION_MINOR(swscale_version()) << "."
    << AV_VERSION_MICRO(swscale_version()) << std::endl;
// === SDL2 初始化测试 ===
    std::cout << "\n===== SDL2 window test =====" << std::endl;
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;//SDL_GetError是标准的SDL错误检验函数
    return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
    "mini-player toolchain test",
    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
    640, 360,
    SDL_WINDOW_SHOWN
    );

    if (!window) {
    std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
    SDL_Quit();
    return 1;
    }

    std::cout << "Window opened. Will close in 3 seconds..." << std::endl;
    SDL_Delay(3000);
    SDL_DestroyWindow(window);
    SDL_Quit();
    std::cout << "Toolchain OK. You are ready to roll." << std::endl;
    return 0;
}