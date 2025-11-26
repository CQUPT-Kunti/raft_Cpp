#include "timer.h"

Timer::Timer(int min_s, int max_s, func func_)
{
    callBack = func_;
    reset_time(min_s, max_s);
}

int Timer::randomBetween(int minMs, int maxMs)
{
    static thread_local std::mt19937 gen{std::random_device{}()};
    std::uniform_int_distribution<> dist(minMs, maxMs);
    return dist(gen);
}

void Timer::reset_time(int min_s, int max_s)
{
    out_times = randomBetween(min_s, max_s);
    queue_outTime_index = std::chrono::steady_clock::now() + std::chrono::milliseconds(out_times);
}

std::chrono::steady_clock::time_point Timer::getTime() const
{
    return queue_outTime_index;
}
