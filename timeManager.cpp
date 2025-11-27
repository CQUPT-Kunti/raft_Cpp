#include "timeManager.h"
#include <iostream>

TimeManager::TimeManager(ThreadPool *pool, size_t timer_pool_size)
    : running(true), thread_pool(pool), timer_pool(timer_pool_size)
{

    worker_thread = std::thread(&TimeManager::run, this);
}

TimeManager::~TimeManager()
{
    stop();
}

void TimeManager::addTimer(int min_ms, int max_ms, CallbackFunc callback)
{
    Timer *timer = timer_pool.acquire();
    timer->set(min_ms, max_ms, callback);

    {
        std::lock_guard<std::mutex> lock(mtx);
        timer_queue.push(timer);
    }
    cv.notify_one();
}

void TimeManager::run()
{
    while (running)
    {
        std::unique_lock<std::mutex> lock(mtx);

        if (timer_queue.empty())
        {
            cv.wait(lock, [this]()
                    { return !running || !timer_queue.empty(); });

            if (!running)
            {
                return;
            }
        }

        Timer *timer = timer_queue.top();
        auto now = std::chrono::steady_clock::now();

        if (timer->getExpireTime() <= now)
        {
            timer_queue.pop();

            CallbackFunc callback = timer->getCallback();
            timer_pool.release(timer); // 归还到池中

            lock.unlock();

            // 提交到线程池执行
            thread_pool->submit(callback);
        }
        else
        {
            auto wait_time = timer->getExpireTime() - now;
            cv.wait_for(lock, wait_time, [this]()
                        { return !running; });
        }
    }
}

void TimeManager::stop()
{
    running = false;
    cv.notify_all();

    if (worker_thread.joinable())
    {
        worker_thread.join();
    }
}

size_t TimeManager::getFreeTimerCount()
{
    return timer_pool.getFreeCount();
}

size_t TimeManager::getTotalTimerCount()
{
    return timer_pool.getTotalCount();
}
