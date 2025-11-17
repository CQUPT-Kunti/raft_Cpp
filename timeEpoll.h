#pragma once;

#include <sys/epoll.h>
#include "configArgs.h"
#include <shared_mutex>
#include <mutex>
#include <thread>
#include <functional>

typedef std::function<void()> func;

class TimeEpoll
{
public:
    // TimeEpoll(func vote_func, func heart_func);
    TimeEpoll();
    ~TimeEpoll();

    void resetOutTime(TimerArgs &time_arg, int min_ms, int max_ms);
    int randomBetween(int minMs, int maxMs);
    void TimeServiceStart();

private:
    int epoll_fd;
    int vote_fd;
    int heart_fd;
    std::shared_mutex time_mtx;

    std::thread timeEpoll_thread;
    TimerArgs vote_time_arg;
    TimerArgs heart_time_arg;

    // std::function<void()> voteCallback;
    // std::function<void()> heartCallback;
};