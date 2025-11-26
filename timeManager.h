#pragma once
#include "timer.h"
#include "ThreadPool.h"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <memory>
#include <vector>
#include <chrono>

struct TimerCompare
{
    bool operator()(const std::shared_ptr<Timer> &a, const std::shared_ptr<Timer> &b) const
    {
        return *a > *b;
    }
};

class TimeManager
{
private:
    std::priority_queue<std::shared_ptr<Timer>, std::vector<std::shared_ptr<Timer>>, TimerCompare> queue;
    std::mutex queue_mtx;
    std::condition_variable cv;
    std::thread consume_thread;
    std::atomic<bool> running{true};
    ThreadPool &threadPool;

public:
    bool add_timer(std::shared_ptr<Timer> timer);
    explicit TimeManager(ThreadPool &pool);
    ~TimeManager();
    void run();
    void stop();
};
