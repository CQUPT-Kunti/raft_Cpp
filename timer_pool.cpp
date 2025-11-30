#include "timer_pool.h"

TimerPool::TimerPool(size_t size) : init_size(size)
{
    for (size_t i = 0; i < size; ++i)
    {
        Timer *timer = new Timer();
        all_timers.push_back(timer);
        free_timers.push(timer);
    }
}

TimerPool::~TimerPool()
{
    for (Timer *timer : all_timers)
    {
        delete timer;
    }
    all_timers.clear();
}

Timer *TimerPool::acquire()
{
    std::lock_guard<std::mutex> lock(mtx);

    if (free_timers.empty())
    {
        // 池子用完了，扩展一倍
        size_t expand_size = all_timers.size();
        for (size_t i = 0; i < expand_size; ++i)
        {
            Timer *timer = new Timer();
            all_timers.push_back(timer);
            free_timers.push(timer);
        }
    }

    Timer *timer = free_timers.front();
    free_timers.pop();
    return timer;
}

void TimerPool::release(Timer *timer)
{
    if (timer == nullptr)
        return;

    std::lock_guard<std::mutex> lock(mtx);
    free_timers.push(timer);
}

size_t TimerPool::getFreeCount()
{
    std::lock_guard<std::mutex> lock(mtx);
    return free_timers.size();
}

size_t TimerPool::getTotalCount()
{
    std::lock_guard<std::mutex> lock(mtx);
    return all_timers.size();
}
