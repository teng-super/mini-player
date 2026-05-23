#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>
#include "audio_fifo.h"
using namespace mp;

int main(){
    AudioFifo fifo;
    assert(fifo.Open(44100)); // 1 秒 buffer
    // 写入 1000 个 sample (4000 字节)
    std::vector<uint8_t> data(4000);
    for (int i = 0; i < 4000; ++i) data[i] = static_cast<uint8_t>(i & 0xFF);
    int written = fifo.WriteBytes(data.data(), 4000);
    assert(written == 1000);
    assert(fifo.AvailableSamples() == 1000);
    // 读出 400 个 sample (1600 字节)
    std::vector<uint8_t> out(4000);
    int read = fifo.ReadBytes(out.data(), 1600);
    assert(read == 1600);
    assert(fifo.AvailableSamples() == 600);
    // 验证读出的数据和写入的前 1600 字节一致
    assert(std::memcmp(out.data(), data.data(), 1600) == 0);
    // 读出剩余
    int read2 = fifo.ReadBytes(out.data(), 4000);
    assert(read2 == 2400); // 剩 600 sample = 2400 字节
    assert(fifo.AvailableSamples() == 0);
    // 空 FIFO 读出 0
    int read3 = fifo.ReadBytes(out.data(), 1000);
    assert(read3 == 0);
    std::cout << "AudioFifo tests passed!" << std::endl;
    return 0;
}