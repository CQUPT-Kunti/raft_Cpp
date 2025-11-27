#pragma once

#include "timer.h"
#include "timerPool.h"
#include "ThreadPool.h"
#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

// 用于优先队列的比较器
struct TimerCompare
{
    bool operator()(Timer *a, Timer *b) const
    {
        return *a > *b;
    }
};

class TimeManager
{
private:
    std::priority_queue<Timer *, std::vector<Timer *>, TimerCompare> timer_queue;
    std::mutex mtx;
    std::condition_variable cv;
    std::thread worker_thread;
    std::atomic<bool> running;

    ThreadPool *thread_pool;
    TimerPool timer_pool;

public:
    TimeManager(ThreadPool *pool, size_t timer_pool_size = 100);
    ~TimeManager();

    // 添加定时任务
    void addTimer(int min_ms, int max_ms, CallbackFunc callback);

    // 停止管理器
    void stop();

    // 获取统计信息
    size_t getFreeTimerCount();
    size_t getTotalTimerCount();

private:
    void run();
};
