#pragma once
#include <chrono>
#include <random>
#include <functional>

#define func std::function<void()>

// 定时器载体
class Timer
{
private:
    std::chrono::steady_clock::time_point queue_outTime_index;
    int out_times;
    func callBack;

public:
    Timer(int min_s, int max_s, func func_);
    ~Timer() = default;

    void reset_time(int min_s, int max_s);
    int randomBetween(int minMs, int maxMs);
    std::chrono::steady_clock::time_point getTime() const;
    func getCallback() const { return callBack; }

    bool operator>(const Timer &other) const
    {
        return queue_outTime_index > other.queue_outTime_index;
    }
};
