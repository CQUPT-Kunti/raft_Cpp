#pragma once

#include <chrono>
#include <random>
#include <functional>

typedef std::function<void()> CallbackFunc;

class Timer
{
private:
    std::chrono::steady_clock::time_point expire_time;
    CallbackFunc callback;
    int timeout_ms;

public:
    Timer();
    ~Timer() = default;

    // 设置定时器
    void set(int min_ms, int max_ms, CallbackFunc func);

    // 重置时间
    void reset(int min_ms, int max_ms);

    // 获取过期时间
    std::chrono::steady_clock::time_point getExpireTime() const;

    // 获取回调函数
    CallbackFunc getCallback() const;

    // 比较操作符
    bool operator>(const Timer &other) const;

private:
    int randomBetween(int min, int max);
};
