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
src/renderer.cpp
include/ffmpeg_error.h
include/ffmpeg_raii.h
include/demuxer.h
include/video_decoder.h
include/renderer.h
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