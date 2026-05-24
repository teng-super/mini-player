# ===================================================================
# mini-player CMakeLists.txt
# ===================================================================
# 指定 CMake 最低版本要求
# 3.16 是 Ubuntu 20.04+ 默认带的版本，⾜够现代
cmake_minimum_required(VERSION 3.16)
# 定义项⽬名称、版本、使⽤的语⾔
project(mini-player VERSION 0.1.0 LANGUAGES CXX)
# 指定 C++ 标准为 C++20，且必须⽀持
# CXX_STANDARD_REQUIRED ON 表⽰如果编译器不⽀持就报错⽽不是降级
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # 关闭 GCC 扩展，保持代码可移植
# 默认使⽤ Debug 模式（如果没指定的话）
# Debug 会带 -g 调试信息且不优化，适合开发阶段
if(NOT CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE Debug)
endif()
# 输出当前构建模式给开发者看
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
# === 编译警告设置 ===
# 这些警告会在编译期帮你抓出⼤量潜在 bug
# -Wall -Wextra 是 GCC/Clang 的标准严格模式
# -Wpedantic 进⼀步严格遵守标准
add_compile_options(-Wall -Wextra -Wpedantic)
# === 查找 FFmpeg 各个库 ===
# 通过 pkg-config 找到 FFmpeg 的头⽂件和库⽂件位置
# 这是最推荐的⽅式（前提是系统装了 -dev 包并且 .pc ⽂件存在）
set(ENV{PKG_CONFIG_PATH} "/usr/local/ffmpeg-8.1.1/lib/pkgconfig")
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
libavformat
libavcodec
libavutil
libswscale
libswresample
)
# === 查找 SDL2 ===
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
# === 定义可执⾏⽂件 ===
add_executable(mini-player
src/main.cpp
src/demuxer.cpp
src/video_decoder.cpp
src/audio_decoder.cpp
src/renderer.cpp
src/audio_resampler.cpp
src/audio_fifo.cpp
src/audio_player.cpp
include/ffmpeg_error.h
include/ffmpeg_raii.h
include/demuxer.h
include/video_decoder.h
include/audio_decoder.h
include/renderer.h
include/audio_resampler.h
include/audio_fifo.h
include/audio_player.h
)
# === 头⽂件搜索路径 ===
# include ⽬录是放⾃⼰写的头⽂件的地⽅
target_include_directories(mini-player PRIVATE
${CMAKE_SOURCE_DIR}/include
)
# === 链接库 ===
# ⽤ PkgConfig::FFMPEG 这种现代写法
# 它会⾃动包含 include 路径、定义宏、链接库
target_link_libraries(mini-player PRIVATE
PkgConfig::FFMPEG
PkgConfig::SDL2
)
# === 显⽰⼀些有⽤的诊断信息 ===
message(STATUS "FFmpeg include dirs: ${FFMPEG_INCLUDE_DIRS}")
message(STATUS "FFmpeg libraries: ${FFMPEG_LIBRARIES}")
message(STATUS "SDL2 version: ${SDL2_VERSION}")

# === 测试文件添加 ===
option(BUILD_TESTS "Build test programs" OFF)##添加BUILD_TESTS类型默认值为OFF，中间是注释
if(BUILD_TESTS)
    # ------------这个是有节队列的测试文件
    add_executable(
        test_bounded_blocking_queue
        tests/test_bounded_blocking_queue.cpp
    )
    target_include_directories(test_bounded_blocking_queue PRIVATE
        ${CMAKE_SOURCE_DIR}/include##SOURCE_DIR表示
    )
    find_package(Threads REQUIRED)
    target_link_libraries(test_bounded_blocking_queue PRIVATE Threads::Threads)
    # ------------------测试fifo是否正常---------------
    add_executable(test_audio_fifo
        tests/test_audio_fifo.cpp
        src/audio_fifo.cpp
    )
    target_include_directories(test_audio_fifo PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    target_link_libraries(test_audio_fifo PRIVATE PkgConfig::FFMPEG)
    # ------------------ 新增 test_audio_resampler ------------------
    add_executable(test_audio_resampler
        tests/test_audio_resampler.cpp
        src/demuxer.cpp
        src/audio_decoder.cpp
        src/audio_resampler.cpp
    )
    target_include_directories(test_audio_resampler PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    # 因为用到了 FFmpeg 和线程
    target_link_libraries(test_audio_resampler PRIVATE 
        PkgConfig::FFMPEG
        Threads::Threads
    )
endif()

# === Sanitizers 动态检查工具 (建议在 Debug 时开启) ===
# 开启方法: cmake -B build -DSANITIZE_THREAD=ON 
# (注意：ThreadSanitizer 和 AddressSanitizer 通常不能同时开启)

# 1. 开启 ThreadSanitizer: 用于检查多线程的数据竞争(Data Race)和死锁
option(SANITIZE_THREAD "Enable ThreadSanitizer" OFF)

# 2. 开启 AddressSanitizer: 用于检查内存泄漏、越界访问、使用已释放内存等问题
option(SANITIZE_ADDRESS "Enable AddressSanitizer" OFF)

if(SANITIZE_THREAD)
    message(STATUS "ThreadSanitizer enabled")
    # 注入线程检查代码，-g保留行号，-O1基础优化防止太卡
    add_compile_options(-fsanitize=thread -g -O1)
    add_link_options(-fsanitize=thread)
endif()

if(SANITIZE_ADDRESS)
    message(STATUS "AddressSanitizer enabled")
    # 注入内存检查代码，-fno-omit-frame-pointer让报错时的调用栈更清晰
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g -O1)
    add_link_options(-fsanitize=address)
endif()