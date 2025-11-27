#include "timer.h"

Timer::Timer() : timeout_ms(0) {}

void Timer::set(int min_ms, int max_ms, CallbackFunc func)
{
    callback = func;
    reset(min_ms, max_ms);
}

void Timer::reset(int min_ms, int max_ms)
{
    timeout_ms = randomBetween(min_ms, max_ms);
    expire_time = std::chrono::steady_clock::now() +
                  std::chrono::milliseconds(timeout_ms);
}

int Timer::randomBetween(int min, int max)
{
    static thread_local std::mt19937 gen{std::random_device{}()};
    std::uniform_int_distribution<> dist(min, max);
    return dist(gen);
}

std::chrono::steady_clock::time_point Timer::getExpireTime() const
{
    return expire_time;
}

CallbackFunc Timer::getCallback() const
{
    return callback;
}

bool Timer::operator>(const Timer &other) const
{
    return expire_time > other.expire_time;
}
