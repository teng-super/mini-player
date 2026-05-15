#include "bounded_blocking_queue.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
using namespace mp;
using namespace std::chrono_literals;

void TestBasicPushPop() {
BoundedBlockingQueue<int> q(3);
assert(q.push(1));//断言
assert(q.push(2));
assert(q.push(3));
auto a = q.pop();
assert(a.has_value() && *a == 1);
auto b = q.pop();
assert(b.has_value() && *b == 2);
auto c = q.pop();
assert(c.has_value() && *c == 3);
std::cout << "TestBasicPushPop passed\n";
}
//满了会不会阻塞
void TestPushBlocksWhenFull() {
    BoundedBlockingQueue<int> q(2);
    q.push(1);
    q.push(2);
    std::atomic<bool> push_returned{false};
    std::thread producer([&] {
        q.push(3); // 应该阻塞
        push_returned = true;
    });
    std::this_thread::sleep_for(100ms);
    assert(!push_returned.load());
    q.pop();
    producer.join();
    assert(push_returned.load());
    std::cout << "TestPushBlocksWhenFull passed\n";
}

//Close 能不能唤醒阻塞线程
void TestCloseWakesUpWaiters() {
    BoundedBlockingQueue<int> q(1);
    q.push(42);
    std::atomic<bool> push_returned{false};
    std::atomic<bool> push_succeeded{false};
    std::thread producer([&] {
        bool ok = q.push(99);
        push_returned = true;
        push_succeeded = ok;
    });
    std::this_thread::sleep_for(100ms);
    assert(!push_returned.load());
    q.Close();
    producer.join();
    assert(push_returned.load());
    assert(!push_succeeded.load());
    std::cout << "TestCloseWakesUpWaiters passed\n";
}

//测试真实并发下会不会丢数据
void TestConcurrentPushPop() {
    BoundedBlockingQueue<int> q(100);
    constexpr int kCount = 10000;

    std::thread producer([&] {
        for (int i = 0; i < kCount; ++i) {
            q.push(i);
        }
        q.Close();
    });

    int sum = 0;
    while (auto x = q.pop()) {
        sum += *x;
    }

    producer.join();

    int expected = (kCount - 1) * kCount / 2;//高斯公式算总和
    assert(sum == expected);

    std::cout << "TestConcurrentPushPop passed (sum=" << sum << ")\n";
}

int main() {
    TestBasicPushPop();
    TestPushBlocksWhenFull();
    TestCloseWakesUpWaiters();
    TestConcurrentPushPop();

    std::cout << "\nAll tests passed!\n";
    return 0;
}