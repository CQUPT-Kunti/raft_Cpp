#pragma once
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <map>
#include <functional>
#include <random>
#include <iostream>

class TimeEvent
{
public:
    using Callback = std::function<void()>;

    TimeEvent();
    ~TimeEvent();

    int addRandomTimer(int min_ms, int max_ms, const Callback &cb);
    void addFd(int fd, const Callback &cb);
    void handleEvent(int fd);
    void pollOnce(); // 不阻塞整个程序

private:
    int epoll_fd;
    std::map<int, Callback> callbacks_;

    itimerspec make_random_time(int min_ms, int max_ms);
};
