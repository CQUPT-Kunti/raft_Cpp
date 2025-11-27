#pragma once

#include "timer.h"
#include <vector>
#include <queue>
#include <mutex>

class TimerPool
{
private:
    std::vector<Timer *> all_timers; // 所有Timer对象
    std::queue<Timer *> free_timers; // 空闲的Timer
    std::mutex mtx;
    size_t init_size;

public:
    explicit TimerPool(size_t size = 100);
    ~TimerPool();

    // 获取一个Timer
    Timer *acquire();

    // 归还一个Timer
    void release(Timer *timer);

    // 获取统计信息
    size_t getFreeCount();
    size_t getTotalCount();
};
